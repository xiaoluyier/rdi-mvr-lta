import itertools
import importlib.util
import math
import os
import pathlib
import re
import subprocess
import sys
import tempfile
import unittest


HERE = os.path.dirname(os.path.abspath(__file__))
PARSER_DIR = os.path.dirname(HERE)
if PARSER_DIR not in sys.path:
    sys.path.insert(0, PARSER_DIR)

from kepler import IfunN4pContext, IfunN4qContext, Kepler2IOSA


def leaf(name, rate):
    return '"{}" fail~exponential({}) repair~dirac(99999999.0);'.format(
        name, rate)


def ft_text(gates, leaves, top="TOP"):
    return 'toplevel "{}";\n\n{}\n\n{}\n'.format(
        top, "\n".join(gates), "\n".join(leaves))


def make_context(ft, mission_time=1.0, cls=IfunN4qContext):
    kepler = Kepler2IOSA(transient_horizon=mission_time)
    raw_tree, compiler, state_tree = kepler._prepare_ifun_tree(ft)
    return cls(
        compiler,
        raw_tree,
        state_tree,
        mission_time=mission_time,
        scale=1000,
        d_scale=100.0,
        epsilon=1e-300,
    )


class N4qStaticProbabilityTests(unittest.TestCase):
    def setUp(self):
        self.rate_a = 0.2
        self.rate_b = 0.4
        self.rate_c = 0.7
        self.q_a = 1.0 - math.exp(-self.rate_a)
        self.q_b = 1.0 - math.exp(-self.rate_b)
        self.q_c = 1.0 - math.exp(-self.rate_c)

    def test_two_leaf_or_matches_hand_calculation(self):
        ctx = make_context(ft_text(
            ['"TOP" or "A" "B";'],
            [leaf("A", self.rate_a), leaf("B", self.rate_b)],
        ))
        expected = 1.0 - (1.0 - self.q_a) * (1.0 - self.q_b)
        self.assertAlmostEqual(ctx.initial_h, expected, places=14)

    def test_two_leaf_and_matches_hand_calculation(self):
        ctx = make_context(ft_text(
            ['"TOP" and "A" "B";'],
            [leaf("A", self.rate_a), leaf("B", self.rate_b)],
        ))
        self.assertAlmostEqual(ctx.initial_h, self.q_a * self.q_b, places=14)

    def test_two_of_three_matches_poisson_binomial_tail(self):
        ctx = make_context(ft_text(
            ['"TOP" 2of3 "A" "B" "C";'],
            [
                leaf("A", self.rate_a),
                leaf("B", self.rate_b),
                leaf("C", self.rate_c),
            ],
        ))
        expected = (
            self.q_a * self.q_b * (1.0 - self.q_c)
            + self.q_a * self.q_c * (1.0 - self.q_b)
            + self.q_b * self.q_c * (1.0 - self.q_a)
            + self.q_a * self.q_b * self.q_c
        )
        self.assertAlmostEqual(ctx.initial_h, expected, places=14)

    def test_shared_event_is_not_counted_twice(self):
        ctx = make_context(ft_text(
            [
                '"TOP" and "A" "X";',
                '"X" or "A" "B";',
            ],
            [leaf("A", self.rate_a), leaf("B", self.rate_b)],
        ))
        self.assertAlmostEqual(ctx.initial_h, self.q_a, places=14)
        diag = ctx.diagnostic_row()
        self.assertEqual(diag["unique_leaf_count"], 2)
        self.assertGreaterEqual(diag["repeated_leaf_count"], 1)

    def test_static_gate_probability_is_monotone_over_all_states(self):
        ctx = make_context(ft_text(
            ['"TOP" 2of3 "A" "B" "C";'],
            [
                leaf("A", self.rate_a),
                leaf("B", self.rate_b),
                leaf("C", self.rate_c),
            ],
        ))
        leaves = ("A", "B", "C")
        values = {}
        for n in range(len(leaves) + 1):
            for subset in itertools.combinations(leaves, n):
                values[frozenset(subset)] = ctx.probability(
                    failed_leaves=set(subset))
        for failed, value in values.items():
            for name in leaves:
                if name not in failed:
                    self.assertGreaterEqual(
                        values[failed | {name}], value - 1e-15)
        self.assertEqual(values[frozenset(leaves)], 1.0)


