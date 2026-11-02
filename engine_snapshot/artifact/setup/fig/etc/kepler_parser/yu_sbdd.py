#!/usr/bin/env python
"""Yu Haiyue-style analytic DFT report for Kepler fault trees.

This module implements a practical subset of the thesis workflow:

1. parse a Kepler DFT;
2. expand FDEP using the existing FIG transformation;
3. derive first-hit sequential cut sequences for the top event;
4. optionally compute an exact transient probability for non-repairable
   exponential, phase-dependent models by CTMC uniformization.

The symbolic part is intentionally bounded because disjoint sequential
cut-sequence enumeration can grow factorially.
"""

from __future__ import annotations

import math
from collections import defaultdict, deque
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

try:
    import numpy as np
except Exception:  # pragma: no cover - handled at runtime
    np = None

from lark import Token, Tree

from kepler import TransformFDEP, kepler_parser


PAND_IDLE = 0
PAND_WAITING = 1
PAND_FAILED = 2
PAND_INVALID = 3


@dataclass(frozen=True)
class GateSpec:
    name: str
    kind: str
    children: Tuple[str, ...]
    k: int = 0


@dataclass(frozen=True)
class State:
    failed_mask: int
    pand_statuses: Tuple[int, ...]


@dataclass
class PhaseMeta:
    num_phases: int
    phase_durations: List[float]
    phase_overrides: Dict[str, Dict[str, Dict[int, str]]]
    phase_actives: Dict[int, Set[str]]
    has_active_lists: bool
    inherit_map: Dict[str, Tuple[str, int]]


@dataclass
class AnalyticModel:
    top_name: str
    basic_events: Dict[str, Dict[int, float]]
    gates: Dict[str, GateSpec]
    topo_gates: List[str]
    pand_names: List[str]
    phase_meta: PhaseMeta
    be_index: Dict[str, int]


