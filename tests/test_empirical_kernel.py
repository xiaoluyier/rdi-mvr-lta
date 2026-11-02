from __future__ import annotations

import unittest

from xiwang_v3.empirical_kernel import (
    ChildTransition,
    EmpiricalModalKernel,
    estimate_interaction_objective,
)


def transition(level, parent, child, reached, rare=False, cost=1.0):
    return ChildTransition(level, parent, child, reached, rare, cost)


class EmpiricalModalKernelTests(unittest.TestCase):
    def test_modal_tie_chooses_smallest_reached_level(self) -> None:
        # With four deterministic indexed samples, two reach level 1 and two
        # level 2.  The legacy std::map/max_element behavior chooses level 1.
        kernel = EmpiricalModalKernel(
            [
                transition(0, "root", "a", 1),
                transition(0, "root", "b", 2),
                transition(0, "root", "c", 1),
                transition(0, "root", "d", 2),
                transition(1, "a", "z", 1, rare=True),
                transition(1, "c", "z", 1, rare=True),
            ]
        )
        # Find a seed whose four empirical draws tie; deterministic search is
        # test-only and does not affect production planning.
        tied = None
        for seed in range(10000):
            draws = [
                kernel.sample(level=0, parent_signature="root", seed=seed, step=0, child_index=i)
                for i in range(4)
            ]
            if sorted(item.reached_level for item in draws) == [1, 1, 2, 2]:
                tied = seed
                break
        self.assertIsNotNone(tied)
        result = kernel.simulate_root(
            {0: 4, 1: 4, 2: 4},
            seed=tied,
            initial_level=0,
            initial_parent_signature="root",
            max_steps=3,
        )
        self.assertEqual(result.path[:2], (0, 1))

    def test_round_robin_parent_replication_is_preserved(self) -> None:
        kernel = EmpiricalModalKernel(
            [
                transition(0, "root", "a", 1),
                transition(1, "a", "done", 1, rare=True, cost=2.0),
            ]
        )
        result = kernel.simulate_root(
            {0: 3, 1: 5},
            seed=7,
            initial_level=0,
            initial_parent_signature="root",
        )
        self.assertEqual(result.estimate, 1.0)
        self.assertEqual(result.event_cost, 3.0 + 10.0)
        self.assertEqual(result.path, (0, 1))

    def test_corner_estimator_builds_variance_and_cost_interactions(self) -> None:
        kernel = EmpiricalModalKernel(
            [
                transition(0, "root", "a", 1, cost=1.0),
                transition(0, "root", "dead", 0, cost=2.0),
                transition(1, "a", "rare", 1, rare=True, cost=3.0),
                transition(1, "a", "dead", 1, rare=False, cost=1.0),
            ]
        )
        objective = estimate_interaction_objective(
            kernel,
            levels=(0, 1),
            lower=(2, 2),
            upper=(4, 4),
            rollout_seeds=tuple(range(128)),
            initial_level=0,
            initial_parent_signature="root",
        )
        evaluation = objective.evaluate((3, 3))
        self.assertGreaterEqual(evaluation.variance, 0.0)
        self.assertGreater(evaluation.expected_cost, 0.0)
        self.assertEqual(len(objective.corner_variances), 4)
        self.assertEqual(len(objective.corner_costs or {}), 4)


if __name__ == "__main__":
    unittest.main()