class N4qPandTests(unittest.TestCase):
    def setUp(self):
        self.rate_a = 0.3
        self.rate_b = 0.5
        self.ctx = make_context(ft_text(
            ['"TOP" pand "A" "B";'],
            [leaf("A", self.rate_a), leaf("B", self.rate_b)],
        ))

    def test_completed_blocked_armed_and_not_started(self):
        q_b = 1.0 - math.exp(-self.rate_b)
        expected_fresh = (
            1.0 - math.exp(-self.rate_a)
            - self.rate_a / (self.rate_a - self.rate_b)
            * (math.exp(-self.rate_b) - math.exp(-self.rate_a))
        )
        self.assertEqual(
            self.ctx.probability(pand_states={"TOP": "completed"}), 1.0)
        self.assertEqual(
            self.ctx.probability(pand_states={"TOP": "blocked"}), 0.0)
        self.assertAlmostEqual(
            self.ctx.probability(pand_states={"TOP": "armed"}), q_b,
            places=14)
        self.assertAlmostEqual(
            self.ctx.probability(pand_states={"TOP": "not_started"}),
            expected_fresh,
            places=14)

    def test_equal_hazards_use_stable_limit(self):
        rate = 0.4
        ctx = make_context(ft_text(
            ['"TOP" pand "A" "B";'],
            [leaf("A", rate), leaf("B", rate)],
        ))
        expected = 1.0 - math.exp(-rate) - rate * math.exp(-rate)
        actual = ctx.probability(pand_states={"TOP": "not_started"})
        self.assertTrue(math.isfinite(actual))
        self.assertAlmostEqual(actual, expected, places=14)

    def test_unique_path_blocked_has_zero_reachability(self):
        self.assertEqual(
            self.ctx.probability(pand_states={"TOP": "blocked"}), 0.0)


class N4qExpressionAndAblationTests(unittest.TestCase):
    def setUp(self):
        self.ft = ft_text(
            [
                '"TOP" and "A" "X";',
                '"X" or "A" "B";',
            ],
            [leaf("A", 0.2), leaf("B", 0.4)],
        )

    def test_generation_is_byte_stable_and_bounded(self):
        first = make_context(self.ft)
        second = make_context(self.ft)
        expr1, maximum1 = first.progress_expr()
        expr2, maximum2 = second.progress_expr()
        self.assertEqual((expr1, maximum1), (expr2, maximum2))
        self.assertIn("log(max(", expr1)
        self.assertNotIn("log(0", expr1)
        self.assertEqual(maximum1, 1000)
        self.assertEqual(first.diagnostic_row()["expression_length"], len(expr1))

    def test_diagnostics_have_all_preregistered_fields(self):
        diag = make_context(self.ft).diagnostic_row()
        expected = {
            "n4q_version",
            "bdd_node_count",
            "unique_leaf_count",
            "repeated_leaf_count",
            "pand_count",
            "pand_shared_overlap_count",
            "expression_length",
            "initial_h",
            "initial_d",
            "n4q_mode",
            "approximation_flags",
        }
        self.assertTrue(expected.issubset(diag))
        self.assertEqual(diag["n4q_mode"], "N4Q")

    def test_n4p_uses_structure_bound_not_historical_ten_x_penalty(self):
        pand_ft = ft_text(
            ['"TOP" pand "A" "B";'],
            [leaf("A", 0.2), leaf("B", 0.4)],
        )
        ctx = make_context(pand_ft, cls=IfunN4pContext)
        expr, maximum = ctx.progress_expr()
        self.assertIn("TOP_st==4", expr)
        self.assertEqual(maximum, 1000)
        self.assertEqual(ctx.diagnostic_row()["n4q_mode"], "N4P")
        self.assertIn("pand_blocked_unreachable", ctx.diagnostic_row()["approximation_flags"])


    def test_spare_approximation_is_declared(self):
        project_root = pathlib.Path(__file__).resolve().parents[7]
        ft = (
            project_root / "run_artifact3" / "artifact" / "experiments"
            / "models" / "FT" / "DSPARE_3.ft"
        ).read_text(encoding="utf-8")
        ctx = make_context(ft)
        ctx.progress_expr()
        self.assertIn(
            "spare_as_and", ctx.diagnostic_row()["approximation_flags"])