class YuHaiyueAnalyzer:
    """Generate Yu Haiyue SBDD/CSS-style analytic reports."""

    def __init__(self, horizon: Optional[float] = None, max_css: int = 2000,
                 state_limit: int = 20000, poisson_tol: float = 1e-15) -> None:
        self.horizon = None if horizon is None else float(horizon)
        self.max_css = max(int(max_css), 1)
        self.state_limit = max(int(state_limit), 100)
        # This tolerance controls only the omitted Poisson tail.  It must not
        # be used as an absolute cutoff for state probabilities: the analyzer
        # is specifically expected to preserve rare-event masses far below
        # 1e-12.
        self.poisson_tol = max(float(poisson_tol), 1e-300)

    def analyze_text(self, text: str) -> str:
        tree = kepler_parser.parse(text)
        tree = TransformFDEP(tree).visit(tree)
        model = build_analytic_model(tree)
        css_rows, css_truncated = FirstHitCssEnumerator(model, self.max_css).enumerate()
        exact = self._try_exact_probability(model)
        return self._format_report(model, css_rows, css_truncated, exact)

    def _try_exact_probability(self, model: AnalyticModel) -> Dict[str, object]:
        if np is None:
            return {"available": False, "reason": "numpy is not available"}
        if self.horizon is None:
            return {
                "available": False,
                "reason": "provide --yu-horizon or -t/--transient for a numeric transient probability",
            }
        try:
            solver = YuExactSolver(
                model,
                horizon=float(self.horizon),
                state_limit=self.state_limit,
                poisson_tol=self.poisson_tol,
            )
            return {"available": True, **solver.solve()}
        except Exception as exc:
            return {"available": False, "reason": str(exc)}

    def _format_report(self, model: AnalyticModel, css_rows: List[Tuple[str, ...]],
                       css_truncated: bool, exact: Dict[str, object]) -> str:
        lines: List[str] = []
        lines.append("YU_HAIYUE_ANALYTIC_REPORT")
        lines.append("method=improved_sbdd_first_hit_css_plus_phase_ctmc")
        lines.append("source=Yu Haiyue thesis Chapter 4: multi-phase DFT, improved SBDD, topological sorting of sequential Boolean variables")
        lines.append("")
        lines.append("[workflow]")
        lines.append("1. Parse Kepler fault tree and replace FDEP by equivalent OR gates.")
        lines.append("2. Build the DFT algebra: AND=conjunction, OR=disjunction, VOT=k-of-n, PAND=left output before right output.")
        lines.append("3. Enumerate disjoint first-hit path prefixes, equivalent to SBDD 1-paths after Shannon decomposition.")
        lines.append("4. Express Pr{SysFail} as the sum of disjoint first-hit path probabilities.")
        lines.append("5. For exponential non-repairable phase models, solve the same ordered-event semantics exactly by per-phase CTMC uniformization.")
        lines.append("")
        lines.append("[model]")
        lines.append("top_event={}".format(model.top_name))
        lines.append("basic_events={}".format(",".join(sorted(model.basic_events))))
        lines.append("gates={}".format(",".join(["{}:{}".format(k, model.gates[k].kind) for k in model.topo_gates])))
        lines.append("pand_gates={}".format(",".join(model.pand_names) if model.pand_names else "none"))
        if model.phase_meta.num_phases > 1:
            lines.append("phases={} boundaries_define_first_{}_durations={}".format(
                model.phase_meta.num_phases,
                max(model.phase_meta.num_phases - 1, 0),
                ",".join(_fmt_float(x) for x in model.phase_meta.phase_durations),
            ))
        else:
            lines.append("phases=1")
        lines.append("")
        lines.append("[formulas_from_thesis]")
        lines.append("Shannon: f = x*f[x=1] + not(x)*f[x=0] = ite(x,F1,F0)")
        lines.append("BDD apply: g op h = ite(min_index_root, high_apply, low_apply)")
        lines.append("Forward PDO: Ai op Aj => ite(Ai, G1 op H1, G0 op H) for i<j")
        lines.append("Backward PDO: Ai op Aj => ite(Aj, G op H1, G0 op H0) for i<j")
        lines.append("Event indicator: I(w)=1 iff t(w) in [0,T]")
        lines.append("AND time: t(a*b)=max(t(a),t(b)); OR time: t(a+b)=min(t(a),t(b))")
        lines.append("PAND/sequence: I(a before b)=1 iff 0<=t(a)<t(b)<=T")
        lines.append("Binary sequential variable: gamma_eta=1 iff t(gamma)<t(eta)")
        lines.append("Topological sorting: binary order constraints are converted to disjoint total orders.")
        lines.append("")
        lines.append("[disjoint_first_hit_path_prefixes]")
        lines.append("note=Each P row is a disjoint SBDD/CTMC first-hit path class, not necessarily a minimal CSS.")
        lines.append("note=Pr{firsthit(A < B < C)} means the listed firings occur in that order before the top event first becomes true; unlisted events do not fire earlier.")
        if css_truncated:
            lines.append("truncated=true max_css={}".format(self.max_css))
        else:
            lines.append("truncated=false count={}".format(len(css_rows)))
        if not css_rows:
            lines.append("none")
        else:
            for idx, seq in enumerate(css_rows, 1):
                lines.append("P{}={}".format(idx, " < ".join(seq)))
        lines.append("")
        lines.append("[probability_expression]")
        if css_rows:
            terms = ["Pr{{firsthit({})}}".format(" < ".join(seq)) for seq in css_rows]
            if css_truncated:
                lines.append("Pr{{{}}} = {} + ...".format(model.top_name, " + ".join(terms)))
            else:
                lines.append("Pr{{{}}} = {}".format(model.top_name, " + ".join(terms)))
        else:
            lines.append("Pr{{{}}} = 0".format(model.top_name))
        lines.append("")
        lines.append("[exact_phase_ctmc]")
        if exact.get("available"):
            lines.append("available=true")
            lines.append("horizon={}".format(_fmt_float(float(exact["horizon"]))))
            lines.append("failure_probability={:.17g}".format(float(exact["failure_probability"])))
            for row in exact.get("phase_summaries", []):
                lines.append(
                    "phase={phase} duration={duration} states={states} failure_probability={failure_probability:.17g}".format(
                        phase=row["phase"],
                        duration=_fmt_float(float(row["duration"])),
                        states=row["states"],
                        failure_probability=float(row["failure_probability"]),
                    )
                )
        else:
            lines.append("available=false")
            lines.append("reason={}".format(exact.get("reason", "unknown")))
        return "\n".join(lines)


