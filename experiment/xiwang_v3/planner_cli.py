"""Batch-boundary planner invoked by the original FIG SFE process."""

from __future__ import annotations

import argparse
from dataclasses import asdict
import json
from pathlib import Path
import sys

from .empirical_kernel import (
    ChildTransition,
    EmpiricalModalKernel,
    estimate_interaction_objective,
)
from .solver import solve_global_plan


def _load_request(path: Path) -> dict:
    raw = json.loads(path.read_text(encoding="utf-8"))
    required = {
        "batch_index",
        "levels",
        "lower",
        "upper",
        "initial_level",
        "initial_parent_signature",
        "rollout_count",
        "transitions_file",
    }
    missing = required - set(raw)
    if missing:
        raise ValueError(f"planner request missing keys: {sorted(missing)}")
    return raw


def _load_transitions(path: Path) -> list[ChildTransition]:
    result: list[ChildTransition] = []
    with path.open("r", encoding="utf-8") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            raw = json.loads(line)
            try:
                result.append(
                    ChildTransition(
                        current_level=int(raw["current_level"]),
                        parent_signature=str(raw["parent_signature"]),
                        child_signature=str(raw["child_signature"]),
                        reached_level=int(raw["reached_level"]),
                        rare=bool(raw["rare"]),
                        event_cost=float(raw["event_cost"]),
                    )
                )
            except (KeyError, TypeError, ValueError) as error:
                raise ValueError(f"invalid transition at line {line_number}") from error
    return result


def plan_request(request_path: Path) -> dict:
    request = _load_request(request_path)
    levels = tuple(int(value) for value in request["levels"])
    if not levels:
        return {
            "status": "XIWANG_NOT_APPLICABLE_IDENTITY",
            "batch_index": int(request["batch_index"]),
            "plan": {},
            "reason": "NO_INTERMEDIATE_THRESHOLDS",
        }
    base = request_path.parent
    transitions_path = Path(request["transitions_file"])
    if not transitions_path.is_absolute():
        transitions_path = base / transitions_path
    transitions = _load_transitions(transitions_path)
    kernel = EmpiricalModalKernel(transitions)
    rollout_count = int(request["rollout_count"])
    if rollout_count <= 1:
        raise ValueError("rollout_count must exceed one")
    batch_index = int(request["batch_index"])
    seed_base = int(request.get("shadow_seed_base", 73000000)) + batch_index * 1000003
    objective = estimate_interaction_objective(
        kernel,
        levels=levels,
        lower=tuple(int(value) for value in request["lower"]),
        upper=tuple(int(value) for value in request["upper"]),
        rollout_seeds=tuple(seed_base + index for index in range(rollout_count)),
        initial_level=int(request["initial_level"]),
        initial_parent_signature=str(request["initial_parent_signature"]),
        default_population=int(request.get("default_population", 24)),
    )
    incumbent_mapping = {
        int(level): int(value)
        for level, value in request.get("current_plan", {}).items()
    }
    incumbent = tuple(incumbent_mapping.get(level, 24) for level in levels)
    solved = solve_global_plan(objective, incumbent=incumbent)
    default_population = int(request.get("default_population", 24))
    sparse = {
        str(level): population
        for level, population in zip(levels, solved.evaluation.plan)
        if population != default_population
    }
    return {
        "status": "OPTIMAL",
        "batch_index": batch_index,
        "plan": sparse,
        "plan_text": ",".join(
            f"{level}:{sparse[str(level)]}" for level in levels if str(level) in sparse
        ),
        "complete_plan": {
            str(level): population
            for level, population in zip(levels, solved.evaluation.plan)
        },
        "objective": solved.evaluation.objective,
        "predicted_variance": solved.evaluation.variance,
        "predicted_event_cost": solved.evaluation.expected_cost,
        "boxes_visited": solved.boxes_visited,
        "plans_evaluated": solved.plans_evaluated,
        "pruned_boxes": solved.pruned_boxes,
        "certified_global": solved.certified_global,
        "transition_count": len(transitions),
        "rollout_count": rollout_count,
        "statistical_scope": "EMPIRICAL_FORMAL_ROOT_TRANSITION_KERNEL",
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--request", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--plan-output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = plan_request(args.request)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(result, ensure_ascii=False, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        if args.plan_output is not None:
            args.plan_output.parent.mkdir(parents=True, exist_ok=True)
            args.plan_output.write_text(str(result.get("plan_text", "")) + "\n", encoding="utf-8")
        return 0
    except Exception as error:
        failure = {
            "status": "ERROR",
            "error_type": type(error).__name__,
            "error": str(error),
        }
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(failure, sort_keys=True) + "\n", encoding="utf-8")
        if args.plan_output is not None:
            args.plan_output.parent.mkdir(parents=True, exist_ok=True)
            args.plan_output.write_text("ERROR\n", encoding="utf-8")
        print(f"XiWang v3 planner failed: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
