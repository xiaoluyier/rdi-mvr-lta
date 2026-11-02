from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import math
import random
from typing import Iterable, Mapping


_REQUIRED_CONFIG = {
    "fixed_population",
    "trust_region",
    "eta",
    "max_population",
    "patience",
    "max_batches",
    "bootstrap_replicates",
    "bootstrap_seed",
    "min_clusters",
}


@dataclass(frozen=True)
class XiWangConfig:
    fixed_population: int
    trust_region: int
    eta: float
    max_population: int
    patience: int
    max_batches: int
    bootstrap_replicates: int
    bootstrap_seed: int
    min_clusters: int

    @classmethod
    def from_dict(cls, raw: Mapping[str, object]) -> "XiWangConfig":
        missing = sorted(_REQUIRED_CONFIG - set(raw))
        extra = sorted(set(raw) - _REQUIRED_CONFIG)
        if missing:
            raise ValueError("missing XiWang config: " + ", ".join(missing))
        if extra:
            raise ValueError("unknown XiWang config: " + ", ".join(extra))
        result = cls(
            fixed_population=int(raw["fixed_population"]),
            trust_region=int(raw["trust_region"]),
            eta=float(raw["eta"]),
            max_population=int(raw["max_population"]),
            patience=int(raw["patience"]),
            max_batches=int(raw["max_batches"]),
            bootstrap_replicates=int(raw["bootstrap_replicates"]),
            bootstrap_seed=int(raw["bootstrap_seed"]),
            min_clusters=int(raw["min_clusters"]),
        )
        result._validate()
        return result

    def _validate(self) -> None:
        if self.fixed_population != 24:
            raise ValueError("fixed_population must be 24 for this frozen FIG baseline")
        if self.trust_region < 0:
            raise ValueError("trust_region must be nonnegative")
        if not math.isfinite(self.eta) or self.eta < 0.0:
            raise ValueError("eta must be finite and nonnegative")
        if self.max_population < self.fixed_population:
            raise ValueError("max_population must be at least fixed_population")
        if self.patience <= 0 or self.max_batches <= 0:
            raise ValueError("patience and max_batches must be positive")
        if self.bootstrap_replicates <= 0 or self.min_clusters <= 0:
            raise ValueError("bootstrap_replicates and min_clusters must be positive")

    def as_dict(self) -> dict[str, object]:
        return {
            "fixed_population": self.fixed_population,
            "trust_region": self.trust_region,
            "eta": self.eta,
            "max_population": self.max_population,
            "patience": self.patience,
            "max_batches": self.max_batches,
            "bootstrap_replicates": self.bootstrap_replicates,
            "bootstrap_seed": self.bootstrap_seed,
            "min_clusters": self.min_clusters,
        }

    @property
    def sha256(self) -> str:
        payload = json.dumps(self.as_dict(), sort_keys=True, separators=(",", ":"))
        return hashlib.sha256(payload.encode("utf-8")).hexdigest()


def frozen_config_for(model_id: str, ifun: str, *, level_count: int) -> XiWangConfig:
    if not model_id or ifun not in {"STR", "RDI"}:
        raise ValueError("model_id and IFUN identity are required")
    if level_count < 0:
        raise ValueError("level_count must be nonnegative")
    identity = f"XIWANG_BOOTSTRAP_V1|{model_id}|{ifun}".encode("utf-8")
    bootstrap_seed = int.from_bytes(hashlib.sha256(identity).digest()[:4], "big") & 0x7FFFFFFF
    return XiWangConfig.from_dict({
        "fixed_population": 24,
        "trust_region": max(1, int(level_count)),
        "eta": 0.005,
        "max_population": 48,
        "patience": 2,
        "max_batches": 10,
        "bootstrap_replicates": 2000,
        "bootstrap_seed": bootstrap_seed,
        "min_clusters": 30,
    })


@dataclass(frozen=True)
class BatchObservation:
    batch_id: str
    cluster_id: str
    level: int
    trials: int
    successes: int
    population: int

    def __post_init__(self) -> None:
        if not self.batch_id or not self.cluster_id:
            raise ValueError("batch_id and cluster_id are required")
        if self.level < 0 or self.trials <= 0 or self.population <= 0:
            raise ValueError("level, trials, and population are invalid")
        if self.successes < 0 or self.successes > self.trials:
            raise ValueError("successes must lie in [0,trials]")
        if self.trials != self.population:
            raise ValueError("SFE trials must equal the executed population")


@dataclass(frozen=True)
class LevelEstimate:
    level: int
    q_hat: float
    a_hat: float
    a_safe: float
    clusters: int
    trials: int
    successes: int
    bootstrap_a: tuple[float, ...]


@dataclass(frozen=True)
class PlanDecision:
    plan: dict[int, int]
    additions: int
    predicted_safe_gain: float
    reference_objective: float
    gain_gate: float
    stop_reason: str


def _coefficient(q: float) -> float:
    if q <= 0.0:
        return math.inf
    return max(0.0, (1.0 - q) / q)