class FirstHitCssEnumerator:
    """Enumerate first-hit sequences under DFT/PAND semantics."""

    def __init__(self, model: AnalyticModel, max_css: int) -> None:
        self.model = model
        self.max_css = max_css
        self._pand_index = {name: idx for idx, name in enumerate(model.pand_names)}
        self._truth_cache: Dict[State, Dict[str, bool]] = {}
        self._transition_cache: Dict[Tuple[State, str], State] = {}
        self._event_phases: Dict[str, Tuple[int, ...]] = {
            name: tuple(sorted(phase for phase, rate in rates.items() if rate > 0.0))
            for name, rates in model.basic_events.items()
        }
        for successor, (_, phase) in model.phase_meta.inherit_map.items():
            inherited = set(self._event_phases.get(successor, tuple()))
            inherited.add(phase)
            self._event_phases[successor] = tuple(sorted(inherited))

    def enumerate(self) -> Tuple[List[Tuple[str, ...]], bool]:
        start = State(0, tuple(PAND_IDLE for _ in self.model.pand_names))
        events = tuple(sorted(self.model.basic_events))
        rows: List[Tuple[str, ...]] = []
        truncated = False

        def dfs(state: State, remaining: Tuple[str, ...], prefix: Tuple[str, ...],
                earliest_phase: int) -> None:
            nonlocal truncated
            if truncated or len(rows) >= self.max_css:
                truncated = True
                return
            if self.is_top_failed(state):
                rows.append(prefix)
                return
            if not remaining:
                return
            for event in remaining:
                event_phase = self._next_feasible_phase(event, earliest_phase)
                if event_phase is None:
                    continue
                next_state = self.transition(state, event)
                next_prefix = prefix + (event,)
                if self.is_top_failed(next_state):
                    rows.append(next_prefix)
                    if len(rows) >= self.max_css:
                        truncated = True
                        return
                    continue
                next_remaining = tuple(x for x in remaining if x != event)
                dfs(next_state, next_remaining, next_prefix, event_phase)
                if truncated:
                    return

        dfs(start, events, tuple(), 1)
        # Prefix enumeration can reach identical first-hit descriptions via
        # equivalent gate histories; keep deterministic unique rows.
        unique: List[Tuple[str, ...]] = []
        seen = set()
        for row in rows:
            if row not in seen:
                seen.add(row)
                unique.append(row)
        return unique, truncated

    def _next_feasible_phase(self, event_name: str, earliest_phase: int) -> Optional[int]:
        """Return the earliest phase in which this ordered event can occur."""
        for phase in self._event_phases.get(event_name, tuple()):
            if phase >= earliest_phase:
                return phase
        return None

    def transition(self, state: State, event_name: str) -> State:
        key = (state, event_name)
        cached = self._transition_cache.get(key)
        if cached is not None:
            return cached
        if self._is_be_failed(state.failed_mask, event_name):
            return state
        old_truth = self.truth_map(state)
        new_mask = state.failed_mask | (1 << self.model.be_index[event_name])
        pand_status = {name: state.pand_statuses[self._pand_index[name]] for name in self.model.pand_names}
        truth = dict(old_truth)
        truth[event_name] = True
        for gate_name in self.model.topo_gates:
            gate = self.model.gates[gate_name]
            if gate.kind == "or":
                truth[gate_name] = any(truth[c] for c in gate.children)
            elif gate.kind == "and":
                truth[gate_name] = all(truth[c] for c in gate.children)
            elif gate.kind == "vot":
                truth[gate_name] = sum(1 for c in gate.children if truth[c]) >= gate.k
            elif gate.kind == "pand":
                left, right = gate.children
                pand_status[gate_name] = _update_pand_status(
                    pand_status[gate_name],
                    old_truth[left],
                    truth[left],
                    old_truth[right],
                    truth[right],
                )
                truth[gate_name] = pand_status[gate_name] == PAND_FAILED
        next_state = State(new_mask, tuple(pand_status[name] for name in self.model.pand_names))
        self._transition_cache[key] = next_state
        self._truth_cache[next_state] = truth
        return next_state

    def truth_map(self, state: State) -> Dict[str, bool]:
        cached = self._truth_cache.get(state)
        if cached is not None:
            return cached
        truth = {name: self._is_be_failed(state.failed_mask, name) for name in self.model.basic_events}
        pand_status = {name: state.pand_statuses[self._pand_index[name]] for name in self.model.pand_names}
        for gate_name in self.model.topo_gates:
            gate = self.model.gates[gate_name]
            if gate.kind == "or":
                truth[gate_name] = any(truth[c] for c in gate.children)
            elif gate.kind == "and":
                truth[gate_name] = all(truth[c] for c in gate.children)
            elif gate.kind == "vot":
                truth[gate_name] = sum(1 for c in gate.children if truth[c]) >= gate.k
            elif gate.kind == "pand":
                truth[gate_name] = pand_status[gate_name] == PAND_FAILED
        self._truth_cache[state] = truth
        return truth

    def is_top_failed(self, state: State) -> bool:
        return bool(self.truth_map(state)[self.model.top_name])

    def _is_be_failed(self, mask: int, name: str) -> bool:
        return bool(mask & (1 << self.model.be_index[name]))


