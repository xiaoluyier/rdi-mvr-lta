import csv
import importlib.util
import math
import pathlib
import re
import sys
import tempfile
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[7]
ORACLE_PATH = PROJECT_ROOT / "scripts" / "run_n4q_oracle.py"


def load_oracle_module():
    spec = importlib.util.spec_from_file_location("n4q_oracle_runner", ORACLE_PATH)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class N4qOracleMathTests(unittest.TestCase):
    def test_continuation_seed_is_preregistered_sha256_low64(self):
        oracle = load_oracle_module()
        actual = oracle.continuation_seed(
            "r2_m1_sym_paths_k1_s1", "C3", "C3_000", 1)
        self.assertEqual(actual, 13008627107473213913)

    def test_wilson_interval_and_stop_rule(self):
        oracle = load_oracle_module()
        low, high = oracle.wilson_interval(100, 200)
        self.assertAlmostEqual(low, 0.4313596221, places=8)
        self.assertAlmostEqual(high, 0.5686403779, places=8)
        self.assertGreater((high - low) / 2.0, 0.03)
        low2, high2 = oracle.wilson_interval(500, 1000)
        self.assertLessEqual((high2 - low2) / 2.0, 0.031)

    def test_confident_pair_inversion_uses_nonoverlapping_intervals(self):
        oracle = load_oracle_module()
        states = [
            {"state_id": "a", "h_hat": 0.10, "wilson_low": 0.08, "wilson_high": 0.12,
             "N4": 30.0, "N4Q": 10.0},
            {"state_id": "b", "h_hat": 0.50, "wilson_low": 0.45, "wilson_high": 0.55,
             "N4": 20.0, "N4Q": 40.0},
            {"state_id": "c", "h_hat": 0.11, "wilson_low": 0.09, "wilson_high": 0.14,
             "N4": 31.0, "N4Q": 11.0},
        ]
        n4 = oracle.confident_pair_inversion(states, "N4")
        n4q = oracle.confident_pair_inversion(states, "N4Q")
        self.assertEqual(n4["confident_pairs"], 2)
        self.assertEqual(n4["inversions"], 2)
        self.assertEqual(n4q["inversions"], 0)

    def test_rank_metrics_handle_ties(self):
        oracle = load_oracle_module()
        truth = [0.1, 0.2, 0.2, 0.9]
        perfect = [1.0, 2.0, 2.0, 9.0]
        reversed_score = [9.0, 2.0, 2.0, 1.0]
        self.assertAlmostEqual(oracle.spearman(truth, perfect), 1.0)
        self.assertAlmostEqual(oracle.kendall_tau_b(truth, perfect), 1.0)
        self.assertAlmostEqual(oracle.spearman(truth, reversed_score), -1.0)
        self.assertAlmostEqual(oracle.kendall_tau_b(truth, reversed_score), -1.0)

    def test_state_value_parser_and_ifun_expression_evaluator(self):
        oracle = load_oracle_module()
        state = oracle.parse_state_values("A_broken=1|TOP_st=4|fraction=0.5")
        self.assertEqual(
            state,
            {"A_broken": 1.0, "TOP_st": 4.0, "fraction": 0.5},
        )
        self.assertEqual(
            oracle.evaluate_ifun_expression(
                "1+floor((A_broken*500)+fraction)", state),
            501,
        )

    def test_historical_ifun_hashes_remain_frozen(self):
        oracle = load_oracle_module()
        model = (
            PROJECT_ROOT / "run_artifact3" / "artifact" / "experiments"
            / "models" / "FT" / "rarity_ladder" / "MODEL_A"
            / "MODEL_A_s0001.ft"
        )
        self.assertEqual(
            oracle.historical_ifun_hashes(model, mission_time=1000.0),
            {
                "N4": "cc1e58bd0a7483ece88793740f07d9f10ae5a0855602552d603adaf3f1325b2f",
                "N4A": "8dcd70f082a07e0ccf1aecac6c56e7a5dc356438b30ddde6056a2b398d3aa710",
                "N4AN": "18cd5f0482e54992a3fe3d112d9f91bc616a0e45c46aec96883e15b44d6b409c",
            },
        )


    def test_score_compilation_uses_frozen_n4_n4p_n4q_formulas(self):
        oracle = load_oracle_module()
        ft = (
            'toplevel "TOP";\n'
            '"TOP" or "A" "B";\n'
            '"A" fail~exponential(0.2) repair~dirac(99999999.0);\n'
            '"B" fail~exponential(0.4) repair~dirac(99999999.0);\n'
        )
        expressions = oracle.compile_score_expressions(ft, mission_time=1.0)
        initial = {"TOP_count": 0.0, "A_broken": 0.0, "B_broken": 0.0}
        rare = {"TOP_count": 1.0, "A_broken": 1.0, "B_broken": 0.0}
        self.assertEqual(
            oracle.score_snapshot(expressions, initial),
            {"N4": 1, "N4P": 1, "N4Q": 1},
        )
        self.assertEqual(
            oracle.score_snapshot(expressions, rare),
            {"N4": 501, "N4P": 501, "N4Q": 501},
        )
        self.assertTrue(oracle.is_threshold_near(25, 100, "20|70"))
        self.assertFalse(oracle.is_threshold_near(40, 100, "20|70"))

    def test_model_summary_compares_methods_on_identical_states(self):
        oracle = load_oracle_module()
        states = []
        for index, h_hat in enumerate((0.1, 0.3, 0.6, 0.9)):
            states.append({
                "state_id": str(index),
                "h_hat": h_hat,
                "wilson_low": h_hat - 0.01,
                "wilson_high": h_hat + 0.01,
                "N4": 4 - index,
                "N4P": index + 1,
                "N4Q": index + 1,
                "threshold_near": True,
            })
        summary = oracle.summarize_model_states(states)
        self.assertEqual(summary["confident_pairs"], 6)
        self.assertEqual(summary["n4_inversions"], 6)
        self.assertEqual(summary["n4q_inversions"], 0)
        self.assertAlmostEqual(summary["n4q_spearman"], 1.0)
        self.assertEqual(summary["threshold_confident_pairs"], 6)

    def test_model_cluster_bootstrap_is_deterministic(self):
        oracle = load_oracle_module()
        first = oracle.cluster_bootstrap_ci(
            [0.10, 0.20, 0.30, 0.40], reps=1000, seed=20260722)
        second = oracle.cluster_bootstrap_ci(
            [0.10, 0.20, 0.30, 0.40], reps=1000, seed=20260722)
        self.assertEqual(first, second)
        self.assertGreater(first[0], 0.0)

    def test_analyze_cli_is_explicit_and_requires_paths(self):
        oracle = load_oracle_module()
        args = oracle.build_parser().parse_args([
            "analyze",
            "--parts-dir", "parts",
            "--manifest", "manifest.csv",
            "--out-dir", "oracle",
        ])
        self.assertEqual(args.command, "analyze")
        self.assertEqual(args.parts_dir, "parts")


    def test_preregistered_gate_passes_only_with_all_evidence(self):
        oracle = load_oracle_module()
        models = []
        runs = []
        for index, model_id in enumerate(oracle.DEVELOPMENT_MODELS):
            models.append({
                "model_id": model_id,
                "topology": ("sym", "bottleneck", "shared")[index % 3],
                "state_count": 60,
                "confident_pairs": 100,
                "n4_inversions": 20,
                "n4q_inversions": 10,
                "n4_inversion_rate": 0.20,
                "n4q_inversion_rate": 0.10,
                "replay_ok": True,
            })
            runs.append({
                "model_id": model_id,
                "generation_ok": "1",
                "exprtk_compile_ok": "1",
                "eval_ns_n4": "100",
                "eval_ns_n4q": "104",
            })
        gate = oracle.evaluate_oracle_gate(
            models, runs, oracle.DEVELOPMENT_MODELS)
        self.assertTrue(gate["gate_pass"])
        self.assertEqual(gate["models_improved"], 12)
        missing_compile = [dict(row) for row in runs]
        missing_compile[0]["exprtk_compile_ok"] = "0"
        failed = oracle.evaluate_oracle_gate(
            models, missing_compile, oracle.DEVELOPMENT_MODELS)
        self.assertFalse(failed["gate_pass"])
        self.assertFalse(failed["gate_generation_compile"])


    def test_analyze_oracle_parts_merges_scores_and_summary(self):
        oracle = load_oracle_module()
        model_id = "tiny_oracle_model"
        raw_fields = (
            "model_id source state_id stratum level weight life_time "
            "source_importance source_importance_max source_threshold_values "
            "source_eval_ns_per_call source_eval_call_count "
            "state_values clock_values path_signature hits trials h_hat "
            "wilson_low wilson_high wilson_half_width failure_count "
            "same_seed_reproducible different_seed_distinct clone_unchanged "
            "initial_clock_lifetime_preserved resampled_active_clock_count"
        ).split()
        continuation_fields = (
            "model_id source state_id rep probe continuation_seed hit status "
            "life_before life_after initial_preserved resampled_clock_count "
            "final_signature"
        ).split()
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            parts = root / "parts"
            output = root / "output"
            parts.mkdir()
            ft_path = root / "tiny.ft"
            ft_path.write_text(
                'toplevel "TOP";\n'
                '"TOP" or "A" "B";\n'
                '"A" fail~exponential(0.2) repair~dirac(99999999.0);\n'
                '"B" fail~exponential(0.4) repair~dirac(99999999.0);\n',
                encoding="utf-8",
            )
            manifest = root / "manifest.csv"
            with manifest.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(
                    stream,
                    fieldnames=("model_id", "ft_path", "topology", "mission_time"),
                )
                writer.writeheader()
                writer.writerow({
                    "model_id": model_id,
                    "ft_path": str(ft_path),
                    "topology": "tiny",
                    "mission_time": "1",
                })
            for source in ("C3", "C11Q"):
                raw_path = parts / (model_id + "_" + source + "_raw_states.csv")
                with raw_path.open("w", newline="", encoding="utf-8") as stream:
                    writer = csv.DictWriter(stream, fieldnames=raw_fields)
                    writer.writeheader()
                    for index in range(30):
                        h_hat = (index + 1) / 31.0
                        writer.writerow({
                            "model_id": model_id,
                            "source": source,
                            "state_id": "{}_{:03d}".format(source, index),
                            "stratum": ("early", "middle", "late")[index // 10],
                            "level": index,
                            "weight": 1,
                            "life_time": index,
                            "source_importance": 1,
                            "source_importance_max": 501,
                            "source_threshold_values": "1|250|501",
                            "source_eval_ns_per_call": (100 if source == "C3" else 104),
                            "source_eval_call_count": 6000,
                            "state_values": "TOP_count=0|A_broken=0|B_broken=0",
                            "clock_values": "0:A:1:1.0",
                            "path_signature": "TOP_count=0",
                            "hits": index + 1,
                            "trials": 200,
                            "h_hat": h_hat,
                            "wilson_low": h_hat - 0.005,
                            "wilson_high": h_hat + 0.005,
                            "wilson_half_width": 0.005,
                            "failure_count": 0,
                            "same_seed_reproducible": 1,
                            "different_seed_distinct": 1,
                            "clone_unchanged": 1,
                            "initial_clock_lifetime_preserved": 1,
                            "resampled_active_clock_count": 1,
                        })
                continuation_path = (
                    parts / (model_id + "_" + source + "_continuations.csv"))
                with continuation_path.open(
                    "w", newline="", encoding="utf-8"
                ) as stream:
                    writer = csv.DictWriter(
                        stream, fieldnames=continuation_fields)
                    writer.writeheader()
                    writer.writerow({
                        "model_id": model_id,
                        "source": source,
                        "state_id": source + "_000",
                        "rep": 1,
                        "probe": 0,
                        "continuation_seed": 7,
                        "hit": 0,
                        "status": "ok",
                        "life_before": 0,
                        "life_after": 1,
                        "initial_preserved": 1,
                        "resampled_clock_count": 1,
                        "final_signature": "stable",
                    })
            result = oracle.analyze_oracle_parts(
                parts,
                manifest,
                output,
                expected_models=(model_id,),
            )
            self.assertFalse(result["gate_pass"])
            self.assertAlmostEqual(result["median_eval_overhead"], 0.04)
            self.assertTrue((output / "run_manifest.csv").is_file())
            with (output / "raw_states.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                raw_rows = list(csv.DictReader(stream))
            self.assertEqual(len(raw_rows), 60)
            self.assertEqual(set(raw_rows[0]).issuperset({"N4", "N4P", "N4Q"}), True)
            with (output / "continuations.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 2)
            with (output / "summary.csv").open(
                newline="", encoding="utf-8"
            ) as stream:
                summary_rows = list(csv.DictReader(stream))
            self.assertEqual(summary_rows[0]["model_id"], model_id)


class N4qOracleSourceContractTests(unittest.TestCase):
    def test_traial_copy_preserves_next_split_level(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "include" / "Traial.h"
        text = path.read_text(encoding="utf-8")
        body = re.search(
            r"inline Traial& operator=\(const Traial& that\)(.*?)return \*this;",
            text,
            re.DOTALL,
        ).group(1)
        self.assertIn("nextSplitLevel", body)

    def test_clock_distributions_expose_replay_reset(self):
        core = (PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "include" / "core_typedefs.h").read_text(encoding="utf-8")
        clock_h = (PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "include" / "Clock.h").read_text(encoding="utf-8")
        clock_cpp = (PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "Clock.cpp").read_text(encoding="utf-8")
        self.assertIn("virtual void reset_rng_state() const", core)
        self.assertIn("reset_distribution_rng_state", clock_h)
        self.assertIn("reseed_for_diagnostics", clock_h)
        self.assertIn("Clock::reset_distribution_rng_state", clock_cpp)
        self.assertIn("Clock::reseed_for_diagnostics", clock_cpp)

    def test_sfe_oracle_is_explicitly_gated_and_exports_full_snapshot(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertIn('N4Q_ORACLE_DIAG', text)
        for token in (
            "state_values",
            "clock_values",
            "life_time",
            "level",
            "weight",
            "wilson_low",
            "wilson_high",
            "source_importance",
            "source_importance_max",
            "source_threshold_values",
            "source_eval_ns_per_call",
            "source_eval_call_count",
            "continuation_seed",
        ):
            self.assertIn(token, text)

    def test_sfe_requires_ten_snapshots_in_each_preregistered_stratum(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertRegex(
            text,
            r"oracleSelected\[stratum\]\.size\(\) != 10ul[\s\S]*?throw_FigException",
        )

    def test_sfe_captures_snapshots_before_and_after_each_simulation_step(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertRegex(
            text,
            r"oracleCapture\(traial, lvl\);[\s\S]*?"
            r"model_->simulation_step\([\s\S]*?"
            r"oracleCapture\(traial, traial\.level\);",
        )

    def test_oracle_sampling_effort_can_fill_each_ten_snapshot_stratum(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertRegex(
            text,
            r"""N4Q_ORACLE_LEVEL_EFFORT", 200ul[\s\S]*?"""
            r"std::max\(oracleLevelEffort, nominalLevelEffort\)",
        )
        self.assertIn("oracle_level_effort", text)
        self.assertIn("N4Q_ORACLE_HORIZON", text)
        self.assertIn("stratum_basis", text)
        self.assertRegex(
            text,
            r"oracleWatchEvents[\s\S]*?oracleCapture\(candidate, candidate\.level\)",
        )

    def test_oracle_continuations_finish_before_dedicated_run_stops(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        watcher = re.search(
            r"EventWatcher continuationWatcher(.*?)\n\s*\};",
            text,
            re.DOTALL,
        ).group(1)
        self.assertNotIn("interrupted", watcher)
        self.assertRegex(
            text,
            r"rawStates\.flush\(\)[\s\S]*?continuations\.flush\(\)[\s\S]*?std::_Exit\(EXIT_SUCCESS\)",
        )
    def test_continuation_resamples_active_ft_clocks_after_clone_check(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertRegex(
            text,
            r"clone\s*=\s*snapshot;[\s\S]*?"
            r"outcome\.initialPreserved[\s\S]*?"
            r"oracleResampleActiveClocks\(clone\)",
        )
        self.assertIn("resampled_clock_count", text)
        self.assertRegex(
            text, r"clock\.resample\(timeout\.sampled, timeout\.value\)"
        )



    def test_zero_continuation_seed_branch_is_closed_before_running(self):
        path = PROJECT_ROOT / "run_artifact3" / "artifact" / "setup" / "fig" / "src" / "SimulationEngineSFE.cpp"
        text = path.read_text(encoding="utf-8")
        self.assertRegex(text, r"if \(0ul == seeds\[attempt\]\)\s*\{\s*failures\+\+;\s*continue;\s*\}\s*const auto outcome")


if __name__ == "__main__":
    unittest.main()
