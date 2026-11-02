"""Empirical full-pool transition kernel for original modal FIG SFE.

The kernel is learned only from child transitions produced by formal roots.
Shadow replay performs no FIG simulation.  It reproduces the legacy
round-robin parent replication, success factor, modal reached-level selection,
and next-parent-pool construction on the empirical transition distribution.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
from math import fsum
from typing import Iterable, Mapping, Sequence

from .objective import InteractionObjective


@dataclass(frozen=True)
class ChildTransition:
    current_level: int
    parent_signature: str
    child_signature: str
    reached_level: int
    rare: bool
    event_cost: float

    @property
    def advanced(self) -> bool:
        return self.reached_level > self.current_level

    @property
    def success(self) -> bool:
        return self.advanced or self.rare


@dataclass(frozen=True)
class ShadowRootResult:
    estimate: float
    event_cost: float
    path: tuple[int, ...]
    factors: tuple[float, ...]


def _uniform(seed: int, *parts: object) -> float:
    payload = "|".join((str(seed), *(str(part) for part in parts))).encode("utf-8")
    raw = hashlib.blake2b(payload, digest_size=8, person=b"xiwangv3").digest()
    integer = int.from_bytes(raw, "little")
    return (integer + 0.5) / float(1 << 64)


class EmpiricalModalKernel:
    def __init__(self, transitions: Iterable[ChildTransition]) -> None:
        exact: dict[tuple[int, str], list[ChildTransition]] = {}
        pooled: dict[int, list[ChildTransition]] = {}
        for transition in transitions:
            if transition.current_level < 0 or transition.reached_level < transition.current_level:
                raise ValueError("invalid reached level")
            if not transition.parent_signature or not transition.child_signature:
                raise ValueError("state signatures must be nonempty")
            if transition.event_cost < 0.0:
                raise ValueError("event cost must be nonnegative")
            exact.setdefault(
                (transition.current_level, transition.parent_signature), []
            ).append(transition)
            pooled.setdefault(transition.current_level, []).append(transition)
        if not pooled:
            raise ValueError("at least one transition is required")
        self._exact = {key: tuple(value) for key, value in exact.items()}
        self._pooled = {key: tuple(value) for key, value in pooled.items()}

    @property
    def observed_levels(self) -> tuple[int, ...]:
        return tuple(sorted(self._pooled))

    def outcomes(self, level: int, parent_signature: str) -> tuple[ChildTransition, ...]:
        result = self._exact.get((level, parent_signature))
        if result:
            return result
        result = self._pooled.get(level)
        if not result:
            raise KeyError(f"no empirical transitions for level {level}")
        return result

    def sample(
        self,
        *,
        level: int,
        parent_signature: str,
        seed: int,
        step: int,
        child_index: int,
    ) -> ChildTransition:
        outcomes = self.outcomes(level, parent_signature)
        u = _uniform(seed, step, level, child_index, parent_signature)
        index = min(len(outcomes) - 1, int(u * len(outcomes)))
        return outcomes[index]

    def simulate_root(
        self,
        plan: Mapping[int, int],
        *,
        seed: int,
        initial_level: int,
        initial_parent_signature: str,
        default_population: int = 24,
        max_steps: int | None = None,
    ) -> ShadowRootResult:
        if default_population <= 0:
            raise ValueError("default_population must be positive")
        population_plan = {int(level): int(value) for level, value in plan.items()}
        if any(value <= 0 for value in population_plan.values()):
            raise ValueError("populations must be positive")
        parents = (initial_parent_signature,)
        level = int(initial_level)
        estimate = 1.0
        event_cost = 0.0
        path: list[int] = []
        factors: list[float] = []
        limit = max_steps if max_steps is not None else len(self.observed_levels) + 2

        for step in range(limit):
            if level not in self._pooled:
                break
            population = population_plan.get(level, default_population)
            outcomes: list[ChildTransition] = []
            for child_index in range(population):
                parent = parents[child_index % len(parents)]
                outcome = self.sample(
                    level=level,
                    parent_signature=parent,
                    seed=seed,
                    step=step,
                    child_index=child_index,
                )
                outcomes.append(outcome)
                event_cost += outcome.event_cost
            successes = sum(outcome.success for outcome in outcomes)
            factor = successes / float(population)
            estimate *= factor
            path.append(level)
            factors.append(factor)

            advanced = [outcome for outcome in outcomes if outcome.advanced]
            if not advanced:
                break
            histogram: dict[int, int] = {}
            for outcome in advanced:
                histogram[outcome.reached_level] = histogram.get(outcome.reached_level, 0) + 1
            # std::max_element over the ordered std::map keeps the first key on
            # count ties, hence the smallest reached level wins.
            modal_level = min(
                histogram,
                key=lambda reached: (-histogram[reached], reached),
            )
            parents = tuple(
                outcome.child_signature
                for outcome in advanced
                if outcome.reached_level == modal_level
            )
            level = modal_level
        else:
            raise RuntimeError("empirical modal path did not terminate")

        return ShadowRootResult(estimate, event_cost, tuple(path), tuple(factors))


def estimate_interaction_objective(
    kernel: EmpiricalModalKernel,
    *,
    levels: Sequence[int],
    lower: Sequence[int],
    upper: Sequence[int],
    rollout_seeds: Sequence[int],
    initial_level: int,
    initial_parent_signature: str,
    default_population: int = 24,
) -> InteractionObjective:
    """Estimate all 2^L full-interaction corners using common random numbers."""
    level_tuple = tuple(int(level) for level in levels)
    lower_tuple = tuple(int(value) for value in lower)
    upper_tuple = tuple(int(value) for value in upper)
    if not rollout_seeds:
        raise ValueError("rollout_seeds must be nonempty")
    if len(level_tuple) != len(lower_tuple) or len(level_tuple) != len(upper_tuple):
        raise ValueError("level and bound dimensions differ")
    if len(set(level_tuple)) != len(level_tuple):
        raise ValueError("levels must be unique")

    corner_variances: dict[int, float] = {}
    corner_costs: dict[int, float] = {}
    for mask in range(1 << len(level_tuple)):
        plan = {
            level: upper_tuple[index] if mask & (1 << index) else lower_tuple[index]
            for index, level in enumerate(level_tuple)
        }
        roots = [
            kernel.simulate_root(
                plan,
                seed=int(seed),
                initial_level=initial_level,
                initial_parent_signature=initial_parent_signature,
                default_population=default_population,
            )
            for seed in rollout_seeds
        ]
        mean = fsum(root.estimate for root in roots) / len(roots)
        second = fsum(root.estimate * root.estimate for root in roots) / len(roots)
        corner_variances[mask] = max(0.0, second - mean * mean)
        corner_costs[mask] = fsum(root.event_cost for root in roots) / len(roots)

    return InteractionObjective(
        lower=lower_tuple,
        upper=upper_tuple,
        corner_variances=corner_variances,
        corner_costs=corner_costs,
    )