class YuExactSolver(FirstHitCssEnumerator):
    """Exact transient probability for exponential phase-dependent models."""

    def __init__(self, model: AnalyticModel, horizon: float, state_limit: int,
                 poisson_tol: float) -> None:
        super().__init__(model, max_css=1)
        self.horizon = float(horizon)
        self.state_limit = state_limit
        self.poisson_tol = poisson_tol

    def solve(self) -> Dict[str, object]:
        durations = self._phase_durations_with_horizon()
        distribution: Dict[State, float] = {
            State(0, tuple(PAND_IDLE for _ in self.model.pand_names)): 1.0
        }
        phase_rows: List[Dict[str, object]] = []
        for phase in range(1, self.model.phase_meta.num_phases + 1):
            if phase > 1:
                distribution = self._apply_boundary_reuse_distribution(distribution, phase)
            distribution = self._advance_phase_distribution(distribution, phase, durations[phase - 1])
            fail_prob = sum(prob for state, prob in distribution.items() if self.is_top_failed(state))
            phase_rows.append(
                {
                    "phase": phase,
                    "duration": durations[phase - 1],
                    "states": len(distribution),
                    "failure_probability": fail_prob,
                }
            )
        final_prob = sum(prob for state, prob in distribution.items() if self.is_top_failed(state))
        return {
            "horizon": self.horizon,
            "failure_probability": final_prob,
            "phase_summaries": phase_rows,
            "boundary_state_count": len(distribution),
        }

    def _phase_durations_with_horizon(self) -> List[float]:
        num_phases = self.model.phase_meta.num_phases
        if num_phases <= 1:
            if self.horizon <= 0:
                raise RuntimeError("horizon must be positive")
            return [self.horizon]
        durations = list(self.model.phase_meta.phase_durations)
        used = sum(durations)
        tail = self.horizon - used
        if tail <= 0.0:
            raise RuntimeError(
                "horizon {} must exceed last phase boundary {}".format(self.horizon, used)
            )
        durations.append(tail)
        return durations

    def _apply_boundary_reuse_distribution(self, distribution: Dict[State, float],
                                           next_phase: int) -> Dict[State, float]:
        result: Dict[State, float] = defaultdict(float)
        for state, prob in distribution.items():
            result[self._apply_boundary_reuse_state(state, next_phase)] += prob
        return self._normalize_distribution(result)

    def _apply_boundary_reuse_state(self, state: State, next_phase: int) -> State:
        if self.is_top_failed(state):
            return state
        new_state = state
        for succ, (pred, phase_idx) in sorted(self.model.phase_meta.inherit_map.items()):
            if phase_idx == next_phase and self._is_be_failed(new_state.failed_mask, pred):
                new_state = self.transition(new_state, succ)
        return new_state

    def _advance_phase_distribution(self, distribution: Dict[State, float],
                                    phase: int, duration: float) -> Dict[State, float]:
        result: Dict[State, float] = defaultdict(float)
        for start_state, mass in distribution.items():
            if mass <= 0.0:
                continue
            if self.is_top_failed(start_state) or duration <= 0.0:
                result[start_state] += mass
                continue
            local = self._phase_transient(start_state, phase, duration)
            for end_state, prob in local.items():
                result[end_state] += mass * prob
        return self._normalize_distribution(result)

    def _phase_transient(self, start_state: State, phase: int, duration: float) -> Dict[State, float]:
        reachable = self._enumerate_phase_states(start_state, phase)
        idx = {state: i for i, state in enumerate(reachable)}
        q = np.zeros((len(reachable), len(reachable)), dtype=float)
        for state in reachable:
            row = idx[state]
            if self.is_top_failed(state):
                continue
            total_rate = 0.0
            for event, rate in self._enabled_events(state, phase):
                next_state = self.transition(state, event)
                q[row, idx[next_state]] += rate
                total_rate += rate
            q[row, row] -= total_rate
        p0 = np.zeros(len(reachable), dtype=float)
        p0[idx[start_state]] = 1.0
        # In this non-repairable model each enabled BE can fire at most once.
        # Summing at least this many Poisson terms preserves paths whose mass
        # is much smaller than the requested Poisson-tail tolerance.
        min_terms = sum(1 for _ in self._enabled_events(start_state, phase))
        pt = self._uniformize(p0, q, duration, min_terms=min_terms)
        result: Dict[State, float] = defaultdict(float)
        for state, i in idx.items():
            if math.isfinite(float(pt[i])) and pt[i] > 0.0:
                result[state] += float(pt[i])
        return self._normalize_distribution(result)

    def _enumerate_phase_states(self, start_state: State, phase: int) -> List[State]:
        queue: deque[State] = deque([start_state])
        seen = {start_state}
        order = [start_state]
        while queue:
            state = queue.popleft()
            if self.is_top_failed(state):
                continue
            for event, rate in self._enabled_events(state, phase):
                if rate <= 0.0:
                    continue
                next_state = self.transition(state, event)
                if next_state not in seen:
                    seen.add(next_state)
                    order.append(next_state)
                    queue.append(next_state)
                    if len(order) > self.state_limit:
                        raise RuntimeError("reachable state count exceeded limit {}".format(self.state_limit))
        return order

    def _enabled_events(self, state: State, phase: int) -> Iterable[Tuple[str, float]]:
        for name, rates in self.model.basic_events.items():
            if self._is_be_failed(state.failed_mask, name):
                continue
            rate = float(rates.get(phase, 0.0))
            if rate > 0.0:
                yield name, rate

    def _uniformize(self, p0, q, duration: float, min_terms: int = 0):
        if duration <= 0.0:
            return p0.copy()
        rates = -np.diag(q)
        q_rate = float(np.max(rates)) if len(rates) else 0.0
        if q_rate <= 0.0:
            return p0.copy()
        p = np.eye(q.shape[0], dtype=float) + q / q_rate
        qt = q_rate * duration
        term = p0.copy()
        weight = math.exp(-qt)
        cumulative_weight = weight
        result = weight * term
        cap = int(max(50, min_terms, qt + 20.0 * math.sqrt(qt + 1.0) + 20.0))
        for n in range(1, cap + 1):
            term = term @ p
            weight *= qt / float(n)
            cumulative_weight += weight
            result += weight * term
            remaining_weight = max(0.0, 1.0 - cumulative_weight)
            if n >= min_terms and n > qt and remaining_weight <= self.poisson_tol:
                break
        return result

    def _normalize_distribution(self, distribution: Dict[State, float]) -> Dict[State, float]:
        positive = {
            state: float(prob)
            for state, prob in distribution.items()
            if math.isfinite(float(prob)) and prob > 0.0
        }
        total = sum(positive.values())
        if total <= 0.0:
            raise RuntimeError("distribution became empty")
        return {state: prob / total for state, prob in positive.items()}


