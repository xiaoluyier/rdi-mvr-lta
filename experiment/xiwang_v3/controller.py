from __future__ import annotations

from dataclasses import dataclass, replace
import math
from typing import Iterable, Mapping

from .core import (
    BatchObservation,
    LevelEstimate,
    XiWangConfig,
    _quantile,
    estimate_level_coefficients,
    solve_next_plan,
)


@dataclass(frozen=True)
class BatchResult:
    batch_id: str
    input_plan: dict[int, int]
    output_plan: dict[int, int]
    batch_a: dict[int, float]
    historical_a: dict[int, float]
    safe_a: dict[int, float]
    level_states: dict[int, str]
    stop_reason: str
    status: str


class XiWangController:
    def __init__(
        self, *, levels: Iterable[int], config: XiWangConfig,
        initial_plan: Mapping[int, int] | None = None,
    ):
        self.levels = tuple(sorted(int(level) for level in levels))
        if not self.levels or len(self.levels) != len(set(self.levels)):
            raise ValueError("levels must be nonempty and unique")
        self.config = config
        self.initial_plan = {
            level: int((initial_plan or {}).get(level, config.fixed_population))
            for level in self.levels
        }
        self.current_plan = dict(self.initial_plan)
        self.history: list[dict[str, object]] = []
        self.stable_batches = 0
        self.status = "LEARNING"

    def ingest(
        self, batch_id: str, observations: Iterable[BatchObservation],
        *, safe_floors: Mapping[int, int] | None = None,
    ) -> BatchResult:
        if self.status != "LEARNING":
            raise ValueError("controller is no longer accepting learning batches")
        if any(item["batch_id"] == batch_id for item in self.history):
            raise ValueError("duplicate batch_id")
        rows = list(observations)
        if not rows or any(row.batch_id != batch_id for row in rows):
            raise ValueError("observation batch identity mismatch")
        observed_levels = tuple(sorted({row.level for row in rows}))
        new_levels = tuple(level for level in observed_levels if level not in self.current_plan)
        for level in new_levels:
            populations = {row.population for row in rows if row.level == level}
            if len(populations) != 1:
                raise ValueError(f"new level {level} has inconsistent executed populations")
            self.current_plan[level] = populations.pop()
        if new_levels:
            self.levels = tuple(sorted(self.current_plan))
        cluster_counts = {
            level: len({row.cluster_id for row in rows if row.level == level})
            for level in self.levels
        }
        level_states = {
            level: (
                "ZERO_SUCCESS_FROZEN"
                if (cluster_counts[level] >= self.config.min_clusters and
                    sum(row.successes for row in rows if row.level == level) == 0)
                else "OPTIMIZABLE_ACTIVE" if cluster_counts[level] >= self.config.min_clusters
                else ("UNOBSERVED_FROZEN" if cluster_counts[level] == 0
                      else "INSUFFICIENT_CLUSTERS_FROZEN")
            )
            for level in self.levels
        }
        active_levels = tuple(
            level for level in self.levels
            if level_states[level] == "OPTIMIZABLE_ACTIVE"
        )
        active_rows = [row for row in rows if row.level in active_levels]
        estimates = estimate_level_coefficients(
            active_rows,
            levels=active_levels,
            bootstrap_replicates=self.config.bootstrap_replicates,
            bootstrap_seed=self.config.bootstrap_seed + len(self.history),
            min_clusters=self.config.min_clusters,
            expected_plan=self.current_plan,
        )
        input_plan = dict(self.current_plan)
        floors = dict(input_plan)
        if safe_floors is not None:
            floors.update({int(level): int(value) for level, value in safe_floors.items()})
        record = {
            "batch_id": batch_id,
            "input_plan": input_plan,
            "estimates": estimates,
            "observations": list(rows),
            "safe_floors": floors,
        }
        provisional = self.history + [record]
        historical_a, historical_safe = self._historical_coefficients(provisional)
        batch_config = replace(
            self.config,
            trust_region=max(1, len(observed_levels)),
        )
        decision = solve_next_plan(
            current_plan=input_plan,
            mean_a=historical_a,
            safe_a=historical_safe,
            safe_floors=floors,
            config=batch_config,
            optimizable={
                level: level_states[level] == "OPTIMIZABLE_ACTIVE"
                for level in self.levels
            },
        )
        output_plan = dict(decision.plan)
        self.stable_batches = self.stable_batches + 1 if output_plan == input_plan else 0
        self.current_plan = output_plan
        record["output_plan"] = output_plan
        record["historical_a"] = historical_a
        record["safe_a"] = historical_safe
        record["decision_stop_reason"] = decision.stop_reason
        record["level_states"] = level_states
        self.history.append(record)
        if self.stable_batches >= self.config.patience:
            self.status = "CONVERGED"
        elif len(self.history) >= self.config.max_batches:
            self.status = "MAX_BATCH_REACHED_NOT_CONVERGED"
        return BatchResult(
            batch_id=batch_id,
            input_plan=input_plan,
            output_plan=output_plan,
            batch_a={level: estimate.a_hat for level, estimate in estimates.items()},
            historical_a=historical_a,
            safe_a=historical_safe,
            level_states=level_states,
            stop_reason=decision.stop_reason,
            status=self.status,
        )

    def sparse_plan(self, *, reference_population: int) -> dict[int, int]:
        if reference_population <= 0:
            raise ValueError("reference_population must be positive")
        return {
            level: population
            for level, population in sorted(self.current_plan.items())
            if population != reference_population
        }

    def _historical_coefficients(
        self, records: list[dict[str, object]],
    ) -> tuple[dict[int, float], dict[int, float]]:
        mean: dict[int, float] = {}
        safe: dict[int, float] = {}
        for level in self.levels:
            estimates = [
                record["estimates"][level]
                for record in records if level in record["estimates"]
            ]
            if not estimates:
                mean[level] = 0.0
                safe[level] = 0.0
                continue
            weights = [estimate.clusters for estimate in estimates]
            denominator = sum(weights)
            values = [estimate.a_hat for estimate in estimates]
            if any(math.isinf(value) and weight > 0 for value, weight in zip(values, weights)):
                mean[level] = math.inf
            else:
                mean[level] = sum(value * weight for value, weight in zip(values, weights)) / denominator
            bootstrap_history = []
            for index in range(self.config.bootstrap_replicates):
                replicate = [estimate.bootstrap_a[index] for estimate in estimates]
                if any(math.isinf(value) and weight > 0 for value, weight in zip(replicate, weights)):
                    bootstrap_history.append(math.inf)
                else:
                    bootstrap_history.append(
                        sum(value * weight for value, weight in zip(replicate, weights)) / denominator
                    )
            safe[level] = max(0.0, _quantile(bootstrap_history, 0.05))
        return mean, safe

    def snapshot(self) -> dict[str, object]:
        return {
            "schema": "XIWANG_CLEAN_V1",
            "config_sha256": self.config.sha256,
            "levels": list(self.levels),
            "initial_plan": dict(self.initial_plan),
            "current_plan": dict(self.current_plan),
            "stable_batches": self.stable_batches,
            "status": self.status,
            "batches": [
                {
                    "batch_id": record["batch_id"],
                    "safe_floors": dict(record["safe_floors"]),
                    "observations": [
                        {
                            "batch_id": row.batch_id,
                            "cluster_id": row.cluster_id,
                            "level": row.level,
                            "trials": row.trials,
                            "successes": row.successes,
                            "population": row.population,
                        }
                        for row in record["observations"]
                    ],
                }
                for record in self.history
            ],
        }

    @classmethod
    def restore(
        cls, snapshot: Mapping[str, object], *, config: XiWangConfig,
    ) -> "XiWangController":
        if snapshot.get("schema") != "XIWANG_CLEAN_V1":
            raise ValueError("snapshot schema mismatch")
        if snapshot.get("config_sha256") != config.sha256:
            raise ValueError("config identity mismatch")
        controller = cls(
            levels=snapshot["levels"], config=config,
            initial_plan={
                int(k): int(v)
                for k, v in snapshot.get("initial_plan", {}).items()
            } or None,
        )
        for batch in snapshot.get("batches", []):
            rows = [BatchObservation(**raw) for raw in batch["observations"]]
            controller.ingest(
                str(batch["batch_id"]), rows,
                safe_floors={int(k): int(v) for k, v in batch["safe_floors"].items()},
            )
        expected_plan = {int(k): int(v) for k, v in snapshot["current_plan"].items()}
        if controller.current_plan != expected_plan:
            raise ValueError("resume history does not reproduce current plan")
        if controller.stable_batches != int(snapshot["stable_batches"]):
            raise ValueError("resume history does not reproduce stability state")
        if controller.status != str(snapshot["status"]):
            raise ValueError("resume history does not reproduce controller status")
        return controller
