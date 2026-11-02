"""Deterministic global integer solver for the full-interaction objective."""

from __future__ import annotations

from dataclasses import dataclass
import heapq
from itertools import count
from itertools import product
from math import exp, floor, ceil, log, prod
from typing import Sequence

from .objective import InteractionObjective, PlanEvaluation


@dataclass(frozen=True)
class GlobalPlanResult:
    evaluation: PlanEvaluation
    boxes_visited: int
    plans_evaluated: int
    pruned_boxes: int
    certified_global: bool


def _box_size(lower: Sequence[int], upper: Sequence[int]) -> int:
    return prod(hi - lo + 1 for lo, hi in zip(lower, upper))


def _dc_box_lower_bound(
    objective: InteractionObjective,
    lower: Sequence[int],
    upper: Sequence[int],
) -> tuple[float, tuple[float, ...]]:
    """Rigorous DC lower bound plus a continuous minimizer candidate."""
    terms = objective.objective_exponential_terms()
    positive = [(exponents, coefficient) for exponents, coefficient in terms.items() if coefficient > 0.0]
    negative = [(exponents, -coefficient) for exponents, coefficient in terms.items() if coefficient < 0.0]
    y_lower = tuple(log(value) for value in lower)
    y_upper = tuple(log(value) for value in upper)
    initial = tuple(0.5 * (a + b) for a, b in zip(y_lower, y_upper))

    def positive_value_gradient(y: Sequence[float]) -> tuple[float, list[float]]:
        value = 0.0
        gradient = [0.0] * objective.dimension
        for exponents, coefficient in positive:
            term = coefficient * exp(sum(power * coordinate for power, coordinate in zip(exponents, y)))
            value += term
            for level, power in enumerate(exponents):
                gradient[level] += power * term
        return value, gradient

    if positive:
        # Projected gradient with Armijo backtracking only seeks a tight
        # tangent point.  The tangent lower bound below is rigorous at every
        # point, so solver convergence is not required for correctness.
        point = initial
        value, gradient = positive_value_gradient(point)
        for _ in range(8):
            squared_norm = sum(component * component for component in gradient)
            if squared_norm <= 1.0e-24:
                break
            step = 1.0
            accepted = False
            while step >= 1.0e-12:
                candidate = tuple(
                    max(y_lower[level], min(y_upper[level], point[level] - step * gradient[level]))
                    for level in range(objective.dimension)
                )
                candidate_value, candidate_gradient = positive_value_gradient(candidate)
                displacement_dot_gradient = sum(
                    gradient[level] * (candidate[level] - point[level])
                    for level in range(objective.dimension)
                )
                if candidate_value <= value + 1.0e-4 * displacement_dot_gradient:
                    point = candidate
                    value = candidate_value
                    gradient = candidate_gradient
                    accepted = True
                    break
                step *= 0.5
            if not accepted:
                break
        value, gradient = positive_value_gradient(point)
        # A tangent to a convex function is a global lower bound.  Minimize
        # that affine tangent analytically over this log-population box.
        positive_lower = value + sum(
            derivative * ((y_lower[level] if derivative >= 0.0 else y_upper[level]) - point[level])
            for level, derivative in enumerate(gradient)
        )
    else:
        point = initial
        positive_lower = 0.0

    # Each negative component is convex and nonnegative.  Summing its
    # componentwise box maxima is a safe upper bound, hence subtraction gives
    # a rigorous lower bound for the signed exponential polynomial.
    negative_upper = 0.0
    for exponents, coefficient in negative:
        exponent_max = sum(
            power * (y_upper[level] if power >= 0 else y_lower[level])
            for level, power in enumerate(exponents)
        )
        negative_upper += coefficient * exp(exponent_max)
    return positive_lower - negative_upper, point


def _rounded_candidates(
    point: Sequence[float], lower: Sequence[int], upper: Sequence[int]
) -> tuple[tuple[int, ...], ...]:
    choices: list[tuple[int, ...]] = []
    for coordinate, lo, hi in zip(point, lower, upper):
        raw = exp(coordinate)
        values = sorted({max(lo, min(hi, int(floor(raw)))), max(lo, min(hi, int(ceil(raw))))})
        choices.append(tuple(values))
    return tuple(tuple(plan) for plan in product(*choices))


def solve_global_plan(
    objective: InteractionObjective,
    *,
    incumbent: Sequence[int] | None = None,
    tolerance: float = 1.0e-15,
) -> GlobalPlanResult:
    """Globally minimize over every integer population vector in the box.

    Branch-and-bound uses a valid multilinear variance lower bound and the
    monotone additive-cost lower bound.  It never imposes a trust region or a
    fixed number of changed layers.  A returned result is globally certified
    for the supplied interaction surface.
    """

    if tolerance < 0.0:
        raise ValueError("tolerance must be nonnegative")
    initial_plan = tuple(incumbent) if incumbent is not None else objective.lower
    best = objective.evaluate(initial_plan)
    plans_evaluated = 1
    boxes_visited = 0
    pruned_boxes = 0
    serial = count()
    root_lower = objective.lower
    root_upper = objective.upper
    root_bound, root_point = _dc_box_lower_bound(objective, root_lower, root_upper)
    for candidate_plan in _rounded_candidates(root_point, root_lower, root_upper):
        candidate = objective.evaluate(candidate_plan)
        plans_evaluated += 1
        if (candidate.objective, candidate.plan) < (best.objective, best.plan):
            best = candidate
    queue: list[tuple[float, int, tuple[int, ...], tuple[int, ...]]] = [
        (root_bound, next(serial), root_lower, root_upper)
    ]

    while queue:
        bound, _, lower, upper = heapq.heappop(queue)
        boxes_visited += 1
        if bound >= best.objective - tolerance:
            pruned_boxes += 1
            continue
        size = _box_size(lower, upper)
        if size == 1:
            candidate = objective.evaluate(lower)
            plans_evaluated += 1
            if (candidate.objective, candidate.plan) < (best.objective, best.plan):
                best = candidate
            continue

        widths = [hi - lo for lo, hi in zip(lower, upper)]
        split_level = max(range(objective.dimension), key=lambda level: (widths[level], -level))
        middle = (lower[split_level] + upper[split_level]) // 2
        children: list[tuple[tuple[int, ...], tuple[int, ...]]] = []
        left_upper = list(upper)
        left_upper[split_level] = middle
        children.append((lower, tuple(left_upper)))
        right_lower = list(lower)
        right_lower[split_level] = middle + 1
        children.append((tuple(right_lower), upper))
        for child_lower, child_upper in children:
            child_bound, child_point = _dc_box_lower_bound(objective, child_lower, child_upper)
            for candidate_plan in _rounded_candidates(child_point, child_lower, child_upper):
                candidate = objective.evaluate(candidate_plan)
                plans_evaluated += 1
                if (candidate.objective, candidate.plan) < (best.objective, best.plan):
                    best = candidate
            if child_bound >= best.objective - tolerance:
                pruned_boxes += 1
                continue
            heapq.heappush(
                queue,
                (child_bound, next(serial), child_lower, child_upper),
            )

    return GlobalPlanResult(
        evaluation=best,
        boxes_visited=boxes_visited,
        plans_evaluated=plans_evaluated,
        pruned_boxes=pruned_boxes,
        certified_global=True,
    )