def build_analytic_model(tree: Tree) -> AnalyticModel:
    meta = _extract_phase_meta(tree)
    top_node = next(tree.find_data("toplevel"))
    top_name = _name(top_node.children[0])
    basic_events = _collect_basic_events(tree, meta)
    gates = _collect_gates(tree)
    topo_gates = _topological_gates(top_name, gates)
    if top_name not in gates and top_name not in basic_events:
        raise RuntimeError("top event '{}' is not a BE or supported gate".format(top_name))
    pand_names = [name for name in topo_gates if gates[name].kind == "pand"]
    be_names = sorted(basic_events)
    be_index = {name: idx for idx, name in enumerate(be_names)}
    return AnalyticModel(
        top_name=top_name,
        basic_events=basic_events,
        gates=gates,
        topo_gates=topo_gates,
        pand_names=pand_names,
        phase_meta=meta,
        be_index=be_index,
    )


def _extract_phase_meta(tree: Tree) -> PhaseMeta:
    boundaries: List[float] = []
    for node in tree.find_data("phases_def"):
        boundaries = [float(str(child)) for child in node.children]
        break
    if boundaries:
        prev = 0.0
        durations: List[float] = []
        for bound in boundaries:
            durations.append(bound - prev)
            prev = bound
        num_phases = len(boundaries) + 1
    else:
        durations = []
        num_phases = 1
    phase_overrides: Dict[str, Dict[str, Dict[int, str]]] = {}
    phase_actives: Dict[int, Set[str]] = {}
    has_active_lists = False
    for block in tree.find_data("phase_block"):
        if len(block.children) < 2:
            continue
        phase_idx = int(str(block.children[0]))
        for node in block.children[1:]:
            if not isinstance(node, Tree):
                continue
            if node.data == "active_decl":
                has_active_lists = True
                for child in node.children:
                    phase_actives.setdefault(phase_idx, set()).add(_name(child))
                continue
            if node.data in {"be_override", "sbe_override"}:
                node_name = _name(node.children[0])
                for spec in node.children[1:]:
                    if isinstance(spec, Tree) and spec.data in {"fail", "rep", "dorm"}:
                        phase_overrides.setdefault(node_name, {}).setdefault(spec.data, {})[phase_idx] = _dist_to_str(spec.children[0])
    if has_active_lists:
        missing = [p for p in range(1, num_phases + 1) if p not in phase_actives]
        if missing:
            raise RuntimeError("missing active lists for phases {}".format(",".join(str(x) for x in missing)))
    inherit_map: Dict[str, Tuple[str, int]] = {}
    for node in tree.find_data("reuse_decl"):
        chain = [_name(child) for child in node.children]
        if len(chain) < 2:
            continue
        if len(chain) != num_phases:
            raise RuntimeError("reuse chain length must match phase count")
        for i in range(1, len(chain)):
            inherit_map[chain[i]] = (chain[i - 1], i + 1)
    return PhaseMeta(
        num_phases=num_phases,
        phase_durations=durations,
        phase_overrides=phase_overrides,
        phase_actives=phase_actives,
        has_active_lists=has_active_lists,
        inherit_map=inherit_map,
    )