class N4qDispatchIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.ft = ft_text(
            ['"TOP" or "A" "B";'],
            [leaf("A", 0.2), leaf("B", 0.4)],
        )

    def test_kepler_has_independent_n4p_and_n4q_dispatches(self):
        kepler = Kepler2IOSA(transient_horizon=1.0)
        n4p = kepler.ifun_n4p(self.ft)
        n4p_diag = kepler.n4p_diagnostic()
        n4q = kepler.ifun_n4q(self.ft)
        n4q_diag = kepler.n4q_diagnostic()
        self.assertEqual(n4p[1:], (1, 501))
        self.assertEqual(n4q[1:], (1, 501))
        self.assertIn("n4q_mode=N4P", n4p_diag)
        self.assertIn("n4q_mode=N4Q", n4q_diag)
        self.assertNotEqual(n4p[0], n4q[0])

    def test_kompilator_generates_both_new_ifuns(self):
        parser_dir = pathlib.Path(PARSER_DIR)
        kompilator = parser_dir / "kompilator.py"
        with tempfile.TemporaryDirectory() as tmp:
            ft_path = pathlib.Path(tmp) / "model.ft"
            iosa_path = pathlib.Path(tmp) / "model.sa"
            ft_path.write_text(self.ft, encoding="utf-8")
            result = subprocess.run(
                [
                    sys.executable,
                    str(kompilator),
                    str(ft_path),
                    str(iosa_path),
                    "-f",
                    "-t",
                    "1",
                    "-i",
                    "N4P",
                    "N4Q",
                ],
                cwd=str(parser_dir),
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, msg=result.stderr)
            generated = iosa_path.read_text(encoding="utf-8")
            self.assertIn("// N4P DIAG:", generated)
            self.assertIn("// IFUN N4P:", generated)
            self.assertIn("// N4Q DIAG:", generated)
            self.assertIn("// IFUN N4Q:", generated)

    def test_paper_runner_reserves_c11p_and_c11q(self):
        project_root = pathlib.Path(__file__).resolve().parents[7]
        runner_path = project_root / "scripts" / "run_paper_claim_slice.py"
        spec = importlib.util.spec_from_file_location("n4q_paper_runner", runner_path)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = runner
        spec.loader.exec_module(runner)
        self.assertEqual(
            runner.CONFIGS["C11P"],
            {"ifun": "N4P", "ifun_kind": "N4P", "pool": "modal", "slice": "11"},
        )
        self.assertEqual(
            runner.CONFIGS["C11Q"],
            {"ifun": "N4Q", "ifun_kind": "N4Q", "pool": "modal", "slice": "11"},
        )

    def test_oneclick_has_complete_n4p_n4q_routes(self):
        project_root = pathlib.Path(__file__).resolve().parents[7]
        oneclick_path = (
            project_root
            / "run_artifact3"
            / "artifact"
            / "experiments"
            / "scripts"
            / "run_phase_pand_reuse_oneclick.sh"
        )
        script = oneclick_path.read_text(encoding="utf-8")
        for kind in ("N4P", "N4Q"):
            self.assertRegex(
                script,
                re.compile(
                    r"^\s*{}\)\s*$.*?IFUN {}:".format(kind, kind),
                    re.MULTILINE | re.DOTALL,
                ),
            )
        no_ft_line = next(
            line for line in script.splitlines()
            if line.lstrip().startswith('if [[ "$IFUN_KIND" == "TIME"')
        )
        self.assertIn('"N4P"', no_ft_line)
        self.assertIn('"N4Q"', no_ft_line)
        self.assertIn('N4Q_ORACLE_DIAG', script)
        self.assertIn('FIG_ARGS+=(--batch-size 1)', script)

    def test_runner_parses_all_n4q_diagnostic_fields(self):
        project_root = pathlib.Path(__file__).resolve().parents[7]
        runner_path = project_root / "scripts" / "run_paper_claim_slice.py"
        spec = importlib.util.spec_from_file_location("n4q_diag_runner", runner_path)
        runner = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = runner
        spec.loader.exec_module(runner)
        fields = {
            "n4q_version": "N4Q-v1",
            "bdd_node_count": "9",
            "unique_leaf_count": "3",
            "repeated_leaf_count": "1",
            "pand_count": "2",
            "pand_shared_overlap_count": "1",
            "expression_length": "777",
            "initial_h": "1e-08",
            "initial_d": "18.4207",
            "n4q_mode": "N4Q",
            "approximation_flags": "pand_effective_hazard",
        }
        payload = ",".join("{}={}".format(k, v) for k, v in fields.items())
        with tempfile.TemporaryDirectory() as tmp:
            iosa = pathlib.Path(tmp) / "model.sa"
            iosa.write_text("// N4Q DIAG: N4Q " + payload + "\n", encoding="utf-8")
            parsed = runner.parse_n4q_diagnostic(str(iosa))
        self.assertEqual(parsed, fields)
        self.assertTrue(set(fields).issubset(runner.FIELDNAMES))


if __name__ == "__main__":
    unittest.main()
