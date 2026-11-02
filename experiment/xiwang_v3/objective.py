"""Full layer-interaction objective for within-run SFE population planning.

The variance surface is represented by its values at all corners of a box in
``x_l = 1 / n_l``.  Multilinear interpolation is not an additive layer model:
it contains every interaction from order two through order ``L``.  For the
standard particle-pair/coalescent second-moment recursion this representation
is exact.  For the empirical modal-SFE transition kernel the corner values are
sample-average estimates collected from formal roots.
"""

from __future__ import annotations

from dataclasses import dataclass
from itertools import product
from math import isfinite
from typing import Iterable, Mapping, Sequence


Plan = tuple[int, ...]


@dataclass(frozen=True)
class PlanEvaluation:
    plan: Plan
    variance: float
    expected_cost: float
    objective: float


class InteractionObjective:
    """Multilinear full-interaction variance and expected-cost surfaces.

    ``corner_variances[mask]`` is the variance at a population-box corner.
    Bit ``l`` equal to zero selects ``lower[l]`` and one selects ``upper[l]``.
    The interpolation coordinate is reciprocal population, matching the exact
    particle-pair expansion in coalescence probabilities.
    """

    def __init__(
        self,
        *,
        lower: Sequence[int],
        upper: Sequence[int],
        corner_variances: Mapping[int, float],
        corner_costs: Mapping[int, float] | None = None,
        base_cost: float = 0.0,
        cost_per_trajectory: Sequence[float] | None = None,
    ) -> None:
        self.lower = tuple(int(value) for value in lower)
        self.upper = tuple(int(value) for value in upper)
        self.dimension = len(self.lower)
        if self.dimension == 0 or len(self.upper) != self.dimension:
            raise ValueError("population bounds must have equal nonzero dimension")
        if any(lo <= 0 or hi < lo for lo, hi in zip(self.lower, self.upper)):
            raise ValueError("invalid population bounds")
        expected_masks = set(range(1 << self.dimension))
        if set(corner_variances) != expected_masks:
            raise ValueError("all 2^L corner variances are required")
        self.corner_variances = {
            int(mask): float(value) for mask, value in corner_variances.items()
        }
        if any(not isfinite(value) or value < 0.0 for value in self.corner_variances.values()):
            raise ValueError("corner variances must be finite and nonnegative")
        self.corner_costs = None if corner_costs is None else {
            int(mask): float(value) for mask, value in corner_costs.items()
        }
        if self.corner_costs is not None:
            if set(self.corner_costs) != expected_masks:
                raise ValueError("all 2^L corner costs are required")
            if any(not isfinite(value) or value < 0.0 for value in self.corner_costs.values()):
                raise ValueError("corner costs must be finite and nonnegative")
        self.base_cost = float(base_cost)
        coefficients = (0.0,) * self.dimension if cost_per_trajectory is None else cost_per_trajectory
        self.cost_per_trajectory = tuple(float(value) for value in coefficients)
        if len(self.cost_per_trajectory) != self.dimension:
            raise ValueError("one cost coefficient is required per level")
        if not isfinite(self.base_cost) or self.base_cost < 0.0:
            raise ValueError("base_cost must be finite and nonnegative")
        if any(not isfinite(value) or value < 0.0 for value in self.cost_per_trajectory):
            raise ValueError("cost coefficients must be finite and nonnegative")

        self._x_low = tuple(1.0 / hi for hi in self.upper)
        self._x_high = tuple(1.0 / lo for lo in self.lower)

    def _validate_plan(self, plan: Sequence[int]) -> Plan:
        result = tuple(int(value) for value in plan)
        if len(result) != self.dimension:
            raise ValueError("plan dimension mismatch")
        if any(value < lo or value > hi for value, lo, hi in zip(result, self.lower, self.upper)):
            raise ValueError("plan lies outside population bounds")
        return result

    def reciprocal_coordinates(self, plan: Sequence[int]) -> tuple[float, ...]:
        plan_tuple = self._validate_plan(plan)
        coordinates: list[float] = []
        for level, population in enumerate(plan_tuple):
            span = self._x_high[level] - self._x_low[level]
            if span == 0.0:
                coordinates.append(0.0)
            else:
                # Coordinate zero is the lower-population corner and one is
                # the upper-population corner.  Because x=1/n, its direction
                # is opposite to population.
                coordinates.append((self._x_high[level] - 1.0 / population) / span)
        return tuple(coordinates)

    def variance(self, plan: Sequence[int]) -> float:
        t = self.reciprocal_coordinates(plan)
        return self._interpolate(self.corner_variances, t)

    def _interpolate(
        self, corners: Mapping[int, float], coordinates: Sequence[float]
    ) -> float:
        value = 0.0
        for mask, corner in corners.items():
            weight = 1.0
            for level, coordinate in enumerate(coordinates):
                weight *= coordinate if mask & (1 << level) else 1.0 - coordinate
            value += weight * corner
        # Roundoff can produce a tiny negative number even though every corner
        # and every interpolation weight is nonnegative.
        return max(0.0, value)

    def expected_cost(self, plan: Sequence[int]) -> float:
        plan_tuple = self._validate_plan(plan)
        if self.corner_costs is not None:
            return self._interpolate(self.corner_costs, self.reciprocal_coordinates(plan_tuple))
        return self.base_cost + sum(
            coefficient * population
            for coefficient, population in zip(self.cost_per_trajectory, plan_tuple)
        )

    def evaluate(self, plan: Sequence[int]) -> PlanEvaluation:
        plan_tuple = self._validate_plan(plan)
        variance = self.variance(plan_tuple)
        cost = self.expected_cost(plan_tuple)
        return PlanEvaluation(plan_tuple, variance, cost, variance * cost)

    def gradient_x(self, plan: Sequence[int]) -> tuple[float, ...]:
        """Exact gradient of the interaction variance with respect to x=1/n."""
        t = self.reciprocal_coordinates(plan)
        gradient: list[float] = []
        for differentiated in range(self.dimension):
            span = self._x_high[differentiated] - self._x_low[differentiated]
            if span == 0.0:
                gradient.append(0.0)
                continue
            derivative = 0.0
            for mask, corner in self.corner_variances.items():
                weight = 1.0 if mask & (1 << differentiated) else -1.0
                for level, coordinate in enumerate(t):
                    if level == differentiated:
                        continue
                    weight *= coordinate if mask & (1 << level) else 1.0 - coordinate
                derivative -= corner * weight / span
            gradient.append(derivative)
        return tuple(gradient)

    def hessian_x(self, plan: Sequence[int]) -> tuple[tuple[float, ...], ...]:
        """Exact cross-layer Hessian; diagonal entries are zero for multilinear V."""
        t = self.reciprocal_coordinates(plan)
        matrix = [[0.0 for _ in range(self.dimension)] for _ in range(self.dimension)]
        for first in range(self.dimension):
            for second in range(first + 1, self.dimension):
                span_first = self._x_high[first] - self._x_low[first]
                span_second = self._x_high[second] - self._x_low[second]
                if span_first == 0.0 or span_second == 0.0:
                    continue
                derivative = 0.0
                for mask, corner in self.corner_variances.items():
                    sign_first = 1.0 if mask & (1 << first) else -1.0
                    sign_second = 1.0 if mask & (1 << second) else -1.0
                    weight = sign_first * sign_second
                    for level, coordinate in enumerate(t):
                        if level in (first, second):
                            continue
                        weight *= coordinate if mask & (1 << level) else 1.0 - coordinate
                    derivative += corner * weight / (span_first * span_second)
                matrix[first][second] = derivative
                matrix[second][first] = derivative
        return tuple(tuple(row) for row in matrix)

    def box_variance_lower_bound(
        self, lower: Sequence[int], upper: Sequence[int]
    ) -> float:
        """Exact continuous-box minimum of multilinear V, attained at a corner."""
        lo = self._validate_plan(lower)
        hi = self._validate_plan(upper)
        if any(a > b for a, b in zip(lo, hi)):
            raise ValueError("invalid sub-box")
        minimum = float("inf")
        for bits in product((0, 1), repeat=self.dimension):
            corner = tuple(hi[level] if bits[level] else lo[level] for level in range(self.dimension))
            minimum = min(minimum, self.variance(corner))
        return max(0.0, minimum)

    def box_objective_lower_bound(
        self, lower: Sequence[int], upper: Sequence[int]
    ) -> float:
        lo = self._validate_plan(lower)
        variance_lower = self.box_variance_lower_bound(lo, upper)
        if self.corner_costs is None:
            # Additive costs are nonnegative and monotone in population.
            cost_lower = self.expected_cost(lo)
        else:
            hi = self._validate_plan(upper)
            cost_lower = min(
                self.expected_cost(
                    tuple(hi[level] if bits[level] else lo[level] for level in range(self.dimension))
                )
                for bits in product((0, 1), repeat=self.dimension)
            )
        return variance_lower * max(0.0, cost_lower)

    def _surface_monomials_x(self, corners: Mapping[int, float]) -> dict[int, float]:
        """Return coefficients of sum c_S prod_{l in S} x_l."""
        coefficients = {mask: float(value) for mask, value in corners.items()}
        for level in range(self.dimension):
            bit = 1 << level
            x_at_zero = self._x_high[level]  # lower population
            x_at_one = self._x_low[level]    # upper population
            denominator = x_at_one - x_at_zero
            if denominator == 0.0:
                for mask in range(1 << self.dimension):
                    if not mask & bit:
                        coefficients[mask | bit] = 0.0
                continue
            for mask in range(1 << self.dimension):
                if mask & bit:
                    continue
                value_zero = coefficients[mask]
                value_one = coefficients[mask | bit]
                slope = (value_one - value_zero) / denominator
                intercept = value_zero - slope * x_at_zero
                coefficients[mask] = intercept
                coefficients[mask | bit] = slope
        scale = max(1.0, max(abs(value) for value in coefficients.values()))
        return {
            mask: value
            for mask, value in coefficients.items()
            if abs(value) > 1.0e-13 * scale
        }

    def objective_exponential_terms(self) -> dict[tuple[int, ...], float]:
        """Represent J as sum c_a exp(a^T log(n)).

        This signed exponential-polynomial form supports a rigorous
        difference-of-convex lower bound in the global solver.
        """
        variance_terms = self._surface_monomials_x(self.corner_variances)
        result: dict[tuple[int, ...], float] = {}

        def add(exponents: tuple[int, ...], coefficient: float) -> None:
            result[exponents] = result.get(exponents, 0.0) + coefficient

        if self.corner_costs is not None:
            cost_terms = self._surface_monomials_x(self.corner_costs)
            for variance_mask, variance_coefficient in variance_terms.items():
                for cost_mask, cost_coefficient in cost_terms.items():
                    exponents = tuple(
                        -int(bool(variance_mask & (1 << level)))
                        -int(bool(cost_mask & (1 << level)))
                        for level in range(self.dimension)
                    )
                    add(exponents, variance_coefficient * cost_coefficient)
        else:
            for variance_mask, variance_coefficient in variance_terms.items():
                base_exponents = tuple(
                    -int(bool(variance_mask & (1 << level)))
                    for level in range(self.dimension)
                )
                add(base_exponents, variance_coefficient * self.base_cost)
                for cost_level, cost_coefficient in enumerate(self.cost_per_trajectory):
                    exponents = list(base_exponents)
                    exponents[cost_level] += 1
                    add(tuple(exponents), variance_coefficient * cost_coefficient)
        scale = max(1.0, max((abs(value) for value in result.values()), default=1.0))
        return {
            exponents: value
            for exponents, value in result.items()
            if abs(value) > 1.0e-12 * scale
        }
