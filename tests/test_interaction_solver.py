from __future__ import annotations

from itertools import product
import unittest

from xiwang_v3.objective import InteractionObjective
from xiwang_v3.solver import solve_global_plan


class FullInteractionObjectiveTests(unittest.TestCase):
    def test_cross_hessian_detects_layer_interaction(self) -> None:
        lower = (2, 2)
        upper = (5, 5)

        def variance(plan: tuple[int, int]) -> float:
            x0, x1 = (1.0 / plan[0], 1.0 / plan[1])
            return 0.2 + 3.0 * x0 + 2.0 * x1 + 7.0 * x0 * x1

        corners = {
            mask: variance(tuple(upper[level] if mask & (1 << level) else lower[level] for level in range(2)))
            for mask in range(4)
        }
        objective = InteractionObjective(
            lower=lower,
            upper=upper,
            corner_variances=corners,
            base_cost=1.0,
            cost_per_trajectory=(0.1, 0.1),
        )
        self.assertAlmostEqual(objective.variance((3, 4)), variance((3, 4)), places=12)
        hessian = objective.hessian_x((3, 4))
        self.assertAlmostEqual(hessian[0][1], 7.0, places=10)
        self.assertAlmostEqual(hessian[1][0], 7.0, places=10)

    def test_global_solver_matches_exhaustive_search(self) -> None:
        lower = (2, 2, 2)
        upper = (7, 6, 5)

        def variance(plan: tuple[int, int, int]) -> float:
            x0, x1, x2 = (1.0 / value for value in plan)
            return (
                0.03
                + 1.5 * x0
                + 0.9 * x1
                + 1.2 * x2
                + 2.0 * x0 * x1
                + 1.1 * x1 * x2
                + 4.0 * x0 * x1 * x2
            )

        corners = {
            mask: variance(tuple(upper[level] if mask & (1 << level) else lower[level] for level in range(3)))
            for mask in range(8)
        }
        objective = InteractionObjective(
            lower=lower,
            upper=upper,
            corner_variances=corners,
            base_cost=0.5,
            cost_per_trajectory=(0.4, 0.8, 0.2),
        )
        expected = min(
            (objective.evaluate(plan) for plan in product(*(range(lo, hi + 1) for lo, hi in zip(lower, upper)))),
            key=lambda item: (item.objective, item.plan),
        )
        actual = solve_global_plan(objective)
        self.assertTrue(actual.certified_global)
        self.assertEqual(actual.evaluation.plan, expected.plan)
        self.assertAlmostEqual(actual.evaluation.objective, expected.objective, places=14)

    def test_solver_may_change_any_number_of_levels(self) -> None:
        lower = (2, 2, 2, 2)
        upper = (8, 8, 8, 8)

        def variance(plan: tuple[int, ...]) -> float:
            x = [1.0 / value for value in plan]
            return 0.001 + sum(20.0 * value for value in x) + 15.0 * x[0] * x[3]

        corners = {
            mask: variance(tuple(upper[level] if mask & (1 << level) else lower[level] for level in range(4)))
            for mask in range(16)
        }
        objective = InteractionObjective(
            lower=lower,
            upper=upper,
            corner_variances=corners,
            base_cost=1.0,
            cost_per_trajectory=(0.01, 0.01, 0.01, 0.01),
        )
        result = solve_global_plan(objective)
        changed = sum(a != b for a, b in zip(result.evaluation.plan, lower))
        self.assertEqual(changed, 4)

    def test_cost_surface_can_also_contain_interactions(self) -> None:
        lower = (2, 2)
        upper = (6, 6)

        def variance(plan: tuple[int, int]) -> float:
            return 1.0 / plan[0] + 1.0 / plan[1]

        def cost(plan: tuple[int, int]) -> float:
            x0, x1 = 1.0 / plan[0], 1.0 / plan[1]
            return 10.0 - 2.0 * x0 + 3.0 * x1 + 5.0 * x0 * x1


        corner_plans = {
            mask: tuple(upper[level] if mask & (1 << level) else lower[level] for level in range(2))
            for mask in range(4)
        }
        objective = InteractionObjective(
            lower=lower,
            upper=upper,
            corner_variances={mask: variance(plan) for mask, plan in corner_plans.items()},
            corner_costs={mask: cost(plan) for mask, plan in corner_plans.items()},
        )
        self.assertAlmostEqual(objective.expected_cost((3, 4)), cost((3, 4)), places=12)

    def test_exponential_terms_reconstruct_objective(self) -> None:
        lower = (2, 3)
        upper = (7, 8)

        def variance(plan):
            x0, x1 = 1.0 / plan[0], 1.0 / plan[1]
            return 0.1 + 2.0 * x0 + 3.0 * x1 + 4.0 * x0 * x1

        corners = {
            mask: variance(tuple(upper[level] if mask & (1 << level) else lower[level] for level in range(2)))
            for mask in range(4)
        }
        objective = InteractionObjective(
            lower=lower,
            upper=upper,
            corner_variances=corners,
            base_cost=1.5,
            cost_per_trajectory=(0.3, 0.7),
        )
        terms = objective.objective_exponential_terms()
        for plan in product(range(2, 8), range(3, 9)):
            reconstructed = sum(
                coefficient * __import__("math").exp(
                    sum(power * __import__("math").log(value) for power, value in zip(exponents, plan))
                )
                for exponents, coefficient in terms.items()
            )
            self.assertAlmostEqual(reconstructed, objective.evaluate(plan).objective, places=10)


if __name__ == "__main__":
    unittest.main()
