import json
import tempfile
import unittest
from pathlib import Path

from xiwang_v3.formal_runner import (
    FigRunArtifact,
    FormalSequentialRunner,
)


def write_jsonl(path: Path, rows) -> None:
    path.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows),
        encoding="utf-8",
    )


class FakeBackend:
    def __init__(self) -> None:
        self.calls = []

    def run(self, spec):
        self.calls.append(spec)
        lines = []
        for root in range(2):
            for step, level in enumerate((0, 5)):
                effort = spec.sparse_plan.get(level, 24)
                successes = effort // (2 if level == 0 else 4)
                lines.append(
                    f"SFE_LEVEL sweep_step={step} level={level} "
                    f"effort={effort} successes={successes}"
                )
        return FigRunArtifact(
            diagnostic_text="\n".join(lines),
            metrics={"estimate": 0.01, "rhw": 0.2, "root_sweeps": 100},
        )


class SequentialFormalRunnerTests(unittest.TestCase):
    def test_first_batch_uses_explicit_fixed_24_then_complete_xiwang_plan(self):
        from xiwang_v3.formal_runner import FigRunSpec, population_environment

        first = FigRunSpec(
            run_id="first", phase="SEQUENTIAL_FORMAL", model_path="m.ft",
            ifun="STR", seed=1, thresholds_adhoc="1:5", sparse_plan={},
            stop_time=120, fixed_population_override=24,
        )
        self.assertEqual(population_environment(first), {"BATS_LEVEL_POPULATION": "24"})

        later = FigRunSpec(
            run_id="later", phase="SEQUENTIAL_FORMAL", model_path="m.ft",
            ifun="STR", seed=2, thresholds_adhoc="1:5", sparse_plan={2: 25},
            stop_time=120, full_population_plan={1: 24, 2: 25, 3: 24},
        )
        self.assertEqual(
            population_environment(later),
            {"BATS_XIWANG_PLAN": "1:24,2:25,3:24"},
        )

    def test_controller_status_paths_are_unique_for_replicate_chains(self):
        from xiwang_v3.formal_runner import controller_status_filename

        self.assertNotEqual(
            controller_status_filename("L01_C02__STR_MVR"),
            controller_status_filename("L01_C03__STR_MVR"),
        )

    def test_v1_warm_start_is_used_by_first_formal_batch_and_survives_resume(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "models").mkdir()
            (root / "models" / "M.ft").write_text("model", encoding="utf-8")
            controller = {
                "controller_id": "M|STR",
                "model_id": "M",
                "ifun": "STR",
                "server": "646",
                "model_path": "models/M.ft",
                "initialization": "V1_FROZEN_PLAN",
                "initial_full_plan": {"0": 24, "5": 26},
                "optimizer": {
                    "fixed_population_reference": 24,
                    "trust_region_policy": "max(1,current_observed_level_count)",
                    "eta": 0.0,
                    "max_population": 28,
                    "patience": 5,
                    "max_batches": 2,
                    "bootstrap_replicates": 20,
                    "bootstrap_seed": 9,
                    "min_clusters": 2,
                },
                "formal_batches": [
                    {
                        "task_id": f"warm-{seed}", "batch_index": index,
                        "controller_id": "M|STR", "model_id": "M", "ifun": "STR",
                        "method": "STR_XIWANG_V3_WARM", "server": "646",
                        "seed": seed, "model_path": "models/M.ft",
                        "thresholds_adhoc": "1:5,2:5", "stop_time": 120,
                        "mission_time": 30, "global_effort": 5,
                    }
                    for index, seed in enumerate((21, 22), start=1)
                ],
            }
            write_jsonl(root / "server_646_controllers.jsonl", [controller])
            first_backend = FakeBackend()
            FormalSequentialRunner(root, "646", first_backend, parallel_jobs=1).run_all()
            self.assertEqual(first_backend.calls[0].sparse_plan, {5: 26})

            first_result = json.loads(
                (root / "results" / "warm-21.json").read_text(encoding="utf-8")
            )
            self.assertEqual(first_result["initialization"], "V1_FROZEN_PLAN")
            self.assertEqual(first_result["protocol"], "XIWANG_V3_WARM_SEQUENTIAL_FORMAL")

            resumed_backend = FakeBackend()
            resumed = FormalSequentialRunner(
                root, "646", resumed_backend, parallel_jobs=1,
            ).run_all()
            self.assertEqual(resumed_backend.calls, [])
            self.assertEqual(resumed["skipped_formal_batches"], 2)

    def test_formal_batches_run_in_seed_order_and_plan_only_between_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "models").mkdir()
            (root / "models" / "M.ft").write_text("model", encoding="utf-8")
            controller = {
                "controller_id": "M|STR",
                "model_id": "M",
                "ifun": "STR",
                "server": "646",
                "model_path": "models/M.ft",
                "optimizer": {
                    "fixed_population_reference": 24,
                    "trust_region_policy": "max(1,current_observed_level_count)",
                    "eta": 0.0,
                    "max_population": 26,
                    "patience": 5,
                    "max_batches": 2,
                    "bootstrap_replicates": 20,
                    "bootstrap_seed": 9,
                    "min_clusters": 2,
                },
                "formal_batches": [
                    {
                        "task_id": f"task-{seed}",
                        "batch_index": index,
                        "controller_id": "M|STR",
                        "model_id": "M",
                        "ifun": "STR",
                        "method": "STR_XIWANG_V3",
                        "server": "646",
                        "seed": seed,
                        "model_path": "models/M.ft",
                        "thresholds_adhoc": "1:5,2:5",
                        "stop_time": 120,
                        "mission_time": 30,
                        "global_effort": 5,
                    }
                    for index, seed in enumerate((11, 12, 13), start=1)
                ],
            }
            write_jsonl(root / "server_646_controllers.jsonl", [controller])
            backend = FakeBackend()
            report = FormalSequentialRunner(
                root, "646", backend, parallel_jobs=16,
            ).run_all()

            self.assertEqual([call.seed for call in backend.calls], [11, 12, 13])
            self.assertTrue(all(call.stop_time == 120 for call in backend.calls))
            self.assertTrue(all(call.phase == "SEQUENTIAL_FORMAL" for call in backend.calls))
            self.assertEqual(backend.calls[0].sparse_plan, {})
            self.assertNotEqual(backend.calls[1].sparse_plan, {})
            self.assertEqual(report["formal_batches"], 3)
            self.assertEqual(report["completed_formal_batches"], 3)

            rows = [
                json.loads(path.read_text(encoding="utf-8"))
                for path in sorted((root / "results").glob("*.json"))
            ]
            self.assertEqual({row["protocol"] for row in rows}, {"XIWANG_V3_SEQUENTIAL_FORMAL"})
            self.assertTrue(all(row["formal_data_included"] for row in rows))
            self.assertTrue(all(row["planning_seconds_after_run"] >= 0 for row in rows))
            self.assertEqual(rows[-1]["optimizer_updates_completed"], 2)

            calls_before_resume = len(backend.calls)
            resumed = FormalSequentialRunner(
                root, "646", backend, parallel_jobs=16,
            ).run_all()
            self.assertEqual(len(backend.calls), calls_before_resume)
            self.assertEqual(resumed["skipped_formal_batches"], 3)

    def test_rejects_nonsequential_batch_indices(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            controller = {
                "controller_id": "M|STR",
                "model_id": "M",
                "ifun": "STR",
                "server": "646",
                "model_path": "models/M.ft",
                "optimizer": {},
                "formal_batches": [{"batch_index": 2}],
            }
            write_jsonl(root / "server_646_controllers.jsonl", [controller])
            with self.assertRaisesRegex(ValueError, "batch indices"):
                FormalSequentialRunner(root, "646", FakeBackend(), parallel_jobs=1).run_all()


if __name__ == "__main__":
    unittest.main()
