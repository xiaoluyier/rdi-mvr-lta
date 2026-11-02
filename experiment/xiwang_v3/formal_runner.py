from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from typing import Mapping, Protocol

from .controller import XiWangController
from .core import XiWangConfig
from .execution import parse_sfe_level_log


@dataclass(frozen=True)
class FigRunSpec:
    run_id: str
    phase: str
    model_path: str
    ifun: str
    seed: int
    thresholds_adhoc: str
    sparse_plan: dict[int, int]
    stop_time: int
    mission_time: int = 30
    global_effort: int = 5


@dataclass(frozen=True)
class FigRunArtifact:
    diagnostic_text: str
    metrics: dict[str, object]


class FigBackend(Protocol):
    def run(self, spec: FigRunSpec) -> FigRunArtifact: ...


def _is_allowed_out_dir(out_dir: Path, assigned_tmp_root: Path) -> bool:
    resolved = out_dir.resolve()
    assigned = assigned_tmp_root.resolve()
    if assigned in resolved.parents:
        return True
    return (
        resolved.parent == Path("/tmp").resolve()
        and resolved.name.startswith("phase_pand_reuse_")
    )


_NUMBER = r"([0-9.eE+\-]+|inf|nan)"
_COST_KEYS = (
    "production_trajectory_count", "simulation_step_count",
    "event_transition_count", "threshold_build_trajectory_count",
    "threshold_build_simulation_step_count",
    "threshold_build_event_transition_count",
)


