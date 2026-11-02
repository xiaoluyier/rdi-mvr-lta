import json
import tempfile
import unittest
from pathlib import Path

from xiwang_v3.formal42 import build_formal_package


ROOT = Path(__file__).resolve().parents[2]


class Formal42V3Tests(unittest.TestCase):
    def test_builds_v1_warm_started_package_with_one_plan_per_controller(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            package = build_formal_package(
                source_v2_package=ROOT / "XIWANG_V2_FORMAL_42_20260831",
                output_dir=output,
                server_ids=("646", "641"),
                warm_start_plan_dirs=(
                    ROOT / "results" / "xiwang_pure_formal_44_clean_20260831" /
                    "extracted_646" / "frozen_plans",
                    ROOT / "results" / "xiwang_pure_formal_44_clean_20260831" /
                    "extracted_647" / "frozen_plans",
                ),
            )
            self.assertEqual(package.controller_count, 84)
            self.assertTrue(all(
                item["initialization"] == "V1_FROZEN_PLAN"
                and item["initial_full_plan"]
                for item in package.controllers
            ))
            self.assertEqual(len(list((output / "v1_frozen_plans").glob("*.json"))), 84)
            preflight = json.loads(
                (output / "preflight_report.json").read_text(encoding="utf-8")
            )
            self.assertEqual(preflight["INITIALIZATION"], "V1_FROZEN_PLAN")
            self.assertEqual(preflight["V1_FROZEN_PLANS"], 84)
            self.assertEqual(preflight["FIRST_BATCH_ALL_24"], 0)

    def test_builds_closed_sequential_package_without_learning_or_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            package = build_formal_package(
                source_v2_package=ROOT / "XIWANG_V2_FORMAL_42_20260831",
                output_dir=Path(directory),
                server_ids=("646", "641"),
            )
            self.assertEqual(package.model_count, 44)
            self.assertEqual(package.active_model_count, 42)
            self.assertEqual(package.controller_count, 84)
            self.assertEqual(package.not_applicable_controller_count, 4)
            self.assertEqual(package.formal_batch_count, 1680)
            self.assertEqual(package.server_counts, {"646": 840, "641": 840})

            output = Path(directory)
            self.assertFalse((output / "learning_controllers.jsonl").exists())
            self.assertFalse((output / "confirmation_tasks.jsonl").exists())
            self.assertFalse((output / "candidate_validations").exists())
            self.assertTrue((output / "xiwang_v3" / "formal_runner.py").is_file())
            self.assertTrue((output / "MANIFEST_SHA256.json").is_file())

            controllers = package.controllers
            self.assertTrue(all(len(item["formal_batches"]) == 20 for item in controllers))
            self.assertTrue(all(
                [batch["batch_index"] for batch in item["formal_batches"]] == list(range(1, 21))
                for item in controllers
            ))
            self.assertTrue(all(
                batch["method"].endswith("_XIWANG_V3")
                for item in controllers for batch in item["formal_batches"]
            ))
            self.assertFalse(any(
                key in item for item in controllers
                for key in ("learning_seeds", "validation_seeds", "confirmation_seeds")
            ))
            preflight = json.loads((output / "preflight_report.json").read_text(encoding="utf-8"))
            self.assertEqual(preflight["INTERNAL_64_ROOT_UPDATES"], 0)
            self.assertEqual(preflight["EXTRA_LEARNING_RUNS"], 0)
            self.assertEqual(preflight["EXTRA_VALIDATION_RUNS"], 0)
            self.assertEqual(preflight["PLANNING_LOCATION"], "BETWEEN_120S_FORMAL_BATCHES")


if __name__ == "__main__":
    unittest.main()
