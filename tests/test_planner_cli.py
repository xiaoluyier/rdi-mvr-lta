from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from xiwang_v3.planner_cli import main, plan_request


class PlannerCliTests(unittest.TestCase):
    def test_empty_levels_are_identity_without_reading_transitions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            request = Path(directory) / "request.json"
            request.write_text(json.dumps({
                "batch_index": 1,
                "levels": [],
                "lower": [],
                "upper": [],
                "initial_level": 0,
                "initial_parent_signature": "root",
                "rollout_count": 32,
                "transitions_file": "missing.jsonl",
            }))
            result = plan_request(request)
            self.assertEqual(result["status"], "XIWANG_NOT_APPLICABLE_IDENTITY")
            self.assertEqual(result["plan"], {})

    def test_end_to_end_request_writes_certified_plan(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            transitions = root / "transitions.jsonl"
            rows = [
                {"current_level": 0, "parent_signature": "root", "child_signature": "a", "reached_level": 1, "rare": False, "event_cost": 1},
                {"current_level": 0, "parent_signature": "root", "child_signature": "d", "reached_level": 0, "rare": False, "event_cost": 1},
                {"current_level": 1, "parent_signature": "a", "child_signature": "r", "reached_level": 1, "rare": True, "event_cost": 2},
                {"current_level": 1, "parent_signature": "a", "child_signature": "d", "reached_level": 1, "rare": False, "event_cost": 1},
            ]
            transitions.write_text("".join(json.dumps(row) + "\n" for row in rows))
            request = root / "request.json"
            request.write_text(json.dumps({
                "batch_index": 1,
                "levels": [0, 1],
                "lower": [2, 2],
                "upper": [4, 4],
                "initial_level": 0,
                "initial_parent_signature": "root",
                "rollout_count": 64,
                "transitions_file": "transitions.jsonl",
                "default_population": 2,
                "current_plan": {"0": 2, "1": 2},
            }))
            output = root / "plan.json"
            plan_output = root / "plan.txt"
            self.assertEqual(main([
                "--request", str(request), "--output", str(output),
                "--plan-output", str(plan_output),
            ]), 0)
            result = json.loads(output.read_text())
            self.assertEqual(result["status"], "OPTIMAL")
            self.assertTrue(result["certified_global"])
            self.assertEqual(result["transition_count"], 4)
            self.assertNotEqual(plan_output.read_text().strip(), "ERROR")


if __name__ == "__main__":
    unittest.main()