def _metrics(fig_out: str, fig_err_lines: list[str]) -> dict[str, object]:
    result: dict[str, object] = {}
    estimates = re.findall(r"Computed estimate:\s*" + _NUMBER, fig_out, re.I)
    rhws = re.findall(r"RHW:\s*" + _NUMBER, fig_out, re.I)
    samples = re.findall(r"Computed estimate:.*?\((\d+)\s+samples\)", fig_out, re.I)
    if estimates:
        result["estimate"] = float(estimates[-1])
        result["nonzero"] = int(math.isfinite(float(estimates[-1])) and float(estimates[-1]) > 0)
    if rhws:
        result["rhw"] = float(rhws[-1])
    elif estimates:
        ci95 = re.findall(
            r"95%\s+confidence[\s\S]*?interval:\s*\[\s*"
            + _NUMBER + r"\s*,\s*" + _NUMBER + r"\s*\]",
            fig_out, re.I,
        )
        estimate = float(estimates[-1])
        if ci95 and math.isfinite(estimate) and estimate > 0.0:
            low, high = (float(ci95[-1][0]), float(ci95[-1][1]))
            if math.isfinite(low) and math.isfinite(high) and high >= low:
                result["rhw"] = ((high - low) / 2.0) / estimate
    if samples:
        result["root_sweeps"] = int(samples[-1])
    if estimates and float(estimates[-1]) == 0.0 and "rhw" not in result:
        result["rhw"] = None
    stderr_text = "\n".join(fig_err_lines)
    final_rows = re.findall(
        r"^\s*Estimate:\s*" + _NUMBER
        + r"\s*\(var=" + _NUMBER + r",prec=" + _NUMBER + r"\)",
        stderr_text, re.I | re.M,
    )
    if final_rows:
        result.setdefault("estimate", float(final_rows[-1][0]))
        result.setdefault("rhw", float(final_rows[-1][2]))
        estimate = float(result["estimate"])
        result.setdefault("nonzero", int(math.isfinite(estimate) and estimate > 0))
    sums = {key: 0 for key in _COST_KEYS}
    for line in fig_err_lines:
        if "SFE_COUNT_SUMMARY" not in line:
            continue
        fields = dict(re.findall(r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)", line))
        for key in _COST_KEYS:
            sums[key] += int(round(float(fields.get(key, "0"))))
    result.update({key: value for key, value in sums.items() if value})
    return result


class OneClickBackend:
    """Thin wrapper around the unchanged FIG one-click runner."""

    def __init__(self, *, command: tuple[str, ...], runtime_dir: Path | str):
        if not command:
            raise ValueError("one-click command is required")
        self.command = tuple(str(value) for value in command)
        self.runtime_dir = Path(runtime_dir)

    def run(self, spec: FigRunSpec) -> FigRunArtifact:
        run_key = hashlib.sha256(spec.run_id.encode("utf-8")).hexdigest()
        run_dir = self.runtime_dir / "runs" / run_key
        tmp_root = self.runtime_dir / "tmp" / run_key
        run_dir.mkdir(parents=True, exist_ok=True)
        tmp_root.mkdir(parents=True, exist_ok=True)
        env = dict(os.environ)
        for key in list(env):
            if key.startswith("BATS_") or key.startswith("CLF_"):
                env.pop(key, None)
        env.update({
            "BATS_CONFIG_ID": f"{spec.ifun}_XIWANG_V3",
            "IFUN_KIND": "STR" if spec.ifun == "STR" else "N4",
            "MODE": "isplit",
            "ENGINE": "sfe",
            "THR_TECH": "fix" if spec.thresholds_adhoc else "hyb",
            "STOP_TIME": f"{spec.stop_time}s",
            "GLOBAL_EFFORT": str(spec.global_effort),
            "TRANSIENT_HORIZON": str(spec.mission_time),
            "RNG_SEED": str(spec.seed),
            "FIG_TIMEOUT": str(spec.stop_time + 180),
            "SFE_POOL_MODE": "modal",
            "PROPERTY_EXECUTION_MODE": "single",
            "PHASE_PAND_TMP_ROOT": str(tmp_root),
        })
        if spec.phase == "SEQUENTIAL_FORMAL":
            env["BATS_XIWANG_DIAGNOSTICS"] = "1"
            env["BATS_XIWANG_DIAGNOSTIC_ROOTS"] = "256"
        env["SFE_COUNT_ONLY"] = "1"
        if spec.thresholds_adhoc:
            env["THRESHOLDS_ADHOC"] = spec.thresholds_adhoc
        if spec.sparse_plan:
            env["BATS_XIWANG_PLAN"] = ",".join(
                f"{level}:{spec.sparse_plan[level]}" for level in sorted(spec.sparse_plan)
            )
        completed = subprocess.run(
            self.command + (spec.model_path,), env=env,
            text=True, capture_output=True, check=False,
        )
        (run_dir / "runner.stdout").write_text(completed.stdout, encoding="utf-8")
        (run_dir / "runner.stderr").write_text(completed.stderr, encoding="utf-8")
        if completed.returncode != 0:
            raise RuntimeError(f"FIG run {spec.run_id} failed: {completed.returncode}")
        matches = re.findall(r"^OUT_DIR:\s*(.+?)\s*$", completed.stdout, re.M)
        if len(matches) != 1:
            raise RuntimeError("one-click runner did not report exactly one OUT_DIR")
        out_dir = Path(matches[0]).resolve()
        if not _is_allowed_out_dir(out_dir, tmp_root):
            raise RuntimeError("one-click OUT_DIR escaped the assigned runtime directory")
        fig_out = (out_dir / "fig.out").read_text(encoding="utf-8", errors="replace")
        err_lines = []
        cost_sums = {key: 0 for key in _COST_KEYS}
        with (out_dir / "fig.err").open(
            "r", encoding="utf-8", errors="replace"
        ) as stream:
            for line in stream:
                if "SFE_COUNT_SUMMARY" in line:
                    fields = dict(re.findall(
                        r"([A-Za-z_][A-Za-z0-9_]*)=([^\s]+)", line
                    ))
                    for key in _COST_KEYS:
                        cost_sums[key] += int(round(float(fields.get(key, "0"))))
                elif "SFE_LEVEL " in line or line.lstrip().startswith("Estimate:"):
                    err_lines.append(line.rstrip("\r\n"))
        diagnostic = "\n".join(line for line in err_lines if "SFE_LEVEL " in line)
        (run_dir / "fig.out").write_text(fig_out, encoding="utf-8")
        _atomic_json(run_dir / "identity.json", {
            "run_id": spec.run_id, "phase": spec.phase, "seed": spec.seed,
            "model_path": spec.model_path, "ifun": spec.ifun,
            "sparse_plan": spec.sparse_plan,
        })
        metrics = _metrics(fig_out, err_lines)
        metrics.update({key: value for key, value in cost_sums.items() if value})
        if spec.phase == "SEQUENTIAL_FORMAL":
            summary = "\n".join(
                line for line in err_lines
                if "SFE_LEVEL " in line or line.lstrip().startswith("Estimate:")
            )
        else:
            # The production engine emits one SFE_COUNT_SUMMARY per root sweep.
            # Persisting all of them duplicates hundreds of MB per task even
            # though ``metrics`` already contains their exact aggregate.
            summary = "XIWANG_FINAL_METRICS " + json.dumps(
                metrics, sort_keys=True, separators=(",", ":")
            )
        (run_dir / "fig.err.summary").write_text(summary, encoding="utf-8")
        if spec.phase == "SEQUENTIAL_FORMAL" and "estimate" not in metrics:
            raise RuntimeError("FIG completed but estimate could not be parsed")
        if (
            spec.phase == "SEQUENTIAL_FORMAL"
            and float(metrics["estimate"]) > 0.0
            and not isinstance(metrics.get("rhw"), (int, float))
        ):
            raise RuntimeError("FIG completed but estimate/RHW could not be parsed")
        shutil.rmtree(out_dir)
        return FigRunArtifact(diagnostic, metrics)


def _read_jsonl(path: Path) -> list[dict[str, object]]:
    if not path.is_file():
        raise FileNotFoundError(path)
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line]


