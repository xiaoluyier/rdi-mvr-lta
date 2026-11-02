from __future__ import annotations

from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import subprocess
from typing import Iterable, Mapping, Protocol

from .controller import XiWangController
from .core import BatchObservation


_FIELD = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)")


def parse_sfe_level_log(
    text: str, *, batch_id: str, expected_plan: Mapping[int, int],
    reference_population: int = 24,
) -> list[BatchObservation]:
    return _parse_sfe_level_lines(
        text.splitlines(), batch_id=batch_id, expected_plan=expected_plan,
        reference_population=reference_population,
    )


def _parse_sfe_level_lines(
    lines: Iterable[str], *, batch_id: str, expected_plan: Mapping[int, int],
    reference_population: int = 24,
) -> list[BatchObservation]:
    if not batch_id:
        raise ValueError("batch_id is required")
    plan = {int(level): int(population) for level, population in expected_plan.items()}
    if reference_population <= 0:
        raise ValueError("reference_population must be positive")
    rows: list[BatchObservation] = []
    cluster = 0
    previous_step: int | None = None
    for line in lines:
        if "SFE_LEVEL " not in line:
            continue
        fields = dict(_FIELD.findall(line))
        required = {"sweep_step", "level", "effort", "successes"}
        if not required.issubset(fields):
            raise ValueError("incomplete SFE_LEVEL record")
        step = int(fields["sweep_step"])
        level = int(fields["level"])
        effort = int(fields["effort"])
        successes = int(fields["successes"])
        expected_population = plan.get(level, reference_population)
        if effort != expected_population:
            raise ValueError(
                f"executed population mismatch at level {level}: "
                f"{effort} != {expected_population}"
            )
        if previous_step is None or step <= previous_step:
            cluster += 1
        previous_step = step
        rows.append(BatchObservation(
            batch_id=batch_id,
            cluster_id=f"{batch_id}:r{cluster}",
            level=level,
            trials=effort,
            successes=successes,
            population=effort,
        ))
    if not rows:
        raise ValueError("no SFE_LEVEL observations found")
    return rows


class LearningAdapter(Protocol):
    def run_batch(
        self, batch_id: str, plan: Mapping[int, int],
    ) -> Iterable[BatchObservation]: ...


_FORBIDDEN_CONTROLS = {
    "BATS_KBRANCH",
    "BATS_TVCP",
    "BATS_TVCP_PLAN",
    "BATS_ADAPTIVE_LEVEL_POPULATION",
    "BATS_LEVEL_POPULATION",
    "BATS_XIWANG_PLAN",
}


class FigSubprocessAdapter:
    def __init__(
        self,
        *,
        command: Iterable[str],
        workdir: Path,
        base_env: Mapping[str, str],
        log_dir: Path,
        timeout_seconds: int,
    ) -> None:
        self.command = tuple(str(item) for item in command)
        if not self.command:
            raise ValueError("FIG command must be nonempty")
        self.workdir = Path(workdir)
        self.log_dir = Path(log_dir)
        self.timeout_seconds = int(timeout_seconds)
        if self.timeout_seconds <= 0:
            raise ValueError("timeout_seconds must be positive")
        forbidden = sorted(_FORBIDDEN_CONTROLS.intersection(base_env))
        if forbidden:
            raise ValueError("forbidden FIG control in clean XiWang adapter: " + ", ".join(forbidden))
        self.base_env = {str(key): str(value) for key, value in base_env.items()}

    def run_batch(
        self, batch_id: str, plan: Mapping[int, int],
    ) -> list[BatchObservation]:
        if not batch_id or not plan:
            raise ValueError("batch_id and a complete plan are required")
        sparse_plan = {
            int(level): int(population)
            for level, population in plan.items()
            if int(population) != 24
        }
        plan_text = ",".join(
            f"{level}:{sparse_plan[level]}" for level in sorted(sparse_plan)
        )
        environment = dict(os.environ)
        for key in _FORBIDDEN_CONTROLS:
            environment.pop(key, None)
        environment.update(self.base_env)
        if plan_text:
            environment["BATS_XIWANG_PLAN"] = plan_text
        environment["FIG_ISPLIT_DIAG"] = "1"
        self.log_dir.mkdir(parents=True, exist_ok=True)
        stdout_path = self.log_dir / f"{batch_id}.stdout.log"
        stderr_path = self.log_dir / f"{batch_id}.stderr.log"
        with stdout_path.open("w", encoding="utf-8") as stdout_stream, \
             stderr_path.open("w", encoding="utf-8") as stderr_stream:
            completed = subprocess.run(
                self.command,
                cwd=self.workdir,
                env=environment,
                stdout=stdout_stream,
                stderr=stderr_stream,
                text=True,
                timeout=self.timeout_seconds,
                check=False,
            )
        if completed.returncode != 0:
            raise RuntimeError(
                f"FIG batch {batch_id} failed with return code {completed.returncode}"
            )
        def diagnostic_lines() -> Iterable[str]:
            for path in (stdout_path, stderr_path):
                with path.open("r", encoding="utf-8", errors="replace") as stream:
                    for line in stream:
                        if "SFE_LEVEL " in line:
                            yield line
        return _parse_sfe_level_lines(
            diagnostic_lines(), batch_id=batch_id, expected_plan=sparse_plan,
            reference_population=24,
        )


@dataclass(frozen=True)
class LearningResult:
    status: str
    frozen_plan: dict[int, int]
    completed_batches: int


def run_learning(
    controller: XiWangController, adapter: LearningAdapter,
    *, checkpoint_path: Path | None = None,
) -> LearningResult:
    while controller.status == "LEARNING":
        batch_id = f"batch_{len(controller.history) + 1:03d}"
        input_plan = dict(controller.current_plan)
        observations = list(adapter.run_batch(batch_id, input_plan))
        controller.ingest(batch_id, observations)
        if checkpoint_path is not None:
            target = Path(checkpoint_path)
            target.parent.mkdir(parents=True, exist_ok=True)
            temporary = target.with_suffix(target.suffix + ".tmp")
            temporary.write_text(
                json.dumps(controller.snapshot(), sort_keys=True, indent=2),
                encoding="utf-8",
            )
            temporary.replace(target)
    return LearningResult(
        status=controller.status,
        frozen_plan=dict(controller.current_plan),
        completed_batches=len(controller.history),
    )
