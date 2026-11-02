from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable, Mapping


@dataclass(frozen=True)
class ConfirmationTask:
    method: str
    ifun: str
    model_id: str
    plan: dict[int, int]
    seed: int
    common: dict[str, object]


def build_xiwang_tasks(
    *,
    common: Mapping[str, object],
    xiwang_plans: Mapping[str, Mapping[str, Mapping[int, int]]],
    existing_fixed: Iterable[Mapping[str, object]],
) -> list[ConfirmationTask]:
    if set(xiwang_plans) != {"STR", "RDI"}:
        raise ValueError("XiWang plans must contain exactly STR and RDI")
    if int(common.get("stop_time", -1)) != 120:
        raise ValueError("final confirmation stop_time must be 120 seconds")
    tasks: list[ConfirmationTask] = []
    seen: set[tuple[str, str, int]] = set()
    for baseline in existing_fixed:
        ifun = str(baseline["ifun"])
        model_id = str(baseline["model_id"])
        seed = int(baseline["seed"])
        expected_fixed = f"{ifun}_FIXED"
        if baseline.get("method") != expected_fixed:
            raise ValueError("existing baseline is not a recognized FIXED arm")
        if model_id not in xiwang_plans[ifun]:
            raise ValueError(f"missing XiWang plan for {ifun}/{model_id}")
        key = (ifun, model_id, seed)
        if key in seen:
            raise ValueError("duplicate existing FIXED identity")
        seen.add(key)
        task_common = dict(baseline.get("common", {}))
        for field, value in common.items():
            if field in {
                "engine", "threshold_technique", "pool_mode", "stop_time",
                "model_sha256", "threshold_chain_sha256", "property_execution_mode",
            } and field in task_common and task_common[field] != value:
                raise ValueError(f"paired factor changed outside XiWang plan: {field}")
            task_common[field] = value
        tasks.append(ConfirmationTask(
            method=f"{ifun}_XIWANG",
            ifun=ifun,
            model_id=model_id,
            plan={int(k): int(v) for k, v in xiwang_plans[ifun][model_id].items()},
            seed=seed,
            common=task_common,
        ))
    if not tasks:
        raise ValueError("no existing FIXED rows were supplied")
    return tasks


def validate_xiwang_against_fixed(
    existing_fixed: Iterable[Mapping[str, object]],
    xiwang_tasks: Iterable[ConfirmationTask],
) -> None:
    fixed = list(existing_fixed)
    new = list(xiwang_tasks)
    fixed_keys = {
        (str(row["ifun"]), str(row["model_id"]), int(row["seed"])) for row in fixed
    }
    new_keys = {(row.ifun, row.model_id, row.seed) for row in new}
    if fixed_keys != new_keys:
        raise ValueError("XiWang tasks do not exactly match the reused FIXED identities")
    if any(row.method.endswith("_FIXED") for row in new):
        raise ValueError("new task list must never contain a FIXED run")
    protected = {
        "engine", "threshold_technique", "pool_mode", "stop_time",
        "model_sha256", "threshold_chain_sha256", "property_execution_mode",
    }
    fixed_by_key = {
        (str(row["ifun"]), str(row["model_id"]), int(row["seed"])): row
        for row in fixed
    }
    for task in new:
        baseline = fixed_by_key[(task.ifun, task.model_id, task.seed)]
        baseline_common = baseline.get("common", {})
        for field in protected:
            if field in baseline_common and field in task.common:
                if baseline_common[field] != task.common[field]:
                    raise ValueError(f"paired factor changed outside XiWang plan: {field}")