def _atomic_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, ensure_ascii=False, sort_keys=True, indent=2),
        encoding="utf-8",
    )
    temporary.replace(path)


def _config(raw: Mapping[str, object]) -> XiWangConfig:
    return XiWangConfig.from_dict({
        "fixed_population": int(raw["fixed_population_reference"]),
        "trust_region": 1,
        "eta": float(raw["eta"]),
        "max_population": int(raw["max_population"]),
        "patience": int(raw["patience"]),
        "max_batches": int(raw["max_batches"]),
        "bootstrap_replicates": int(raw["bootstrap_replicates"]),
        "bootstrap_seed": int(raw["bootstrap_seed"]),
        "min_clusters": int(raw["min_clusters"]),
    })


class FormalSequentialRunner:
    def __init__(
        self, package_dir: Path | str, server: str, backend: FigBackend,
        *, parallel_jobs: int = 1,
    ):
        self.root = Path(package_dir)
        self.server = str(server)
        self.backend = backend
        self.parallel_jobs = int(parallel_jobs)
        if self.parallel_jobs <= 0:
            raise ValueError("parallel_jobs must be positive")

    def _run_controller(self, definition: Mapping[str, object]) -> dict[str, int]:
        controller_id = str(definition["controller_id"])
        tasks = list(definition["formal_batches"])
        indices = [int(task["batch_index"]) for task in tasks]
        if indices != list(range(1, len(tasks) + 1)):
            raise ValueError(f"nonsequential batch indices for {controller_id}")
        cfg = _config(definition["optimizer"])
        initial_plan = {
            int(level): int(population)
            for level, population in definition.get("initial_full_plan", {0: cfg.fixed_population}).items()
        }
        controller = XiWangController(
            levels=initial_plan, config=cfg, initial_plan=initial_plan,
        )
        initialization = str(definition.get("initialization", "ALL_24"))
        protocol = (
            "XIWANG_V3_WARM_SEQUENTIAL_FORMAL"
            if initialization == "V1_FROZEN_PLAN"
            else "XIWANG_V3_SEQUENTIAL_FORMAL"
        )
        completed = 0
        skipped = 0

        for task in tasks:
            task_id = str(task["task_id"])
            target = self.root / "results" / f"{task_id}.json"
            if target.is_file():
                saved = json.loads(target.read_text(encoding="utf-8"))
                if (
                    saved.get("task_id") != task_id
                    or saved.get("status") != "COMPLETE"
                    or saved.get("protocol") != protocol
                ):
                    raise ValueError("existing result identity mismatch")
                controller = XiWangController.restore(
                    saved["controller_after"], config=cfg,
                )
                skipped += 1
                continue

            sparse = controller.sparse_plan(
                reference_population=cfg.fixed_population,
            )
            artifact = self.backend.run(FigRunSpec(
                run_id=task_id,
                phase="SEQUENTIAL_FORMAL",
                model_path=str(self.root / str(task["model_path"])),
                ifun=str(task["ifun"]),
                seed=int(task["seed"]),
                thresholds_adhoc=str(task["thresholds_adhoc"]),
                sparse_plan=sparse,
                stop_time=int(task["stop_time"]),
                mission_time=int(task.get("mission_time", 30)),
                global_effort=int(task.get("global_effort", 5)),
            ))

            planning_started = time.perf_counter()
            if controller.status == "LEARNING":
                batch_id = f"formal_batch_{int(task['batch_index']):03d}"
                observations = parse_sfe_level_log(
                    artifact.diagnostic_text,
                    batch_id=batch_id,
                    expected_plan=sparse,
                    reference_population=cfg.fixed_population,
                )
                controller.ingest(batch_id, observations)
            planning_seconds = time.perf_counter() - planning_started
            next_sparse = controller.sparse_plan(
                reference_population=cfg.fixed_population,
            )
            _atomic_json(target, {
                **task,
                "status": "COMPLETE",
                "protocol": protocol,
                "initialization": initialization,
                "formal_data_included": True,
                "sparse_plan_used": sparse,
                "metrics": artifact.metrics,
                "planning_seconds_after_run": planning_seconds,
                "optimizer_updates_completed": len(controller.history),
                "optimizer_status_after": controller.status,
                "next_sparse_plan": next_sparse,
                "controller_after": controller.snapshot(),
            })
            completed += 1

        _atomic_json(
            self.root / "controller_status" /
            f"{str(definition['model_id'])}__{str(definition['ifun'])}.json",
            {
                "controller_id": controller_id,
                "formal_batches": len(tasks),
                "optimizer_updates_completed": len(controller.history),
                "optimizer_status": controller.status,
                "final_sparse_plan": controller.sparse_plan(
                    reference_population=cfg.fixed_population,
                ),
            },
        )
        return {"completed": completed, "skipped": skipped, "batches": len(tasks)}

    def run_all(self) -> dict[str, int]:
        controllers = _read_jsonl(
            self.root / f"server_{self.server}_controllers.jsonl"
        )
        with ThreadPoolExecutor(max_workers=self.parallel_jobs) as pool:
            outcomes = list(pool.map(self._run_controller, controllers))
        report = {
            "controllers": len(controllers),
            "parallel_controller_chains": self.parallel_jobs,
            "formal_batches": sum(item["batches"] for item in outcomes),
            "completed_formal_batches": sum(item["completed"] for item in outcomes),
            "skipped_formal_batches": sum(item["skipped"] for item in outcomes),
        }
        _atomic_json(self.root / f"server_{self.server}_status.json", report)
        return report