def _quantile(values: Iterable[float], probability: float) -> float:
    ordered = sorted(values)
    if not ordered:
        raise ValueError("cannot take a quantile of no values")
    position = probability * (len(ordered) - 1)
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high or not math.isfinite(ordered[high]):
        return ordered[low]
    return ordered[low] + (position - low) * (ordered[high] - ordered[low])


def estimate_level_coefficients(
    observations: Iterable[BatchObservation],
    *,
    levels: Iterable[int],
    bootstrap_replicates: int,
    bootstrap_seed: int,
    min_clusters: int,
    expected_plan: Mapping[int, int] | None = None,
) -> dict[int, LevelEstimate]:
    rows = list(observations)
    by_level: dict[int, list[BatchObservation]] = {int(level): [] for level in levels}
    if len(by_level) != len(tuple(levels)):
        raise ValueError("levels must be unique")
    for row in rows:
        if row.level not in by_level:
            raise ValueError(f"unexpected level {row.level}")
        if expected_plan is not None and row.population != expected_plan[row.level]:
            raise ValueError(
                f"executed population mismatch at level {row.level}: "
                f"{row.population} != {expected_plan[row.level]}"
            )
        by_level[row.level].append(row)

    result: dict[int, LevelEstimate] = {}
    for level, level_rows in by_level.items():
        cluster_ids = [row.cluster_id for row in level_rows]
        if len(cluster_ids) != len(set(cluster_ids)):
            raise ValueError(f"duplicate cluster at level {level}")
        if len(level_rows) < min_clusters:
            raise ValueError(f"insufficient independent clusters at level {level}")
        total_trials = sum(row.trials for row in level_rows)
        total_successes = sum(row.successes for row in level_rows)
        q_hat = total_successes / total_trials
        a_hat = _coefficient(q_hat)
        generator = random.Random(bootstrap_seed + 104729 * level)
        bootstrap: list[float] = []
        for _ in range(bootstrap_replicates):
            sampled = [generator.choice(level_rows) for _ in level_rows]
            sampled_trials = sum(row.trials for row in sampled)
            sampled_successes = sum(row.successes for row in sampled)
            bootstrap.append(_coefficient(sampled_successes / sampled_trials))
        result[level] = LevelEstimate(
            level=level,
            q_hat=q_hat,
            a_hat=a_hat,
            a_safe=max(0.0, _quantile(bootstrap, 0.05)),
            clusters=len(level_rows),
            trials=total_trials,
            successes=total_successes,
            bootstrap_a=tuple(bootstrap),
        )
    return result


def solve_next_plan(
    *,
    current_plan: Mapping[int, int],
    mean_a: Mapping[int, float],
    safe_a: Mapping[int, float],
    safe_floors: Mapping[int, int],
    config: XiWangConfig,
    optimizable: Mapping[int, bool] | None = None,
) -> PlanDecision:
    levels = set(current_plan)
    if set(mean_a) != levels or set(safe_a) != levels or set(safe_floors) != levels:
        raise ValueError("all level maps must have exactly the same keys")
    plan = {level: int(current_plan[level]) for level in sorted(levels)}
    active = {level: True for level in levels}
    if optimizable is not None:
        if set(optimizable) != levels:
            raise ValueError("optimizable map must have exactly the plan levels")
        active = {level: bool(optimizable[level]) for level in levels}
    if any(value < config.fixed_population for value in plan.values()):
        raise ValueError("current plan is below the frozen FIXED population")
    if any(value > config.max_population for value in plan.values()):
        raise ValueError("current plan exceeds max_population")

    mandatory = 0
    for level in sorted(levels):
        floor = max(plan[level], int(safe_floors[level]))
        if floor > config.max_population:
            raise ValueError("safe floor exceeds max_population")
        mandatory += floor - plan[level]
        plan[level] = floor
    if mandatory > config.trust_region:
        raise ValueError("safe floors exceed this batch trust region")

    reference = sum(
        float(mean_a[level]) / current_plan[level]
        for level in levels if active[level]
    )
    gate = config.eta * reference
    additions = mandatory
    predicted = 0.0
    stop_reason = "TRUST_REGION"
    while additions < config.trust_region:
        candidates: list[tuple[float, int]] = []
        for level in sorted(levels):
            if not active[level] or plan[level] >= config.max_population:
                continue
            n = plan[level]
            gain = float(safe_a[level]) / (n * (n + 1.0))
            candidates.append((gain, level))
        if not candidates:
            stop_reason = "ALL_LEVELS_AT_MAX"
            break
        gain, level = max(candidates, key=lambda item: (item[0], -item[1]))
        if not (gain > gate):
            stop_reason = "GAIN_GATE"
            break
        plan[level] += 1
        additions += 1
        predicted += gain
    return PlanDecision(
        plan=plan,
        additions=additions,
        predicted_safe_gain=predicted,
        reference_objective=reference,
        gain_gate=gate,
        stop_reason=stop_reason,
    )