def _collect_basic_events(tree: Tree, meta: PhaseMeta) -> Dict[str, Dict[int, float]]:
    result: Dict[str, Dict[int, float]] = {}
    num_phases = max(meta.num_phases, 1)
    if any(True for _ in tree.find_data("sbe")):
        raise RuntimeError("Yu analytic report currently supports BE-only exact semantics; SBE/spare IOSA simulation remains available")
    for node in tree.find_data("be"):
        name = _name(node.children[0])
        fail_tree = None
        repair_tree = None
        for item in node.children[1:]:
            if isinstance(item, Tree):
                if item.data == "fail":
                    fail_tree = item.children[0]
                elif item.data == "rep":
                    repair_tree = item.children[0]
        if fail_tree is None:
            raise RuntimeError("missing fail distribution for BE {}".format(name))
        if repair_tree is not None:
            rep = _dist_to_str(repair_tree)
            if not rep.startswith("dirac("):
                raise RuntimeError("Yu exact analytic path assumes non-repairable BEs; found repair {} on {}".format(rep, name))
        base_rate = _parse_exponential_rate(_dist_to_str(fail_tree))
        overrides = meta.phase_overrides.get(name, {}).get("fail", {})
        rates: Dict[int, float] = {}
        for phase in range(1, num_phases + 1):
            if phase in overrides:
                rate = _parse_exponential_rate(overrides[phase])
            else:
                rate = base_rate
            if meta.has_active_lists and name not in meta.phase_actives.get(phase, set()):
                rate = 0.0
            rates[phase] = rate
        result[name] = rates
    return result