def validate_results(package_dir: Path | str, server: str) -> dict[str, int]:
    root = Path(package_dir)
    controllers = _read_jsonl(root / f"server_{server}_controllers.jsonl")
    tasks = [task for item in controllers for task in item["formal_batches"]]
    complete = 0
    for task in tasks:
        path = root / "results" / f"{task['task_id']}.json"
        if not path.is_file():
            continue
        result = json.loads(path.read_text(encoding="utf-8"))
        if result.get("task_id") != task["task_id"] or result.get("status") != "COMPLETE":
            raise ValueError(f"invalid result identity: {path}")
        complete += 1
    return {"expected": len(tasks), "complete": complete, "missing": len(tasks) - complete}


def main() -> int:
    parser = argparse.ArgumentParser(description="Run XiWang v3 sequential formal batches")
    parser.add_argument("--package", required=True)
    parser.add_argument("--server", required=True)
    parser.add_argument("--runner")
    parser.add_argument("--parallel-jobs", type=int, default=16)
    parser.add_argument("--validate-only", action="store_true")
    args = parser.parse_args()
    if args.validate_only:
        report = validate_results(args.package, args.server)
    else:
        if not args.runner:
            parser.error("--runner is required unless --validate-only is used")
        backend = OneClickBackend(
            command=("bash", args.runner),
            runtime_dir=Path(args.package) / "runtime" / args.server,
        )
        report = FormalSequentialRunner(
            args.package, args.server, backend, parallel_jobs=args.parallel_jobs,
        ).run_all()
    print(json.dumps(report, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