def _collect_gates(tree: Tree) -> Dict[str, GateSpec]:
    result: Dict[str, GateSpec] = {}
    for node in tree.find_data("or_gate"):
        result[_name(node.children[0])] = GateSpec(
            name=_name(node.children[0]),
            kind="or",
            children=tuple(_name(x) for x in node.children[1:]),
        )
    for node in tree.find_data("and_gate"):
        result[_name(node.children[0])] = GateSpec(
            name=_name(node.children[0]),
            kind="and",
            children=tuple(_name(x) for x in node.children[1:]),
        )
    for node in tree.find_data("vot_gate"):
        result[_name(node.children[0])] = GateSpec(
            name=_name(node.children[0]),
            kind="vot",
            children=tuple(_name(x) for x in node.children[3:]),
            k=int(str(node.children[1])),
        )
    for node in tree.find_data("pand_gate"):
        children = tuple(_name(x) for x in node.children[1:])
        if len(children) != 2:
            raise RuntimeError("Yu analytic report supports binary PAND only; cascade larger PANDs")
        result[_name(node.children[0])] = GateSpec(
            name=_name(node.children[0]),
            kind="pand",
            children=children,
        )
    unsupported: List[str] = []
    for kind in ["spare_gate", "repair_box"]:
        for node in tree.find_data(kind):
            unsupported.append(_name(node.children[0]))
    if unsupported:
        raise RuntimeError("Yu analytic report currently supports OR/AND/VOT/PAND gates only; unsupported: {}".format(",".join(sorted(unsupported))))
    return result


def _topological_gates(top_name: str, gates: Dict[str, GateSpec]) -> List[str]:
    order: List[str] = []
    seen: Set[str] = set()

    def dfs(name: str) -> None:
        if name not in gates or name in seen:
            return
        seen.add(name)
        for child in gates[name].children:
            dfs(child)
        order.append(name)

    dfs(top_name)
    return order


def _update_pand_status(status: int, left_old: bool, left_new: bool,
                        right_old: bool, right_new: bool) -> int:
    if status in {PAND_FAILED, PAND_INVALID}:
        return status
    left_rise = (not left_old) and left_new
    right_rise = (not right_old) and right_new
    if status == PAND_IDLE:
        if left_rise and right_rise:
            return PAND_INVALID
        if right_rise:
            return PAND_FAILED if left_old else PAND_INVALID
        if left_rise:
            return PAND_WAITING
        return PAND_IDLE
    if status == PAND_WAITING:
        if right_rise:
            return PAND_FAILED
        return PAND_WAITING
    return status


def _dist_to_str(dist_tree: object) -> str:
    if not isinstance(dist_tree, Tree):
        return str(dist_tree)
    data = dist_tree.data
    args = [str(child) for child in dist_tree.children]
    if data == "exponential_dist":
        return "exponential({})".format(args[0])
    if data == "dirac_dist":
        return "dirac({})".format(args[0])
    if data == "erlang_dist":
        return "erlang({},{})".format(args[0], args[1])
    if data == "uniform_dist":
        return "uniform({},{})".format(args[0], args[1])
    if data == "normal_dist":
        return "normal({},{})".format(args[0], args[1])
    if data == "lognormal_dist":
        return "lognormal({},{})".format(args[0], args[1])
    if data == "gamma_dist":
        return "gamma({},{})".format(args[0], args[1])
    if data == "weibull_dist":
        return "weibull({},{})".format(args[0], args[1])
    if data == "rayleigh_dist":
        return "rayleigh({})".format(args[0])
    raise RuntimeError("unsupported distribution {}".format(data))


def _parse_exponential_rate(text: str) -> float:
    raw = str(text).strip()
    if not raw.startswith("exponential(") or not raw.endswith(")"):
        raise RuntimeError("Yu exact analytic path supports exponential failure distributions only; found {}".format(raw))
    return float(raw[len("exponential("):-1].strip())


def _name(obj: object) -> str:
    if isinstance(obj, Token):
        return obj.value
    return str(obj)


def _fmt_float(value: float) -> str:
    return "{:.12g}".format(float(value))
