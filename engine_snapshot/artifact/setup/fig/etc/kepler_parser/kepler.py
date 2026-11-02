#!/usr/bin/env python
#
# The Kepler grammar using Lark parser library
#
# Author:  Raul E. Monti
# Date:    15.10.2021
# License: GPLv3
#
from lark import Lark
from lark import Transformer
from lark import Visitor
from lark import Tree
from lark import Token
from lark.visitors import Discard
from iosa import IosaApi
from numpy import lcm as nplcm
from itertools import combinations, product
from pyeda.inter import expr
from pyeda.boolalg.expr import Variable as PyedaVariable
import math
import os
import re

# FIXME: when replacing FDEP we break the tree and then visitors don't work properly.


class HeuristicTuning:

    def __init__(self, phys_k=10, phys_beta=1.0, phys_gamma=1.0, pand_eta=1.0):
        self.phys_k = max(int(phys_k), 2)
        self.phys_beta = float(phys_beta)
        self.phys_gamma = float(phys_gamma)
        self.pand_eta = float(pand_eta)


class PwstrTuning:

    def __init__(self, k=1000, beta_req=0.5, beta_alt=0.5,
                 a_min=0.5, a_max=2.0, b_min=0.5, b_max=2.0, epsilon=1e-300):
        self.k = max(int(k), 2)
        self.beta_req = float(beta_req)
        self.beta_alt = float(beta_alt)
        self.a_min = float(a_min)
        self.a_max = float(a_max)
        self.b_min = float(b_min)
        self.b_max = float(b_max)
        self.epsilon = float(epsilon)


class T3Tuning:

    def __init__(self, phys_k=10, eta=1.0, k=1000, epsilon=1e-300):
        self.phys_k = max(int(phys_k), 2)
        self.eta = float(eta)
        self.k = max(int(k), 2)
        self.epsilon = float(epsilon)


class RDGTParams:

    def __init__(self, alpha_min=None,
                 rare_low=None, rare_high=None,
                 hetero_low=None, hetero_high=None,
                 bottleneck_low=None, bottleneck_high=None,
                 inf_mult=None, fixed_alpha_flag=None,
                 fixed_alpha_value=None, read_env=False):
        self.alpha_min = self._float(alpha_min, "N4A_ALPHA_MIN", 0.10, read_env)
        self.rare_low = self._float(rare_low, "N4A_RARE_LOW", 3.0, read_env)
        self.rare_high = self._float(rare_high, "N4A_RARE_HIGH", 8.0, read_env)
        self.hetero_low = self._float(hetero_low, "N4A_HETERO_LOW", 3.0, read_env)
        self.hetero_high = self._float(hetero_high, "N4A_HETERO_HIGH", 30.0, read_env)
        self.bottleneck_low = self._float(bottleneck_low, "N4A_BOTTLENECK_LOW", 0.40, read_env)
        self.bottleneck_high = self._float(bottleneck_high, "N4A_BOTTLENECK_HIGH", 0.70, read_env)
        self.inf_mult = self._float(inf_mult, "N4A_INF_MULT", 10.0, read_env)
        self.fixed_alpha_flag = self._bool(fixed_alpha_flag, "N4A_FIXED_ALPHA_FLAG", False, read_env)
        self.fixed_alpha_value = self._float(fixed_alpha_value, "N4A_FIXED_ALPHA_VALUE", 0.0, read_env)

    @staticmethod
    def _float(value, env_name, default, read_env):
        if value is None:
            value = os.environ.get(env_name, default) if read_env else default
        return float(value)

    @staticmethod
    def _bool(value, env_name, default, read_env):
        if value is None:
            value = os.environ.get(env_name, default) if read_env else default
        if isinstance(value, bool):
            return value
        return str(value).strip().lower() in ("1", "true", "yes", "y", "on")


kepler_parser = Lark(r"""
    toplevel : "toplevel" name ";" phases_def? (kepler | galileo) reuse_decl* phase_block*

    phases_def : "phases" REAL ("," REAL)* ";"        -> phases_def

    reuse_decl : "reuse" name+ ";"                    -> reuse_decl

    phase_block : "phase" INT "{" (active_decl | be_override | sbe_override)+ "}"  -> phase_block
    active_decl : "active" name+ ";"                   -> active_decl
    be_override : name fail ";"                       -> be_override
                | name (fail rep | rep fail) ";"      -> be_override
    sbe_override : name (dorm fail | fail dorm) ";"   -> sbe_override
                | name (dorm fail rep | dorm rep fail | fail dorm rep | fail rep dorm | rep fail dorm | rep dorm fail) ";"  -> sbe_override
             
    kepler   : (gate
             | be
             | sbe)+
    gate     : name "or"    name+ ";"        -> or_gate
             | name "and"   name+ ";"        -> and_gate
             | name  INT"of"INT name+ ";"    -> vot_gate
             | name "pand"  name+ ";"        -> pand_gate
             | name "fdep"  name+ ";"        -> fdep_gate
             | name "spare" name+ ";"        -> spare_gate
             | name "rbox"  RMODE name+ ";"  -> repair_box
    be       : name fail ";"
             | name (fail rep | rep fail) ";"
    sbe      : name (dorm fail | fail dorm) ";"
             | name (dorm fail rep | dorm rep fail | fail dorm rep | fail rep dorm | rep fail dorm | rep dorm fail) ";"
    RMODE    : "prio" | "fcfs" | "rand"
    fail    : "fail"   "~" dist            
    rep     : "repair" "~" dist
    dorm    : "dorm"   "~" dist
   
   
    galileo : (ggate
             | gbe)+
    ggate    : name "seq" name+ ";"          -> galileo_seq_gate
             | name "csp" name+ ";"          -> galileo_csp_gate
             | name "wsp" name+ ";"          -> galileo_wsp_gate
             | name "hsp" name+ ";"          -> galileo_hsp_gate
    gbe      : name gfail ";"                -> galileo_be
             | name gfail gdorm ";"          -> galileo_be
             | name gdorm gfail ";"          -> galileo_be
    gfail    : "lambda" "=" NUMBER
             | "prob"   "=" PROB
             | "res"    "=" PROB
             | "rate"   "=" NUMBER "shape" "=" NUMBER
             | "mean"   "=" REAL  "stddev" "=" NUMBER
    gdorm    : "dorm"   "=" PROB


    ?name     : "\"" CNAME "\""
    dist     : "exponential" "(" NUMBER ")"             -> exponential_dist
             | "erlang"      "(" INT   "," NUMBER  ")"  -> erlang_dist
             | "uniform"     "(" REAL  "," REAL    ")"  -> uniform_dist
             | "normal"      "(" REAL  "," NUMBER  ")"  -> normal_dist
             | "lognormal"   "(" REAL  "," NUMBER  ")"  -> lognormal_dist
             | "gamma"       "(" NUMBER "," NUMBER ")"  -> gamma_dist
             | "weibull"     "(" NUMBER "," NUMBER ")"  -> weibull_dist
             | "rayleigh"    "(" NUMBER ")"             -> rayleigh_dist
             | "dirac"       "(" NUMBER ")"             -> dirac_dist
    PROB     : "0" "." INT | "0" | "1"
    REAL     : ["+" | "-"] NUMBER
    SGNINT   : ["+" | "-"] INT

    COMMENT  : /\/\/.*/ | /\/\*.*\*\//
    
    %import common.CNAME
    %import common.INT
    %import common.DIGIT
    %import common.WS
    %ignore COMMENT
    %import common.FLOAT
    %import common.NUMBER
    %ignore WS
    
    """, start = "toplevel")

    
def type_to_prefix(t):
    result = ""
    if t == "and_gate":
        result = "AND_"
    elif t == "or_gate":
        result = "OR_"
    elif t == "pand_gate":
        result = "PAND_"
    elif t == "vot_gate":
        result = "VOT_"
    elif t == "spare_gate":
        result = "SPARE_"
    elif t == "be":
        result = "BE_"
    elif t == "sbe":
        result = "SBE_"
    elif t == "repair_box":
        result = "RBOX_"
    elif t == "fdep_gate":
        result = "FDEP_"
    else:
        print("Non supported node type {}".format(t))
        # raise Exception("Non supported node type {}".format(t))
        result = "ERROR_"
    return result

def get_n_inputs(tree):
    n = len(tree.children)
    if tree.data == "vot_gate":
        return n - 3
    elif tree.data == "repair_box":
        return n - 2
    else:
        return n - 1

def div(d, dd):
    """ Simplifies away divisions by 1 and some other cases. """
    if str(dd) == "0":
        return "ERROR: div by 0"
    elif str(dd) == "1":
        return "{}".format(d)
    elif str(d) == str(dd):
        return "1"
    else:
        res = "({}/{})".format(d,dd) 
        try:
            res = int(eval(res))
        except:
            pass
        return res
        
def prod(m, mm):
    """ Simplifies away multiplications by 1. """
    if str(m) == "1":
        return str(mm)
    elif str(mm) == "1":
        return str(m)
    elif str(m) == "-1":
        return "-{}".format(mm)
    elif str(mm) == "-1":
        return "-{}".format(m)
    else:
        res = ("({}*{})").format(m,mm)
        try:
            res = int(eval(res))
        except:
            pass
        return res
        
def mx(ls):
    if len(ls) > 1:
        result = "max(" +  ','.join(ls) + ')'
    else:
        result = ls[0]
    return result

def _to_float(value, default=0.0):
    try:
        return float(str(value))
    except:
        return default

def _dist_tree_proxy(dist_tree):
    if not isinstance(dist_tree, Tree):
        return 1.0
    _args = [_to_float(x, 0.0) for x in dist_tree.children]
    _kind = dist_tree.data
    if _kind == "exponential_dist":
        return max(_args[0], 0.0) if _args else 0.0
    if _kind == "erlang_dist":
        if len(_args) >= 2 and _args[0] > 0 and _args[1] > 0:
            return _args[1] / _args[0]
        return 0.0
    if _kind == "uniform_dist":
        if len(_args) >= 2:
            _mean = (_args[0] + _args[1]) / 2.0
            return 1.0 / _mean if _mean > 0 else 0.0
        return 0.0
    if _kind == "normal_dist":
        if _args and _args[0] > 0:
            return 1.0 / _args[0]
        return 0.0
    if _kind == "lognormal_dist":
        if len(_args) >= 2:
            _mean = math.exp(_args[0] + ((_args[1] ** 2) / 2.0))
            return 1.0 / _mean if _mean > 0 else 0.0
        return 0.0
    if _kind == "gamma_dist":
        if len(_args) >= 2 and _args[0] > 0 and _args[1] > 0:
            return 1.0 / (_args[0] * _args[1])
        return 0.0
    if _kind == "weibull_dist":
        if len(_args) >= 2 and _args[0] > 0 and _args[1] > 0:
            _scale = _args[0]
            _shape = _args[1]
            _mean = _scale * math.gamma(1.0 + 1.0 / _shape)
            return 1.0 / _mean if _mean > 0 else 0.0
        return 0.0
    if _kind == "rayleigh_dist":
        if _args and _args[0] > 0:
            _mean = _args[0] * math.sqrt(math.pi / 2.0)
            return 1.0 / _mean if _mean > 0 else 0.0
        return 0.0
    if _kind == "dirac_dist":
        if _args and _args[0] > 0:
            return 1.0 / _args[0]
        return 0.0
    return 1.0

def _dist_str_proxy(dist_str):
    _m = re.match(r"\s*([a-zA-Z_][a-zA-Z0-9_]*)\((.*)\)\s*$", str(dist_str))
    if not _m:
        return 1.0
    _kind = _m.group(1)
    _parts = [x.strip() for x in _m.group(2).split(',') if x.strip() != ""]
    _args = [_to_float(x, 0.0) for x in _parts]
    return _dist_tree_proxy(Tree(_kind + "_dist", _args))
    
class TransformFDEP(Visitor):
    
    def __init__(self, tree):
        self.fdep_map = {}
        self.find_fdeps(tree)
    
    def find_fdeps(self,tree):
        for e in tree.find_data("fdep_gate"):
            _name, _main, *_ch = e.children
            _name = _name.value
            _main = _main.value
            for be in _ch:
                self.fdep_map.setdefault(be.value, []).append(_main)

    def make_or_gate(self, name, *ch):
        _children = []
        for c in [name] + list(ch):
            _t = Token('CNAME', str(c))
            _children.append(_t)
        return Tree('or_gate',  _children)
    
    def name_for_or_gate(self, be_name):
        return "OR_FROM_FDEP_" + be_name
    
    def replace_be_by_or(self, children):
        for i,e in enumerate(children):
            try:
                _deps = self.fdep_map[str(e)]
                tree.children[i] = Token('CNAME', self.name_for_or_gate(e.value))
            except:
                pass
    
    def and_gate(self, tree):
        self.replace_be_by_or(tree.children[1::])

    def or_gate(self, tree):
        self.replace_be_by_or(tree.children[1::])
        
    def vot_gate(self, tree):
        self.replace_be_by_or(tree.children[3::])

    def pand_gate(self, tree):
        self.replace_be_by_or(tree.children[1::])
        
    def kepler(self, tree):
        for k, v in self.fdep_map.items():
            name = self.name_for_or_gate(str(k))
            gate = self.make_or_gate(name, *(v+[k]))
            tree.children.append(gate)

#
# class Rename(Visitor):
#
#     def __init__(self, tree):
#         self.type_map = {}
#         self.get_types(tree)
#
#     def get_types(self, tree):
#         """ The type is just the keyword to prepend to
#         the corresponding IOSA model for it to
#         recognise it as such a component.
#         """
#         try:
#             lang_node = next(tree.find_data("kepler"))
#         except:
#             lang_node = next(tree.find_data("galileo"))
#         for node in lang_node.children:
#             self.type_map[node.children[0]] = type_to_prefix(node.data)
#
#     def rename_children(self, children, j=0):
#         for i, name in enumerate(children[j::], j):
#             children[i] = Token('CNAME', self.type_map[name] + name)
#
#     def toplevel(self, tree):
#         name = tree.children[0]
#         tree.children[0] = Token('CNAME', self.type_map[name] + name)
#
#     def be(self, tree):
#         name = tree.children[0]
#         tree.children[0] = Token('CNAME', self.type_map[name] + name)
#
#     def sbe(self, tree):
#         name = tree.children[0]
#         tree.children[0] = Token('CNAME', self.type_map[name] + name)
#
#     def and_gate(self, tree):
#         self.rename_children(tree.children)
#
#     def or_gate(self, tree):
#         self.rename_children(tree.children)
#
#     def pand_gate(self, tree):
#         self.rename_children(tree.children)
#
#     def vot_gate(self, tree):
#         name = tree.children[0]
#         tree.children[0] = Token('CNAME', self.type_map[name] + name)
#         self.rename_children(tree.children, 3)
#
#     def spare_gate(self, tree):
#         self.rename_children(tree.children)
#
#     def repair_box(self, tree):
#         name = tree.children[0]
#         tree.children[0] = Token('CNAME', self.type_map[name] + name)
#         self.rename_children(tree.children, 2)
#
#     def fdep_gate(self, tree):
#         self.rename_children(tree.children)


class CompilePass(Transformer):
    """
    Final pass, should be able to transform
    a processed DFT into a valid IOSA model
    using the IOSA module API.
    """
    
    def __init__(self, tree):
        self.num_phases = 0
        self.phase_durations = []
        self.phase_overrides = {}
        self.phase_actives = {}
        self.has_active_lists = False
        self.reuse_chains = []
        self.inherit_map = {}
        self._extract_phase_info(tree)
        self.api = IosaApi(self.num_phases, self.phase_durations)
        self.sbe_2_gates = {}
        self.fill_gates(tree)

    def _dist_to_str(self, dist_tree):
        """Convert a raw 'dist' parse subtree into an IOSA distribution string."""
        if not isinstance(dist_tree, Tree):
            return str(dist_tree)
        dt = dist_tree.data
        a = dist_tree.children
        if dt == "exponential_dist":
            return IosaApi().make_dist_exponential(a[0])
        if dt == "erlang_dist":
            return IosaApi().make_dist_erlang(a[0], a[1])
        if dt == "uniform_dist":
            return IosaApi().make_dist_uniform(a[0], a[1])
        if dt == "normal_dist":
            return IosaApi().make_dist_normal(a[0], a[1])
        if dt == "lognormal_dist":
            return IosaApi().make_dist_lognormal(a[0], a[1])
        if dt == "gamma_dist":
            return IosaApi().make_dist_gamma(a[0], a[1])
        if dt == "weibull_dist":
            return IosaApi().make_dist_weibull(a[0], a[1])
        if dt == "rayleigh_dist":
            return IosaApi().make_dist_rayleigh(a[0])
        if dt == "dirac_dist":
            return IosaApi().make_dist_dirac(a[0])
        raise Exception("Unsupported distribution type {}".format(dt))

    def _extract_phase_info(self, tree):
        boundaries = []
        for pdef in tree.find_data("phases_def"):
            boundaries = [float(str(x)) for x in pdef.children]
            break
        if not boundaries:
            self.num_phases = 0
            self.phase_durations = []
        else:
            prev = 0.0
            durations = []
            for t in boundaries:
                durations.append(t - prev)
                prev = t
            self.num_phases = len(boundaries) + 1
            self.phase_durations = durations

        if self.num_phases <= 1:
            return

        for pblock in tree.find_data("phase_block"):
            if len(pblock.children) < 2:
                continue
            phase_idx = int(str(pblock.children[0]))
            for node in pblock.children[1:]:
                if not isinstance(node, Tree):
                    continue

                if node.data == "active_decl":
                    self.has_active_lists = True
                    for n in node.children:
                        if isinstance(n, Token):
                            n = n.value
                        self.phase_actives.setdefault(phase_idx, set()).add(n)
                    continue

                if node.data in {"be_override", "sbe_override"}:
                    node_name = node.children[0]
                    if isinstance(node_name, Token):
                        node_name = node_name.value
                    for spec in node.children[1:]:
                        if not isinstance(spec, Tree):
                            continue
                        if spec.data not in {"fail", "rep", "dorm"}:
                            continue
                        dist_tree = spec.children[0]
                        dist_str = self._dist_to_str(dist_tree)
                        self.phase_overrides.setdefault(node_name, {}) \
                            .setdefault(spec.data, {})[phase_idx] = dist_str

        if self.has_active_lists:
            missing = [p for p in range(1, self.num_phases + 1) if p not in self.phase_actives]
            if missing:
                raise Exception(
                    "Active lists must be provided for all phases when using 'active ...;'. Missing phases: {}".format(
                        ",".join([str(x) for x in missing])
                    )
                )

        for r in tree.find_data("reuse_decl"):
            chain = []
            for n in r.children:
                if isinstance(n, Token):
                    n = n.value
                chain.append(n)
            if len(chain) < 2:
                continue
            self.reuse_chains.append(chain)

        if self.reuse_chains and self.num_phases <= 1:
            raise Exception("Found 'reuse' declarations but phases are not defined")

        for chain in self.reuse_chains:
            if len(chain) != self.num_phases:
                raise Exception(
                    "Reuse chain length must match number of phases. Got chain of length {} with {} phases".format(
                        len(chain), self.num_phases
                    )
                )
            for i in range(1, len(chain)):
                pred = chain[i - 1]
                succ = chain[i]
                phase_idx = i + 1
                if succ in self.inherit_map:
                    raise Exception("Node '{}' is in more than one reuse chain".format(succ))
                self.inherit_map[succ] = (pred, phase_idx)
        
    def fill_gates(self, tree):
        for e in tree.find_data("spare_gate"):
            for sbe in e.children[2::]:
                self.sbe_2_gates.setdefault(sbe, []).append(e.children[0])
    
    def toplevel(self, args) :
        # args: name ; [phases_def]? ; (kepler|galileo) ; phase_block*
        # keep only the compiled model (kepler/galileo) as returned value
        for a in args:
            if isinstance(a, Tree):
                return a
        return args[-1]

    def phases_def(self, args):
        return Discard

    def phase_block(self, args):
        return Discard

    def reuse_decl(self, args):
        return Discard

    def active_decl(self, args):
        return Discard

    def be_override(self, args):
        return Discard

    def sbe_override(self, args):
        return Discard

    def be(self, args):
        d = {}
        for e in args[1::]:
            d[e.data] = e.children[0]
        fail = d['fail']
        rep = d['rep'] if len(args) > 2 else ""
        if self.num_phases > 1:
            fail_map = {p: fail for p in range(1, self.num_phases + 1)}
            rep_map = {p: rep for p in range(1, self.num_phases + 1)}
            node_ovr = self.phase_overrides.get(args[0], {})
            for p, v in node_ovr.get('fail', {}).items():
                fail_map[int(p)] = v
            for p, v in node_ovr.get('rep', {}).items():
                rep_map[int(p)] = v
            active_map = None
            if self.has_active_lists:
                active_map = {p: (args[0] in self.phase_actives.get(p, set())) for p in range(1, self.num_phases + 1)}
            inherit = self.inherit_map.get(args[0])
            if inherit is not None:
                return self.api.make_be(args[0], fail_map, rep_map, active_map, inherit_fail_from=inherit[0], inherit_at_phase=inherit[1])
            return self.api.make_be(args[0], fail_map, rep_map, active_map)
        return self.api.make_be(args[0], fail, rep)
    
    def sbe(self, args):
        _gates = [x.value for x in self.sbe_2_gates[args[0]]]
        d = {}
        for e in args[1::]:
            d[e.data] = e.children[0]
        dorm = d['dorm']
        fail = d['fail']
        rep = d['rep'] if len(args) > 3 else ""
        if self.num_phases > 1:
            dorm_map = {p: dorm for p in range(1, self.num_phases + 1)}
            fail_map = {p: fail for p in range(1, self.num_phases + 1)}
            rep_map = {p: rep for p in range(1, self.num_phases + 1)}
            node_ovr = self.phase_overrides.get(args[0], {})
            for p, v in node_ovr.get('dorm', {}).items():
                dorm_map[int(p)] = v
            for p, v in node_ovr.get('fail', {}).items():
                fail_map[int(p)] = v
            for p, v in node_ovr.get('rep', {}).items():
                rep_map[int(p)] = v
            active_map = None
            if self.has_active_lists:
                active_map = {p: (args[0] in self.phase_actives.get(p, set())) for p in range(1, self.num_phases + 1)}
            inherit = self.inherit_map.get(args[0])
            if inherit is not None:
                return self.api.make_sbe(args[0], dorm_map, fail_map, rep_map, _gates, active_map, inherit_fail_from=inherit[0], inherit_at_phase=inherit[1])
            return self.api.make_sbe(args[0], dorm_map, fail_map, rep_map, _gates, active_map)
        return self.api.make_sbe(args[0], dorm, fail, rep, _gates)
        
    def and_gate(self, args):
        return self.api.make_and(args[0], args[1:])

    def or_gate(self, args):
        return self.api.make_or(args[0], args[1:])
    
    def pand_gate(self, args):
        return self.api.make_pand(args[0], args[1:])
        
    def vot_gate(self, args):
        return self.api.make_vot(args[0], args[1], args[3::])
        
    def fdep_gate(self, args):
        return Discard
        
    def spare_gate(self, args):
        return self.api.make_spare(args[0], args[1::])
    
    def repair_box(self, args):
        return self.api.make_rbox(args[0], args[1], args[2:])
    
    def uniform_dist(self, args):
        return self.api.make_dist_uniform(args[0], args[1])
        
    def exponential_dist(self, args):
        return self.api.make_dist_exponential(args[0])

    def weibull_dist(self, args):
        return self.api.make_dist_weibull(args[0], args[1])

    def lognormal_dist(self, args):
        return self.api.make_dist_lognormal(args[0], args[1])

    def normal_dist(self, args):
        return self.api.make_dist_normal(args[0], args[1])

    def gamma_dist(self, args):
        return self.api.make_dist_gamma(args[0], args[1])        

    def rayleigh_dist(self, args):
        return self.api.make_dist_rayleigh(args[0])

    def erlang_dist(self, args):
        return self.api.make_dist_erlang(args[0], args[1])
        
    def dirac_dist(self, args):
        return self.api.make_dist_dirac(args[0])
        
    def name(self, args):
        return args[0]
        
    def CNAME(self, name):
        return name.value
        
    def RMODE(self, mode):
        return mode.value


class Kepler2Tree(Visitor):
    
    class NodesNameDir(Visitor):
        """ Visits the a Parsed Kepler tree and builds a 
        dictionary where the keys are the names of the 
        nodes and the values are the actual Tree structures 
        representing the nodes. With this map you can, for
        instance, easily build a tree for further analysis.
    
        The resulting map is placed in self.dir and you
        can access it with get_dir().
        """
    
        def __init__(self):
            self.dir = {}
            return
    
        def get_dir(self):
            return self.dir
    
        def and_gate(self, tree):
            self.dir[tree.children[0].value] = tree
        
        def or_gate(self, tree):
            self.dir[tree.children[0].value] = tree

        def pand_gate(self, tree):
            self.dir[tree.children[0].value] = tree

        def vot_gate(self, tree):
            self.dir[tree.children[0].value] = tree
    
        def be(self, tree):
            self.dir[tree.children[0].value] = tree
    
        def sbe(self, tree):
            self.dir[tree.children[0].value] = tree

        def spare_gate(self, tree):
            self.dir[tree.children[0].value] = tree
    
    def __init__(self, k_tree):
        __dir_pass = self.NodesNameDir()
        __dir_pass.visit(k_tree)
        self.dir = __dir_pass.get_dir()
        
    def toplevel(self, tree):
        tree.children = [self.dir[tree.children[0].value]]
    
    def and_gate(self, tree):
        for i, e in enumerate(tree.children):
            if(i > 0):
                tree.children[i] = self.dir[e.value]
        
    def or_gate(self ,tree):
        for i, e in enumerate(tree.children):
            if(i > 0):
                tree.children[i] = self.dir[e.value]

    def pand_gate(self ,tree):
        for i, e in enumerate(tree.children):
            if(i > 0):
                tree.children[i] = self.dir[e.value]

    def vot_gate(self, tree):
        for i, e in enumerate(tree.children):
            if(i > 2):
                tree.children[i] = self.dir[e.value]

    def spare_gate(self, tree):
        for i, e in enumerate(tree.children):
            if(i > 0):
                tree.children[i] = self.dir[e.value]


class Tree2StructuralIfun(Visitor):
    """
    Defined in Budde et all. 'Rare event simulation for 
    non-Markovian repairable fault trees' (Table 1)
    """

    def __init__(self, active_guard=False):
        # If active_guard is True, BE/SBE terms are wrapped with the runtime
        # '<name>_enabled' boolean state to contribute 0 when disabled.
        self.active_guard = bool(active_guard)
        
    def lcm(self, *args):
        return nplcm.reduce(list(args))
    
    def lcmv(self, *args):
        return str(self.lcm(*[self.maxi(x) for x in args]))

    # TODO        
    def mini(self, tree):
        """Return the minimum structural importance value of a node"""
        return
    
    def maxi(self, tree):
        """ 
        Returns the maximum structural importance value of a node.
        Could optimise saving pre-calculated values in tree nodes, 
        but code is uglier and maybe not needed given the expected
        small size of trees.
        """
        result = -1
        
        try:
            tree.maxi
        except:
            if tree.data in {"be", "sbe"}:
                tree.maxi = 1   
            elif tree.data == "and_gate":
                lcm_v = self.lcm(*[self.maxi(x) for x in tree.children[1::]])
                tree.maxi = lcm_v * (len(tree.children) - 1)
            elif tree.data == "or_gate":
                tree.maxi = self.lcm(*[self.maxi(x) for x in tree.children[1::]])
            elif tree.data == "vot_gate":
                lcm_v = self.lcm(*[self.maxi(x) for x in tree.children[3::]])
                tree.maxi = lcm_v * int(tree.children[1])
            elif tree.data == "pand_gate":
                lcm_v = self.lcm(*[self.maxi(x) for x in tree.children[1::]])
                tree.maxi = lcm_v * 2
            elif tree.data == "spare_gate":
                lcm_v = self.lcm(*[self.maxi(x) for x in tree.children[1::]])
                tree.maxi = lcm_v * (len(tree.children) - 1)
        return tree.maxi 

    def toplevel(self, tree):
        tree.ifun = tree.children[0].ifun
        tree.maxi = self.maxi(tree.children[0])
        
    def be(self, tree):
        n = str(tree.children[0])
        base = "BE_" + n
        tree.ifun = base
        
    def sbe(self, tree):
        n = str(tree.children[0])
        base = "SBE_" + n
        tree.ifun = base

    def div(self, d, dd):
        """ Simplifies away divisions by 1 and some other cases. """
        if str(dd) == "0":
            return "ERROR: div by 0"
        elif str(dd) == "1":
            return "{}".format(d)
        elif str(d) == str(dd):
            return "1"
        else:
            res = "({}/{})".format(d,dd) 
            try:
                res = int(eval(res))
            except:
                pass
            return res
            
    def prod(self, m, mm):
        """ Simplifies away multiplications by 1. """
        if str(m) == "1":
            return str(mm)
        elif str(mm) == "1":
            return str(m)
        elif str(m) == "-1":
            return "-{}".format(mm)
        elif str(mm) == "-1":
            return "-{}".format(m)
        else:
            res = ("({}*{})").format(m,mm)
            try:
                res = int(eval(res))
            except:
                pass
            return res

    def and_gate(self, tree):
        _cs = tree.children[1::]
        _lcmv = self.lcmv(*_cs)
        _sum = []
        for e in _cs:
            if isinstance(e, Tree) and not hasattr(e, "ifun"):
                self.visit(e)
            _scale = div(_lcmv, self.maxi(e))
            _sum.append(prod(_scale, e.ifun))
        _sum = "(" + "+".join(_sum) + ")"
        tree.ifun = _sum

    def or_gate(self, tree):
        _cs = tree.children[1::]
        _lcmv = self.lcmv(*_cs)
        _max = []
        for e in _cs:
            if isinstance(e, Tree) and not hasattr(e, "ifun"):
                self.visit(e)
            _scale = div(_lcmv, self.maxi(e))
            _max.append(prod(_scale, e.ifun))
        _max = "max(" + ",".join(_max) + ")"
        tree.ifun = _max

    def pand_gate(self, tree):
        # Ensure children have been visited and have 'ifun' fields.
        # Lark's Visitor traversal order can be pre-order depending on version,
        # so we defensively visit children when needed.
        for c in tree.children[1:3]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)
        _cs = tree.children
        _name = _cs[0]
        _lcmv = self.lcmv(*_cs[1::])
        _scalel = div(_lcmv, self.maxi(_cs[1]))
        _suml = prod(_scalel, _cs[1].ifun)
        _scaler = div(_lcmv, self.maxi(_cs[2]))
        _scif = prod(_scaler,_cs[2].ifun)
        # Exprtk (used by FIG) doesn't accept C-like ternary operators.
        # Use boolean-as-numeric comparisons (0/1) instead.
        _p = "PAND_" + _name
        _sumr = "(({}>0)*({})-({}<=0)*({}))".format(_p, _scif, _p, _scif)
        _sum = "({}+{})".format(_suml, _sumr)
        _fig_max = 3 # max value assigned by FIG to the if of a pand
        _out = "({}=={})".format(_p, _fig_max)
        _max = "max({},({})*({}))".format(_sum, _out, prod(2, _lcmv))
        tree.ifun = _max

    def vot_gate(self, tree):
        _cs = tree.children
        _count = _cs[1]
        _from = _cs[2]
        _cs = _cs[3::]
        _lcmv = self.lcmv(*_cs)
        for x in _cs:
            if isinstance(x, Tree) and not hasattr(x, "ifun"):
                self.visit(x)
        _prod = [prod(div(_lcmv,self.maxi(x)),x.ifun) for x in _cs]
        _max = "summax({},{})".format(_count, ','.join(_prod))
        tree.ifun = _max

    def spare_gate(self, tree):
        _fig_max = 1 + 2 *(len(tree.children)-2) # max value assigned by FIG to the if of a spare
        _cs = tree.children
        _name = _cs[0]
        _cs = _cs[1::]
        _lcmv = self.lcmv(*_cs)
        for e in _cs:
            if isinstance(e, Tree) and not hasattr(e, "ifun"):
                self.visit(e)
        _sum = [prod(div(_lcmv, self.maxi(e)),e.ifun) for e in _cs]
        _sum = "(" + "+".join(_sum) + ")"
        _st = "({}=={})".format("SPARE_"+_name, _fig_max)
        _max = "max({},{})".format(_sum, prod(_st, prod(len(_cs), _lcmv)))
        tree.ifun = _max


class Tree2MCSIfun(Visitor):

    def __init__(self, active_guard=False):
        # If active_guard is True, BE/SBE boolean terms are guarded by the
        # runtime '<name>_enabled' boolean state.
        self.active_guard = bool(active_guard)

    def _ensure_children_ifun(self, tree, start_idx=1):
        for c in tree.children[start_idx:]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)

    def _guard_term(self, term):
        return term

    def _term(self, expr_str, maxi, sig):
        return {"expr": expr_str, "maxi": int(maxi), "sig": sig}

    def _sig_key(self, sig):
        return repr(sig)

    def _dedup_terms(self, terms):
        # 用结构签名去重，而不是仅按字符串去重，避免等价项重复出现。
        unique = {}
        for t in terms:
            unique[self._sig_key(t["sig"])] = t
        return list(unique.values())

    def _contains_pand(self, tree):
        if not isinstance(tree, Tree):
            return False
        if tree.data == "pand_gate":
            return True
        return any(self._contains_pand(c) for c in tree.children if isinstance(c, Tree))

    def _static_formula(self, tree):
        if tree.data == "toplevel":
            return self._static_formula(tree.children[0])
        if tree.data == "be":
            return "BE_" + str(tree.children[0])
        if tree.data == "sbe":
            return "SBE_" + str(tree.children[0])
        if tree.data == "and_gate":
            return '(' + '&'.join([self._static_formula(c) for c in tree.children[1::]]) + ')'
        if tree.data == "or_gate":
            return '(' + '|'.join([self._static_formula(c) for c in tree.children[1::]]) + ')'
        if tree.data == "vot_gate":
            comb = combinations([self._static_formula(e) for e in tree.children[3::]], int(tree.children[1]))
            return '(' + '|'.join(['&'.join(c) for c in comb]) + ')'
        if tree.data == "spare_gate":
            return '(' + '&'.join([self._static_formula(c) for c in tree.children[1::]]) + ')'
        raise Exception("Static MCS formula unsupported for node type {}".format(tree.data))

    def _terms_from_dnf(self, _dnf):
        # 把 pyeda 的 DNF 对象稳定拆成若干“子句 -> literal 列表”，
        # 避免 OR/AND 结构在不同节点类型下被误读。
        def _clause_literals(node):
            if isinstance(node, PyedaVariable):
                return [str(node)]
            _op = getattr(node, "ASTOP", None)
            if _op == "and":
                return [str(x) for x in getattr(node, "xs", [])]
            if _op in {None, "lit"}:
                return [str(node)]
            raise Exception("Unsupported literal/clause in DNF: {}".format(node))

        if isinstance(_dnf, PyedaVariable):
            result = [[str(_dnf)]]
        else:
            _op = getattr(_dnf, "ASTOP", None)
            if _op == "or":
                result = [_clause_literals(x) for x in getattr(_dnf, "xs", [])]
            elif _op == "and":
                result = [_clause_literals(_dnf)]
            elif str(_dnf) == "0":
                result = []
            elif str(_dnf) == "1":
                result = [[]]
            else:
                result = [_clause_literals(_dnf)]
        return [self._term('(' + '+'.join([self._guard_term(t) for t in x]) + ')',
                           len(x),
                           ("set", tuple(sorted(x)))) for x in result]

    def _static_mcs_terms(self, tree):
        # 不含 PAND 的子树可以直接走布尔化简 + DNF 提取最小割项。
        _flat = self._static_formula(tree)
        _dnf = expr(_flat).to_dnf()
        return self._terms_from_dnf(_dnf)

    def _combine_and_terms(self, terms):
        _expr = '(' + '+'.join([t["expr"] for t in terms]) + ')'
        _maxi = sum([t["maxi"] for t in terms])
        _sig = ("and", tuple(sorted([self._sig_key(t["sig"]) for t in terms])))
        return self._term(_expr, _maxi, _sig)

    def _combine_pand_terms(self, name, left_term, right_term):
        # PAND 需要保留“左先右后”的顺序信息：这里借助 IOSA 中的
        # PAND_<name> 辅助状态变量，把左右子项组合成顺序敏感的 IFUN 项。
        _p = "PAND_" + str(name)
        _lcmv = self.lcm(left_term["maxi"], right_term["maxi"])
        _lexpr = prod(div(_lcmv, left_term["maxi"]), left_term["expr"])
        _rexpr = prod(div(_lcmv, right_term["maxi"]), right_term["expr"])
        _rsigned = "(({}>0)*({})-({}<=0)*({}))".format(_p, _rexpr, _p, _rexpr)
        _full = prod("({}==3)".format(_p), _lcmv * (left_term["maxi"] + right_term["maxi"]))
        _expr = "max(({}+{}),{})".format(_lexpr, _rsigned, _full)
        _sig = ("pand", str(name), left_term["sig"], right_term["sig"])
        return self._term(_expr, _lcmv * (left_term["maxi"] + right_term["maxi"]), _sig)

    def _compute_terms(self, tree):
        # 递归构造 MCS 项：
        # - 无 PAND 子树：直接静态 DNF
        # - 含 PAND 子树：保留树结构逐层组合，避免把顺序信息丢平
        if hasattr(tree, "mcs_terms"):
            return tree.mcs_terms
        if tree.data == "toplevel":
            tree.mcs_terms = self._compute_terms(tree.children[0])
            return tree.mcs_terms
        if not self._contains_pand(tree):
            tree.mcs_terms = self._static_mcs_terms(tree)
            return tree.mcs_terms
        if tree.data in {"be", "sbe"}:
            base = ("BE_" if tree.data == "be" else "SBE_") + str(tree.children[0])
            tree.mcs_terms = [self._term(base, 1, ("atom", base))]
            return tree.mcs_terms
        if tree.data == "or_gate":
            _terms = []
            for c in tree.children[1::]:
                _terms.extend(self._compute_terms(c))
            tree.mcs_terms = self._dedup_terms(_terms)
            return tree.mcs_terms
        if tree.data == "and_gate":
            _parts = [self._compute_terms(c) for c in tree.children[1::]]
            _terms = [self._combine_and_terms(list(x)) for x in product(*_parts)]
            tree.mcs_terms = self._dedup_terms(_terms)
            return tree.mcs_terms
        if tree.data == "vot_gate":
            _parts = [self._compute_terms(c) for c in tree.children[3::]]
            _terms = []
            for combo in combinations(_parts, int(tree.children[1])):
                for choice in product(*combo):
                    _terms.append(self._combine_and_terms(list(choice)))
            tree.mcs_terms = self._dedup_terms(_terms)
            return tree.mcs_terms
        if tree.data == "spare_gate":
            _parts = [self._compute_terms(c) for c in tree.children[1::]]
            _terms = [self._combine_and_terms(list(x)) for x in product(*_parts)]
            tree.mcs_terms = self._dedup_terms(_terms)
            return tree.mcs_terms
        if tree.data == "pand_gate":
            _name = tree.children[0]
            _left = self._compute_terms(tree.children[1])
            _right = self._compute_terms(tree.children[2])
            # 左右子树的每个候选项都要成对组合，才能保留精确的顺序语义。
            _terms = [self._combine_pand_terms(_name, l, r) for l in _left for r in _right]
            tree.mcs_terms = self._dedup_terms(_terms)
            return tree.mcs_terms
        raise Exception("MCS IFUN unsupported for node type {}".format(tree.data))
        
    def ifun(self, tree):
        _mcs = self.get_mcs(tree)
        _max = max([x["maxi"] for x in _mcs]) if _mcs else 0
        _mcs = [x["expr"] for x in _mcs]
        _ifun = mx(_mcs) if _mcs else "0"
        return _ifun, 0, _max
        
    def ifun_n(self, tree):
        _mcs = self.get_mcs(tree)
        _count = [x["maxi"] for x in _mcs]
        if not _count:
            return "0", 0, 0
        _lcm = self.lcm(*_count)
        _max = _lcm
        _sum = [x["expr"] for x in _mcs]
        _prod = [prod(div(_lcm, _count[i]), _sum[i]) for i in range(0,len(_sum))]
        _ifun = mx(_prod)
        return _ifun, 0, _max
        
    def get_mcs(self, tree):
        return self._compute_terms(tree)
        
    def mcs_from_dnf(self, _dnf):
        return self._terms_from_dnf(_dnf)
  
    def maxi(self, tree):
        pass
        
    def lcm(self, *args):
        return nplcm.reduce(list(args))
    
    def toplevel(self, tree):
        tree.ifun = tree.children[0].ifun
        
    def be(self, tree):
        tree.ifun = "BE_" + str(tree.children[0])
        
    def sbe(self, tree):
        tree.ifun = "SBE_" + str(tree.children[0])

    def and_gate(self, tree):
        self._ensure_children_ifun(tree, start_idx=1)
        tree.ifun = '(' + '&'.join([c.ifun for c in tree.children[1::]]) + ')'

    def or_gate(self, tree):
        self._ensure_children_ifun(tree, start_idx=1)
        tree.ifun = '(' + '|'.join([c.ifun for c in tree.children[1::]]) + ')'

    def pand_gate(self, tree):
        self._ensure_children_ifun(tree, start_idx=1)
        tree.ifun = '(' + '&'.join([c.ifun for c in tree.children[1::]]) + ')'

    def vot_gate(self, tree):
        self._ensure_children_ifun(tree, start_idx=3)
        comb = combinations([e.ifun for e in tree.children[3::]], int(tree.children[1]))
        tree.ifun = '(' + '|'.join(['&'.join(c) for c in comb ]) + ')'

    def spare_gate(self, tree):
        self._ensure_children_ifun(tree, start_idx=1)
        tree.ifun = '(' + '&'.join([c.ifun for c in tree.children[1::]]) + ')'


class Tree2HeuristicIfun(Tree2StructuralIfun):

    def __init__(self, mode, num_phases=0, active_guard=False, physical_scores=None, tuning=None):
        super().__init__(active_guard=active_guard)
        self.mode = str(mode)
        self.num_phases = int(num_phases) if num_phases else 0
        self.physical_scores = physical_scores or {}
        self.tuning = tuning or HeuristicTuning()

    def _leaf_name(self, tree):
        return str(tree.children[0])

    def _leaf_base(self, tree):
        return "({}_broken>0)".format(self._leaf_name(tree))

    def _leaf_time_expr(self, name):
        if self.num_phases <= 1:
            return "1"
        # Use the single global PHASE_phase variable (range 1..num_phases)
        # instead of per-BE phase indicators.  This provides genuine time
        # sensitivity (higher phase => closer to deadline => higher importance)
        # while adding only ONE variable to the IFUN expression instead of N
        # per-BE variables, avoiding combinatorial explosion in FIG's adhoc
        # importance function builder.
        # NOTE: the old binary indicator ({name}_phase>0) was always 1
        # since phase is initialised to 1, giving zero time discrimination.
        return "PHASE_phase"

    def _leaf_time_max(self):
        return max(self.num_phases, 1)  # PHASE_phase range: 1..num_phases

    def _leaf_phys_weight(self, name):
        return int(self.physical_scores.get(name, 1))

    def _leaf_maxi(self, tree):
        _name = self._leaf_name(tree)
        _maxi = 1
        if self.mode in {"TIME", "TIMEPHYS"}:
            _maxi *= self._leaf_time_max()
        if self.mode in {"PHYS", "TIMEPHYS"}:
            _maxi *= self._leaf_phys_weight(_name)
        return max(int(_maxi), 1)

    def _leaf_expr(self, tree):
        _name = self._leaf_name(tree)
        _expr = self._leaf_base(tree)
        if self.mode in {"TIME", "TIMEPHYS"}:
            _expr = prod(_expr, self._leaf_time_expr(_name))
        if self.mode in {"PHYS", "TIMEPHYS"}:
            _expr = prod(_expr, self._leaf_phys_weight(_name))
        if self.active_guard and self.num_phases > 1:
            # For TIME/TIMEPHYS the IFUN already uses the global PHASE_phase
            # variable.  Disabled BEs have frozen clocks and can't transition
            # to broken, so _enabled is redundant; omitting it avoids adding
            # N per-BE boolean variables to the adhoc IFUN state space.
            if self.mode not in {"TIME", "TIMEPHYS"}:
                _expr = "(({}_enabled)*({}))".format(_name, _expr)
        return _expr

    def maxi(self, tree):
        try:
            tree.maxi
        except:
            if tree.data in {"be", "sbe"}:
                tree.maxi = self._leaf_maxi(tree)
            else:
                return super().maxi(tree)
        return tree.maxi

    def be(self, tree):
        tree.ifun = self._leaf_expr(tree)

    def sbe(self, tree):
        tree.ifun = self._leaf_expr(tree)

    def pand_gate(self, tree):
        for c in tree.children[1:3]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)
        _cs = tree.children
        _name = str(_cs[0])
        _lcmv = self.lcmv(*_cs[1::])
        _scalel = self.div(_lcmv, self.maxi(_cs[1]))
        _suml = self.prod(_scalel, _cs[1].ifun)
        _scaler = self.div(_lcmv, self.maxi(_cs[2]))
        _scif = self.prod(_scaler, _cs[2].ifun)
        _state = "{}_st".format(_name)
        _pos = "(({}>0)*({}<4))".format(_state, _state)
        _neg = "(({}==0)+({}==4))".format(_state, _state)
        _sumr = "(({})*({})-({})*({}))".format(_pos, _scif, _neg, _scif)
        _sum = "({}+{})".format(_suml, _sumr)
        _pand_bonus = max(1, int(round(2.0 * self.tuning.pand_eta * int(_lcmv))))
        _max = "max({},({}==2)*({}))".format(_sum, _state, _pand_bonus)
        tree.ifun = _max

    def spare_gate(self, tree):
        for c in tree.children[1:]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)
        _cs = tree.children
        _name = str(_cs[0])
        _parts = _cs[1::]
        _lcmv = self.lcmv(*_parts)
        _sum = [self.prod(self.div(_lcmv, self.maxi(e)), e.ifun) for e in _parts]
        _sum = "(" + "+".join(_sum) + ")"
        _max = "max({},{})".format(_sum, self.prod("({}_state==4)".format(_name), self.prod(len(_parts), _lcmv)))
        tree.ifun = _max


class IfunHeuristicContext:

    def __init__(self, compiler, tree, tuning=None):
        self.compiler = compiler
        self.tree = tree
        self.tuning = tuning or HeuristicTuning()

    def _leaf_proxy(self, node):
        _name = str(node.children[0])
        _proxy = 0.0
        for spec in node.children[1:]:
            if not isinstance(spec, Tree):
                continue
            if spec.data in {"fail", "dorm"}:
                _proxy = max(_proxy, _dist_tree_proxy(spec.children[0]))
        _ovr = self.compiler.phase_overrides.get(_name, {})
        for _dist in _ovr.get('fail', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        for _dist in _ovr.get('dorm', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        return _proxy

    def physical_scores(self):
        _proxies = {}
        for _kind in ["be", "sbe"]:
            for node in self.tree.find_data(_kind):
                _name = str(node.children[0])
                _proxies[_name] = self._leaf_proxy(node)
        if not _proxies:
            return {}
        _vals = list(_proxies.values())
        _minv = min(_vals)
        _maxv = max(_vals)
        if _maxv <= 0.0:
            return {k: 1 for k in _proxies}
        if _maxv == _minv:
            _mid = max(1, min(self.tuning.phys_k, int(round((self.tuning.phys_k + 1) / 2.0))))
            return {k: _mid for k in _proxies}
        _scores = {}
        for _name, _proxy in _proxies.items():
            _scaled = _proxy ** self.tuning.phys_beta if _proxy > 0.0 else 0.0
            _scaled_min = _minv ** self.tuning.phys_beta if _minv > 0.0 else 0.0
            _scaled_max = _maxv ** self.tuning.phys_beta if _maxv > 0.0 else 0.0
            if _scaled_max == _scaled_min:
                _norm = 0.0
            else:
                _norm = (_scaled - _scaled_min) / (_scaled_max - _scaled_min)
            _layer = _norm ** self.tuning.phys_gamma
            _scores[_name] = max(1, min(self.tuning.phys_k, int(round(1 + (self.tuning.phys_k - 1) * _layer))))
        return _scores


class IfunN4Context:
    """RD-GT style log-difficulty IFUN.

    Leaf difficulty is -log(q_i), converted to an integer scale. Gates compose
    remaining difficulty as AND=sum, OR=min, VOT=sum of k easiest children and
    PAND=ordered remaining difficulty.
    """

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, epsilon=1e-300,
                 inf_mult=10.0, tuning=None):
        self.compiler = compiler
        self.raw_tree = raw_tree
        self.state_tree = state_tree
        self.T = max(float(mission_time or 0.0), 1.0)
        self.scale = max(int(scale), 10)
        self.d_scale = max(float(d_scale), 1.0)
        self.epsilon = float(epsilon)
        self.inf_mult = max(float(inf_mult), 1.0)
        self.tuning = tuning or HeuristicTuning()
        self.leaf_rates = self._collect_leaf_rates()
        self.leaf_q = {name: self._q(rate) for name, rate in self.leaf_rates.items()}
        self.leaf_d = self._leaf_difficulties()
        self.d0_cache = {}
        self.expr_cache = {}

    def _collect_leaf_rates(self):
        ctx = IfunHeuristicContext(self.compiler, self.raw_tree, tuning=self.tuning)
        rates = {}
        for kind in ["be", "sbe"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                rates[name] = max(rates.get(name, 0.0), ctx._leaf_proxy(node))
        return rates

    def _q(self, rate):
        if rate <= 0.0:
            return self.epsilon
        q = 1.0 - math.exp(-float(rate) * self.T)
        return max(min(q, 1.0 - self.epsilon), self.epsilon)

    def _leaf_difficulties(self):
        out = {}
        for name, q in self.leaf_q.items():
            d = -math.log(max(min(q, 1.0), self.epsilon))
            out[name] = max(1, int(round(d * self.d_scale)))
        return out

    def _leaf_name(self, node):
        return str(node.children[0])

    def _children(self, node):
        if node.data == "vot_gate":
            return [c for c in node.children[3:] if isinstance(c, Tree)]
        return [c for c in node.children[1:] if isinstance(c, Tree)]

    def _min_expr(self, terms, default="0"):
        terms = [str(t) for t in terms if str(t) != ""]
        if not terms:
            return str(default)
        expr = terms[0]
        for term in terms[1:]:
            expr = "min({},({}))".format(expr, term)
        return expr

    def _sum_expr(self, terms):
        terms = [str(t) for t in terms if str(t) not in {"", "0", "0.0"}]
        if not terms:
            return "0"
        if len(terms) == 1:
            return terms[0]
        return "(" + "+".join(terms) + ")"

    def _node_failed_expr(self, node):
        if node.data == "toplevel":
            return self._node_failed_expr(node.children[0])
        if node.data in {"be", "sbe"}:
            return "({}_broken>0)".format(self._leaf_name(node))
        if node.data == "or_gate":
            return "({}_count>0)".format(self._leaf_name(node))
        if node.data == "and_gate":
            return "({}_count=={})".format(self._leaf_name(node), len(self._children(node)))
        if node.data == "vot_gate":
            return "({}_count>={})".format(self._leaf_name(node), int(node.children[1]))
        if node.data == "pand_gate":
            name = self._leaf_name(node)
            return "max(({}_st==1),({}_st==2))".format(name, name)
        if node.data == "spare_gate":
            return "({}_state==4)".format(self._leaf_name(node))
        return "0"

    def _d0(self, node):
        nid = id(node)
        if nid in self.d0_cache:
            return self.d0_cache[nid]
        if node.data == "toplevel":
            d = self._d0(node.children[0])
        elif node.data in {"be", "sbe"}:
            d = self.leaf_d.get(self._leaf_name(node), 1)
        elif node.data == "or_gate":
            ds = [self._d0(c) for c in self._children(node)]
            d = min(ds) if ds else 1
        elif node.data in {"and_gate", "spare_gate"}:
            d = sum(self._d0(c) for c in self._children(node))
        elif node.data == "pand_gate":
            d = sum(self._d0(c) for c in self._children(node))
        elif node.data == "vot_gate":
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            ds = sorted(self._d0(c) for c in self._children(node))
            d = sum(ds[:max(1, min(k, len(ds)))]) if ds else 1
        else:
            d = 1
        d = max(1, int(d))
        self.d0_cache[nid] = d
        return d

    def _drem_expr(self, node, force_failed=None):
        force_failed = force_failed or frozenset()
        key = (id(node), tuple(sorted(force_failed)))
        if key in self.expr_cache:
            return self.expr_cache[key]

        if node.data == "toplevel":
            expr = self._drem_expr(node.children[0], force_failed)
        elif node.data in {"be", "sbe"}:
            name = self._leaf_name(node)
            d = self.leaf_d.get(name, 1)
            if name in force_failed:
                expr = "0"
            else:
                expr = "((1-({}_broken>0))*{})".format(name, d)
        elif node.data in {"and_gate", "spare_gate"}:
            raw = self._sum_expr([self._drem_expr(c, force_failed) for c in self._children(node)])
            failed = self._node_failed_expr(node)
            expr = "((1-({}))*({}))".format(failed, raw)
        elif node.data == "or_gate":
            children = self._children(node)
            raw = self._min_expr([self._drem_expr(c, force_failed) for c in children], default=str(self._d0(node)))
            failed = self._node_failed_expr(node)
            expr = "((1-({}))*({}))".format(failed, raw)
        elif node.data == "vot_gate":
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            child_exprs = [self._drem_expr(c, force_failed) for c in self._children(node)]
            if not child_exprs:
                raw = str(self._d0(node))
            elif k <= 1:
                raw = self._min_expr(child_exprs, default=str(self._d0(node)))
            else:
                combos = []
                for combo in combinations(child_exprs, max(1, min(k, len(child_exprs)))):
                    combos.append(self._sum_expr(combo))
                raw = self._min_expr(combos, default=str(self._d0(node)))
            failed = self._node_failed_expr(node)
            expr = "((1-({}))*({}))".format(failed, raw)
        elif node.data == "pand_gate":
            children = self._children(node)
            if len(children) < 2:
                expr = str(self._d0(node))
            else:
                left, right = children[0], children[1]
                name = self._leaf_name(node)
                failed = self._node_failed_expr(node)
                blocked = "({}_st==4)".format(name)
                left_done = self._node_failed_expr(left)
                not_started = self._sum_expr([self._drem_expr(left, force_failed), str(self._d0(right))])
                armed = self._drem_expr(right, force_failed)
                raw = "(({})*({})+(1-({}))*({}))".format(left_done, armed, left_done, not_started)
                expr = "(({})*0+(1-({}))*(({})*{}+(1-({}))*({})))".format(
                    failed, failed, blocked, self._d0(node), blocked, raw)
        else:
            expr = str(self._d0(node))

        self.expr_cache[key] = expr
        return expr

    def progress_expr(self, force_failed=None):
        d0 = max(1, self._d0(self.state_tree))
        drem = self._drem_expr(self.state_tree, force_failed=frozenset(force_failed or []))
        S = self.scale
        expr = "min({},max(0,floor((({}-({}))*{})/{})))".format(S, d0, drem, S, d0)
        return expr, S


class _N4qRobdd:
    """Small deterministic ROBDD manager used by N4Q.

    Nodes 0 and 1 are the false/true terminals.  All other nodes are
    canonical (variable, low, high) triples.  The manager deliberately has no
    dependency on pyeda so IFUN generation is stable across pyeda versions.
    """

    def __init__(self, variables):
        self.order = {name: i for i, name in enumerate(variables)}
        self.nodes = {}
        self.unique = {}
        self.apply_cache = {}
        self.not_cache = {0: 1, 1: 0}
        self.next_id = 2

    def mk(self, variable, low, high):
        if low == high:
            return low
        key = (variable, low, high)
        if key not in self.unique:
            node_id = self.next_id
            self.next_id += 1
            self.unique[key] = node_id
            self.nodes[node_id] = key
        return self.unique[key]

    def var(self, variable):
        return self.mk(variable, 0, 1)

    def negate(self, node):
        if node in self.not_cache:
            return self.not_cache[node]
        variable, low, high = self.nodes[node]
        result = self.mk(variable, self.negate(low), self.negate(high))
        self.not_cache[node] = result
        return result

    def apply(self, operation, left, right):
        if operation not in {"and", "or"}:
            raise ValueError("unsupported ROBDD operation: " + operation)
        if right < left:
            left, right = right, left
        key = (operation, left, right)
        if key in self.apply_cache:
            return self.apply_cache[key]
        if operation == "and":
            if left == 0 or right == 0:
                return 0
            if left == 1:
                return right
            if right == 1 or left == right:
                return left
        else:
            if left == 1 or right == 1:
                return 1
            if left == 0:
                return right
            if right == 0 or left == right:
                return left
        lvar = self.nodes[left][0]
        rvar = self.nodes[right][0]
        top = lvar if self.order[lvar] <= self.order[rvar] else rvar
        if lvar == top:
            _, llow, lhigh = self.nodes[left]
        else:
            llow = lhigh = left
        if rvar == top:
            _, rlow, rhigh = self.nodes[right]
        else:
            rlow = rhigh = right
        low = self.apply(operation, llow, rlow)
        high = self.apply(operation, lhigh, rhigh)
        result = self.mk(top, low, high)
        self.apply_cache[key] = result
        return result

    def threshold(self, children, k):
        n = len(children)
        if k <= 0:
            return 1
        if k > n:
            return 0
        # dp[j] means at least j successes among children processed so far.
        dp = [1] + [0] * k
        for child in children:
            nxt = [1] + [0] * k
            for j in range(1, k + 1):
                keep = self.apply("and", dp[j], self.negate(child))
                gain = self.apply("and", dp[j - 1], child)
                nxt[j] = self.apply("or", keep, gain)
            dp = nxt
        return dp[k]


class IfunN4pContext(IfunN4Context):
    """N4P ablation: frozen N4 with explicitly unreachable blocked PAND."""

    VERSION = "N4P-v1"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        occurrences = self._leaf_occurrences(self.state_tree)
        self.unique_leaf_count = len(set(occurrences))
        self.repeated_leaf_count = max(0, len(occurrences) - self.unique_leaf_count)
        self.pand_count = sum(1 for _ in self.state_tree.find_data("pand_gate"))
        self.unreachable_bound = 1 + sum(self.leaf_d.get(name, 1) for name in occurrences)
        self.expression_length = 0

    def _leaf_occurrences(self, node):
        if node.data == "toplevel":
            return self._leaf_occurrences(node.children[0])
        if node.data in {"be", "sbe"}:
            return [self._leaf_name(node)]
        result = []
        for child in self._children(node):
            result.extend(self._leaf_occurrences(child))
        return result

    def _drem_expr(self, node, force_failed=None):
        force_failed = force_failed or frozenset()
        key = (id(node), tuple(sorted(force_failed)))
        if key in self.expr_cache:
            return self.expr_cache[key]
        if node.data != "pand_gate":
            return IfunN4Context._drem_expr(self, node, force_failed)
        children = self._children(node)
        if len(children) != 2:
            raise ValueError("N4P-v1 requires binary PAND gates")
        left, right = children
        name = self._leaf_name(node)
        completed = self._node_failed_expr(node)
        blocked = "({}_st==4)".format(name)
        left_done = self._node_failed_expr(left)
        not_started = self._sum_expr([
            self._drem_expr(left, force_failed), str(self._d0(right))])
        armed = self._drem_expr(right, force_failed)
        raw = "(({})*({})+(1-({}))*({}))".format(
            left_done, armed, left_done, not_started)
        expr = "(({})*0+(1-({}))*(({})*{}+(1-({}))*({})))".format(
            completed, completed, blocked, self.unreachable_bound, blocked, raw)
        self.expr_cache[key] = expr
        return expr

    def progress_expr(self, force_failed=None):
        expr, maximum = super().progress_expr(force_failed=force_failed)
        self.expression_length = len(expr)
        return expr, maximum

    def diagnostic_row(self):
        d0 = float(max(1, self._d0(self.state_tree))) / self.d_scale
        return {
            "n4q_version": self.VERSION,
            "bdd_node_count": 0,
            "unique_leaf_count": self.unique_leaf_count,
            "repeated_leaf_count": self.repeated_leaf_count,
            "pand_count": self.pand_count,
            "pand_shared_overlap_count": 0,
            "expression_length": self.expression_length,
            "initial_h": math.exp(-d0),
            "initial_d": d0,
            "n4q_mode": "N4P",
            "approximation_flags": "n4_domain_ablation,pand_blocked_unreachable",
        }

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        return "N4Q " + ",".join(
            "{}={}".format(key, row[key]) for key in sorted(row))


class IfunN4qContext(IfunN4Context):
    """N4Q-v1 conditional top-event reachability approximation.

    Static Boolean structure is represented by a unique-event ROBDD.  Dynamic
    PAND gates are state-conditioned pseudo variables; composite PAND children
    use the preregistered effective-hazard approximation.
    """

    VERSION = "N4Q-v1"
    PAND_PREFIX = "__PAND__:"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        occurrences = self._leaf_occurrences(self.state_tree)
        self.unique_leaves = sorted(set(occurrences))
        self.unique_leaf_count = len(self.unique_leaves)
        self.repeated_leaf_count = max(0, len(occurrences) - self.unique_leaf_count)
        self.pand_nodes = list(self.state_tree.find_data("pand_gate"))
        self.pand_by_name = {self._leaf_name(node): node for node in self.pand_nodes}
        if len(self.pand_by_name) != len(self.pand_nodes):
            raise ValueError("N4Q-v1 requires unique PAND gate names")
        pand_variables = [self._pand_variable(name) for name in sorted(self.pand_by_name)]
        self.bdd = _N4qRobdd(self.unique_leaves + pand_variables)
        self.compile_cache = {}
        self.approximation_flags = set()
        self.root = self._compile_bdd(self.state_tree)
        self.pand_initial = {}
        self.pand_meta = {}
        self.pand_shared_overlap_count = self._count_pand_external_overlaps()
        if self.pand_shared_overlap_count:
            self.approximation_flags.add("pand_external_shared")
        for node in sorted(self.pand_nodes, key=self._tree_depth, reverse=True):
            self._prepare_pand(node)
        self.initial_h = self._bdd_probability(self.root, self._initial_probabilities())
        self.initial_h = max(0.0, min(1.0, self.initial_h))
        self.initial_d = -math.log(max(self.initial_h, self.epsilon))
        self.prob_expr_cache = {}
        self.bdd_expr_cache = {}
        self.expression_length = 0

    def _leaf_occurrences(self, node):
        if node.data == "toplevel":
            return self._leaf_occurrences(node.children[0])
        if node.data in {"be", "sbe"}:
            return [self._leaf_name(node)]
        result = []
        for child in self._children(node):
            result.extend(self._leaf_occurrences(child))
        return result

    def _fmt(self, value):
        return "{:.16g}".format(float(value))

    def _pand_variable(self, name):
        return self.PAND_PREFIX + str(name)

    def _tree_depth(self, node):
        children = self._children(node) if node.data not in {"be", "sbe"} else []
        return 1 + max([self._tree_depth(child) for child in children] or [0])

    def _support(self, node, skip=None):
        if node is skip:
            return set()
        if node.data == "toplevel":
            return self._support(node.children[0], skip=skip)
        if node.data in {"be", "sbe"}:
            return {self._leaf_name(node)}
        support = set()
        for child in self._children(node):
            support |= self._support(child, skip=skip)
        return support

    def _count_pand_external_overlaps(self):
        count = 0
        for node in self.pand_nodes:
            inside = self._support(node)
            outside = self._support(self.state_tree, skip=node)
            if inside & outside:
                count += 1
        return count

    def _compile_bdd(self, node):
        node_id = id(node)
        if node_id in self.compile_cache:
            return self.compile_cache[node_id]
        if node.data == "toplevel":
            result = self._compile_bdd(node.children[0])
        elif node.data in {"be", "sbe"}:
            result = self.bdd.var(self._leaf_name(node))
        elif node.data == "pand_gate":
            result = self.bdd.var(self._pand_variable(self._leaf_name(node)))
        elif node.data in {"and_gate", "spare_gate"}:
            if node.data == "spare_gate":
                self.approximation_flags.add("spare_as_and") if hasattr(self, "approximation_flags") else None
            result = 1
            for child in self._children(node):
                result = self.bdd.apply("and", result, self._compile_bdd(child))
        elif node.data == "or_gate":
            result = 0
            for child in self._children(node):
                result = self.bdd.apply("or", result, self._compile_bdd(child))
        elif node.data == "vot_gate":
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            result = self.bdd.threshold(
                [self._compile_bdd(child) for child in self._children(node)], k)
        else:
            raise ValueError("N4Q-v1 unsupported gate: " + str(node.data))
        self.compile_cache[node_id] = result
        return result

    def _initial_probabilities(self):
        probabilities = dict(self.leaf_q)
        for name, value in self.pand_initial.items():
            probabilities[self._pand_variable(name)] = value
        return probabilities

    def _bdd_probability(self, root, probabilities):
        cache = {0: 0.0, 1: 1.0}

        def visit(node_id):
            if node_id in cache:
                return cache[node_id]
            variable, low, high = self.bdd.nodes[node_id]
            p = max(0.0, min(1.0, float(probabilities[variable])))
            value = (1.0 - p) * visit(low) + p * visit(high)
            cache[node_id] = value
            return value

        return visit(root)

    def _ordered_probability(self, lambda_a, lambda_b):
        a = max(0.0, float(lambda_a))
        b = max(0.0, float(lambda_b))
        if a <= 0.0 or b <= 0.0:
            return 0.0
        if math.isclose(a, b, rel_tol=1e-10, abs_tol=1e-15):
            value = 1.0 - math.exp(-a * self.T) - a * self.T * math.exp(-a * self.T)
        else:
            value = (
                1.0 - math.exp(-a * self.T)
                - a / (a - b) * (math.exp(-b * self.T) - math.exp(-a * self.T))
            )
        return max(0.0, min(1.0, value))

    def _subtree_initial_probability(self, node):
        root = self._compile_bdd(node)
        return self._bdd_probability(root, self._initial_probabilities())

    def _prepare_pand(self, node):
        name = self._leaf_name(node)
        children = self._children(node)
        if len(children) != 2:
            raise ValueError("N4Q-v1 requires binary PAND gates")
        left, right = children
        left_support = self._support(left)
        right_support = self._support(right)
        if left_support & right_support:
            self.approximation_flags.add("pand_internal_shared")
        primitive = left.data in {"be", "sbe"} and right.data in {"be", "sbe"}
        if primitive:
            lambda_a = self.leaf_rates.get(self._leaf_name(left), 0.0)
            lambda_b = self.leaf_rates.get(self._leaf_name(right), 0.0)
        else:
            q_left = self._subtree_initial_probability(left)
            q_right = self._subtree_initial_probability(right)
            cap = 1.0 - 1e-15
            lambda_a = -math.log(max(1e-300, 1.0 - min(q_left, cap))) / self.T
            lambda_b = -math.log(max(1e-300, 1.0 - min(q_right, cap))) / self.T
            self.approximation_flags.add("pand_effective_hazard")
        ordered = self._ordered_probability(lambda_a, lambda_b)
        self.pand_initial[name] = ordered
        self.pand_meta[name] = {
            "node": node,
            "left": left,
            "right": right,
            "lambda_a": lambda_a,
            "lambda_b": lambda_b,
            "not_started": ordered,
        }

    def _pand_probability(self, name, failed_leaves, pand_states):
        state = str(pand_states.get(name, "not_started")).lower()
        if state == "completed":
            return 1.0
        if state == "blocked":
            return 0.0
        if state == "armed":
            return self._subtree_probability(
                self.pand_meta[name]["right"], failed_leaves, pand_states,
                skip_pand=name)
        if state != "not_started":
            raise ValueError("unknown PAND state for {}: {}".format(name, state))
        return self.pand_meta[name]["not_started"]

    def _runtime_probabilities(self, failed_leaves, pand_states, skip_pand=None):
        probabilities = {
            name: (1.0 if name in failed_leaves else self.leaf_q[name])
            for name in self.unique_leaves
        }
        for name in self.pand_by_name:
            if name == skip_pand:
                probabilities[self._pand_variable(name)] = self.pand_initial[name]
            else:
                probabilities[self._pand_variable(name)] = self._pand_probability(
                    name, failed_leaves, pand_states)
        return probabilities

    def _subtree_probability(self, node, failed_leaves, pand_states,
                             skip_pand=None):
        return self._bdd_probability(
            self._compile_bdd(node),
            self._runtime_probabilities(
                failed_leaves, pand_states, skip_pand=skip_pand),
        )

    def probability(self, failed_leaves=None, pand_states=None):
        failed_leaves = set(failed_leaves or [])
        unknown = failed_leaves - set(self.unique_leaves)
        if unknown:
            raise ValueError("unknown failed leaves: " + ",".join(sorted(unknown)))
        return self._bdd_probability(
            self.root,
            self._runtime_probabilities(failed_leaves, dict(pand_states or {})),
        )

    def _variable_probability_expr(self, variable):
        if variable in self.prob_expr_cache:
            return self.prob_expr_cache[variable]
        if not variable.startswith(self.PAND_PREFIX):
            q = self._fmt(self.leaf_q[variable])
            result = "(({}_broken>0)+(({}_broken==0)*{}))".format(
                variable, variable, q)
        else:
            name = variable[len(self.PAND_PREFIX):]
            meta = self.pand_meta[name]
            node = meta["node"]
            completed = self._node_failed_expr(node)
            blocked = "({}_st==4)".format(name)
            left_done = self._node_failed_expr(meta["left"])
            armed = self._bdd_probability_expr(self._compile_bdd(meta["right"]))
            fresh = self._fmt(meta["not_started"])
            raw = "(({})*({})+(1-({}))*({}))".format(
                left_done, armed, left_done, fresh)
            result = "(({})+((1-({}))*(1-({}))*({})))".format(
                completed, completed, blocked, raw)
        self.prob_expr_cache[variable] = result
        return result

    def _bdd_probability_expr(self, node_id):
        if node_id == 0:
            return "0"
        if node_id == 1:
            return "1"
        if node_id in self.bdd_expr_cache:
            return self.bdd_expr_cache[node_id]
        variable, low, high = self.bdd.nodes[node_id]
        p = self._variable_probability_expr(variable)
        if low == 0 and high == 1:
            result = p
        elif low == 0:
            result = "(({})*({}))".format(p, self._bdd_probability_expr(high))
        elif high == 1:
            low_expr = self._bdd_probability_expr(low)
            result = "(({})+(({})*(1-({}))))".format(low_expr, p, low_expr)
        else:
            result = "((1-({}))*({})+({})*({}))".format(
                p,
                self._bdd_probability_expr(low),
                p,
                self._bdd_probability_expr(high),
            )
        self.bdd_expr_cache[node_id] = result
        return result

    def progress_expr(self, force_failed=None):
        if force_failed:
            raise ValueError("N4Q-v1 does not support compile-time force_failed")
        h_expr = self._bdd_probability_expr(self.root)
        d_expr = "(-log(max({},({}))))".format(self._fmt(self.epsilon), h_expr)
        d0 = max(self.initial_d, 1e-12)
        S = self.scale
        expr = "min({},max(0,floor((({}-({}))*{})/{})))".format(
            S, self._fmt(d0), d_expr, S, self._fmt(d0))
        self.expression_length = len(expr)
        return expr, S

    def diagnostic_row(self):
        return {
            "n4q_version": self.VERSION,
            "bdd_node_count": len(self.bdd.nodes) + 2,
            "unique_leaf_count": self.unique_leaf_count,
            "repeated_leaf_count": self.repeated_leaf_count,
            "pand_count": len(self.pand_nodes),
            "pand_shared_overlap_count": self.pand_shared_overlap_count,
            "expression_length": self.expression_length,
            "initial_h": self.initial_h,
            "initial_d": self.initial_d,
            "n4q_mode": "N4Q",
            "approximation_flags": ",".join(sorted(self.approximation_flags)) or "none",
        }

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        return "N4Q " + ",".join(
            "{}={}".format(key, row[key]) for key in sorted(row))


class IfunN4aContext(IfunN4Context):
    """Pure N4A: adaptive physical temperature + mission-time fix + PAND INF.

    This is the original N4A form used in the 2026-05-26/27 experiments.
    Extra constructor arguments are accepted for backward compatibility with
    later experimental subclasses, but pure N4A ignores preset tuning knobs.
    """

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, epsilon=1e-300, tuning=None,
                 alpha_min=0.1, rare_low=3.0, rare_high=8.0,
                 hetero_low=3.0, hetero_high=30.0,
                 bottleneck_low=0.40, bottleneck_high=0.70,
                 inf_mult=10.0, fixed_alpha_flag=0.0, fixed_alpha_value=0.0):
        super().__init__(
            compiler, raw_tree, state_tree, mission_time=mission_time,
            scale=scale, d_scale=d_scale, epsilon=epsilon,
            inf_mult=inf_mult, tuning=tuning)
        self.alpha_min = float(alpha_min)
        self.rare_low = float(rare_low)
        self.rare_high = float(rare_high)
        self.hetero_low = float(hetero_low)
        self.hetero_high = float(hetero_high)
        self.bottleneck_low = float(bottleneck_low)
        self.bottleneck_high = float(bottleneck_high)
        self.fixed_alpha_flag = float(fixed_alpha_flag)
        self.fixed_alpha_value = float(fixed_alpha_value)
        self.alpha = self._compute_alpha()
        self.leaf_d = self._leaf_difficulties_adaptive()
        self.d0_cache = {}
        self.expr_cache = {}

    def _ramp(self, value, low, high):
        if high <= low:
            return 1.0 if value >= high else 0.0
        return max(0.0, min(1.0, (float(value) - float(low)) / (float(high) - float(low))))

    def _bottleneck_ratio(self):
        max_b = 0.0
        for node in self.raw_tree.iter_subtrees():
            if node.data != "and_gate":
                continue
            children = self._children(node)
            if len(children) < 2:
                continue
            ds = [float(self._d0(c)) for c in children]
            total = sum(ds)
            if total > 0.0:
                max_b = max(max_b, max(ds) / total)
        return max_b

    def _compute_alpha(self):
        """alpha = max(alpha_min, g_rare * max(g_hetero, g_bottleneck))."""
        d0_top = max(1, self._d0(self.state_tree))
        self._diag_d0_top = d0_top
        g_rare = max(0.0, min(1.0, (float(d0_top) - 3.0) / (8.0 - 3.0)))
        self._diag_g_rare = g_rare
        qs = list(self.leaf_q.values())
        if len(qs) >= 2:
            qmax = max(qs)
            qmin = min(qs)
            if qmin > self.epsilon and qmax > qmin:
                log_r = math.log(max(qmax / qmin, 1.0))
                g_hetero = max(0.0, min(
                    1.0,
                    (log_r - math.log(3.0)) / (math.log(30.0) - math.log(3.0)),
                ))
            else:
                g_hetero = 0.0
        else:
            g_hetero = 0.0
        self._diag_g_hetero = g_hetero
        g_bottleneck = max(0.0, min(1.0, (self._bottleneck_ratio() - 0.4) / (0.7 - 0.4)))
        self._diag_g_bottleneck = g_bottleneck
        return max(self.alpha_min, g_rare * max(g_hetero, g_bottleneck))

    def _leaf_difficulties_adaptive(self):
        raw = {}
        for name, q in self.leaf_q.items():
            d = -math.log(max(min(q, 1.0), self.epsilon))
            raw[name] = max(1, int(round(d * self.d_scale)))
        if not raw:
            self._diag_raw_d_min = 0
            self._diag_raw_d_max = 0
            self._diag_D_ref = 1.0
            self._diag_adapted_d_min = 0
            self._diag_adapted_d_max = 0
            return {}
        raw_vals = list(raw.values())
        self._diag_raw_d_min = min(raw_vals)
        self._diag_raw_d_max = max(raw_vals)
        log_sum = sum(math.log(max(float(v), 1.0)) for v in raw_vals)
        d_ref = max(1.0, math.exp(log_sum / float(len(raw))))
        self._diag_D_ref = d_ref
        out = {}
        alpha = self.alpha
        for name, d in raw.items():
            if alpha >= 0.999:
                out[name] = d
            elif alpha <= 0.001:
                out[name] = max(1, int(round(d_ref)))
            else:
                ratio = max(float(d), 1.0) / d_ref
                out[name] = max(1, int(round(d_ref * (ratio ** alpha))))
        adapted_vals = list(out.values())
        self._diag_adapted_d_min = min(adapted_vals)
        self._diag_adapted_d_max = max(adapted_vals)
        return out

    def diagnostic_row(self):
        return {
            "n4a_version": "N4AH",
            "alpha": round(float(self.alpha), 6),
            "g_rare": round(float(getattr(self, "_diag_g_rare", 0)), 6),
            "g_hetero": round(float(getattr(self, "_diag_g_hetero", 0)), 6),
            "g_bottleneck": round(float(getattr(self, "_diag_g_bottleneck", 0)), 6),
            "d_ref": round(float(getattr(self, "_diag_D_ref", 1.0)), 2),
            "d0_top": int(getattr(self, "_diag_d0_top", 0)),
            "raw_d_min": int(getattr(self, "_diag_raw_d_min", 0)),
            "raw_d_max": int(getattr(self, "_diag_raw_d_max", 0)),
            "adapted_d_min": int(getattr(self, "_diag_adapted_d_min", 0)),
            "adapted_d_max": int(getattr(self, "_diag_adapted_d_max", 0)),
        }

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        fields = [
            "n4a_version", "alpha", "g_rare", "g_hetero", "g_bottleneck",
            "d_ref", "d0_top", "raw_d_min", "raw_d_max",
            "adapted_d_min", "adapted_d_max",
        ]
        return "N4AH " + ",".join("{}={}".format(k, row.get(k, "")) for k in fields)

    def _drem_expr(self, node, force_failed=None):
        force_failed = force_failed or frozenset()
        key = (id(node), tuple(sorted(force_failed)))
        if key in self.expr_cache:
            return self.expr_cache[key]

        if node.data == "pand_gate":
            children = self._children(node)
            if len(children) < 2:
                expr = str(self._d0(node))
            else:
                left, right = children[0], children[1]
                name = self._leaf_name(node)
                failed = self._node_failed_expr(node)
                blocked = "({}_st==4)".format(name)
                left_done = self._node_failed_expr(left)
                not_started = self._sum_expr([
                    self._drem_expr(left, force_failed),
                    str(self._d0(right)),
                ])
                armed = self._drem_expr(right, force_failed)
                raw = "(({})*({})+(1-({}))*({}))".format(
                    left_done, armed, left_done, not_started)
                inf = max(1, int(round(self.inf_mult * max(1, self._d0(self.state_tree)))))
                expr = "(({})*0+(1-({}))*(({})*{}+(1-({}))*({})))".format(
                    failed, failed, blocked, inf, blocked, raw)
        else:
            expr = IfunN4Context._drem_expr(self, node, force_failed)

        self.expr_cache[key] = expr
        return expr


class IfunArdiV2Context(IfunN4aContext):
    '''ARDI-v2: frozen N4A mechanics with an independent alpha law.'''

    VERSION = 'ARDI-v2.0'
    DEFAULT_ALPHA_LAW = 'inverse_low_temp'

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, epsilon=1e-300, tuning=None,
                 alpha_min=0.10, alpha_max=0.25,
                 alpha_law=DEFAULT_ALPHA_LAW,
                 fixed_alpha_flag=0.0, fixed_alpha_value=0.10):
        self.ardi_alpha_min = max(0.0, min(1.0, float(alpha_min)))
        self.ardi_alpha_max = max(
            self.ardi_alpha_min, min(1.0, float(alpha_max)))
        self.ardi_alpha_law = str(alpha_law or self.DEFAULT_ALPHA_LAW).strip().lower()
        self.ardi_fixed_alpha_flag = bool(float(fixed_alpha_flag))
        self.ardi_fixed_alpha_value = float(fixed_alpha_value)
        super().__init__(
            compiler, raw_tree, state_tree,
            mission_time=mission_time, scale=scale, d_scale=d_scale,
            epsilon=epsilon, tuning=tuning,
            alpha_min=self.ardi_alpha_min, inf_mult=10.0)

    @staticmethod
    def compute_alpha_from_h(h, alpha_min=0.10, alpha_max=0.25,
                             fixed_alpha=None):
        low = max(0.0, min(1.0, float(alpha_min)))
        high = max(low, min(1.0, float(alpha_max)))
        if fixed_alpha is not None:
            return max(low, min(high, float(fixed_alpha)))
        need = max(0.0, min(1.0, float(h)))
        return low + (high - low) * (1.0 - need)

    def _compute_alpha(self):
        historical_alpha = IfunN4aContext._compute_alpha(self)
        need = max(0.0, min(
            1.0,
            float(self._diag_g_rare)
            * max(float(self._diag_g_hetero), float(self._diag_g_bottleneck)),
        ))
        self._diag_ardi_need = need
        fixed = None
        if self.ardi_fixed_alpha_flag or self.ardi_alpha_law == 'fixed':
            fixed = self.ardi_fixed_alpha_value
        if self.ardi_alpha_law == 'historical' and fixed is None:
            return historical_alpha
        if self.ardi_alpha_law not in {
                'inverse_low_temp', 'historical', 'fixed'}:
            raise ValueError('Unsupported ARDI_ALPHA_LAW: ' + self.ardi_alpha_law)
        return self.compute_alpha_from_h(
            need,
            alpha_min=self.ardi_alpha_min,
            alpha_max=self.ardi_alpha_max,
            fixed_alpha=fixed,
        )

    def diagnostic_row(self):
        row = super().diagnostic_row()
        row.update({
            'ardi_version': self.VERSION,
            'ardi_alpha_law': self.ardi_alpha_law,
            'ardi_alpha_min': round(self.ardi_alpha_min, 6),
            'ardi_alpha_max': round(self.ardi_alpha_max, 6),
            'ardi_diag_need': round(float(self._diag_ardi_need), 6),
        })
        return row

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        fields = [
            'ardi_version', 'ardi_alpha_law', 'ardi_alpha_min',
            'ardi_alpha_max', 'ardi_diag_need', 'alpha', 'g_rare',
            'g_hetero', 'g_bottleneck', 'd_ref', 'd0_top',
            'raw_d_min', 'raw_d_max', 'adapted_d_min', 'adapted_d_max',
        ]
        return 'ARDIV2 ' + ','.join(
            '{}={}'.format(key, row.get(key, '')) for key in fields)


class IfunN4anContext(IfunN4Context):
    """N4AN: normalized/parameterized adaptive physical temperature + mission-time fix + PAND INF.

    D_i* = D_ref * (D_i / D_ref)^alpha

    Uses parameterized rarity (d0_top/d_scale), heterogeneity (log-space ramp),
    and bottleneck ratios. Supports fixed-alpha override.

    Environment knobs:
      N4A_ALPHA_MIN        0.1       floor for alpha
      N4A_RARE_LOW/HIGH    3 / 8     rarity ramp bounds (applied to d0_top/d_scale)
      N4A_HETERO_LOW/HIGH  3 / 30    q-ratio ramp bounds (log space)
      N4A_BOTTLENECK_LOW/HIGH 0.4 / 0.7 bottleneck ramp bounds
      N4A_INF_MULT         10        PAND blocked penalty multiplier
      N4A_FIXED_ALPHA_FLAG 0         force alpha to N4A_FIXED_ALPHA_VALUE
      N4A_FIXED_ALPHA_VALUE 0        fixed alpha value when flag is enabled
      N4_MISSION_TIME      <T>       explicit mission time (default: transient_horizon)
    """

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, alpha_min=0.1, epsilon=1e-300,
                 tuning=None, rdgt_params=None):
        super().__init__(
            compiler, raw_tree, state_tree,
            mission_time=mission_time, scale=scale,
            d_scale=d_scale, epsilon=epsilon, tuning=tuning,
        )
        self.rdgt_params = rdgt_params or RDGTParams(alpha_min=alpha_min)
        self.alpha_min = float(self.rdgt_params.alpha_min)
        self.alpha = self._compute_alpha()
        self.leaf_d = self._leaf_difficulties_adaptive()
        self.d0_cache = {}
        self.expr_cache = {}

    def _ramp(self, value, low, high):
        low = float(low)
        high = float(high)
        if high <= low:
            return 1.0 if float(value) >= high else 0.0
        return max(0.0, min(1.0, (float(value) - low) / (high - low)))

    def _compute_alpha(self):
        """alpha = max(alpha_min, g_rare * max(g_hetero, g_bottleneck))."""
        p = self.rdgt_params
        if p.fixed_alpha_flag:
            alpha = max(0.0, min(1.0, float(p.fixed_alpha_value)))
            self._diag_d0_top = max(1, self._d0(self.state_tree))
            self._diag_g_rare = 0.0
            self._diag_g_hetero = 0.0
            self._diag_g_bottleneck = 0.0
            return alpha

        d0_top = max(1, self._d0(self.state_tree))
        self._diag_d0_top = d0_top
        g_rare = self._ramp(float(d0_top) / max(float(self.d_scale), 1.0),
                            p.rare_low, p.rare_high)
        self._diag_g_rare = g_rare

        qs = list(self.leaf_q.values())
        if len(qs) >= 2:
            qmax = max(qs)
            qmin = min(qs)
            if qmin > self.epsilon and qmax > qmin:
                r_q = qmax / qmin
                log_r = math.log(max(r_q, 1.0))
                log_low = math.log(max(float(p.hetero_low), 1.0))
                log_high = math.log(max(float(p.hetero_high),
                                        float(p.hetero_low) + self.epsilon))
                g_hetero = self._ramp(log_r, log_low, log_high)
            else:
                g_hetero = 0.0
        else:
            g_hetero = 0.0
        self._diag_g_hetero = g_hetero

        max_b = 0.0
        for node in self.raw_tree.iter_subtrees():
            if node.data == "and_gate":
                children = self._children(node)
                if len(children) >= 2:
                    child_ds = [float(self._d0(c)) for c in children]
                    total = sum(child_ds)
                    if total > 0:
                        b = max(child_ds) / total
                        if b > max_b:
                            max_b = b
        g_bottleneck = self._ramp(max_b, p.bottleneck_low, p.bottleneck_high)
        self._diag_g_bottleneck = g_bottleneck

        return max(self.alpha_min, g_rare * max(g_hetero, g_bottleneck))

    def _leaf_difficulties_adaptive(self):
        raw = {}
        for name, q in self.leaf_q.items():
            raw[name] = max(1, int(round(
                -math.log(max(min(q, 1.0), self.epsilon)) * self.d_scale)))

        if not raw:
            self._diag_raw_d_min = 0
            self._diag_raw_d_max = 0
            self._diag_D_ref = 1.0
            self._diag_adapted_d_min = 0
            self._diag_adapted_d_max = 0
            return {}

        raw_vals = list(raw.values())
        self._diag_raw_d_min = min(raw_vals)
        self._diag_raw_d_max = max(raw_vals)

        log_sum = sum(math.log(max(float(v), 1.0)) for v in raw_vals)
        n = len(raw)
        D_ref = max(1.0, math.exp(log_sum / float(n)) if n > 0 else 1.0)
        self._diag_D_ref = D_ref

        alpha = self.alpha
        out = {}
        for name, d in raw.items():
            if alpha >= 0.999:
                out[name] = d
            elif alpha <= 0.001:
                out[name] = max(1, int(round(D_ref)))
            else:
                ratio = max(float(d), 1.0) / D_ref
                d_adapted = D_ref * (ratio ** alpha)
                out[name] = max(1, int(round(d_adapted)))

        adapted_vals = list(out.values())
        self._diag_adapted_d_min = min(adapted_vals)
        self._diag_adapted_d_max = max(adapted_vals)
        return out

    def _drem_expr(self, node, force_failed=None):
        force_failed = force_failed or frozenset()
        key = (id(node), tuple(sorted(force_failed)))
        if key in self.expr_cache:
            return self.expr_cache[key]

        if node.data == "pand_gate":
            children = self._children(node)
            if len(children) < 2:
                expr = str(self._d0(node))
            else:
                left, right = children[0], children[1]
                name = self._leaf_name(node)
                d0_top = max(1, self._d0(self.state_tree))
                INF = int(round(self.rdgt_params.inf_mult * d0_top))
                failed = self._node_failed_expr(node)
                blocked = "({}_st==4)".format(name)
                left_done = self._node_failed_expr(left)
                not_started = self._sum_expr([
                    self._drem_expr(left, force_failed),
                    str(self._d0(right)),
                ])
                armed = self._drem_expr(right, force_failed)
                raw = "(({})*({})+(1-({}))*({}))".format(
                    left_done, armed, left_done, not_started)
                expr = "(({})*0+(1-({}))*(({})*{}+(1-({}))*({})))".format(
                    failed, failed, blocked, INF, blocked, raw)
        else:
            expr = IfunN4Context._drem_expr(self, node, force_failed)

        self.expr_cache[key] = expr
        return expr

    def diagnostic_row(self):
        return {
            "n4a_version": "N4AN",
            "alpha": round(float(self.alpha), 6),
            "g_rare": round(float(getattr(self, "_diag_g_rare", 0)), 6),
            "g_hetero": round(float(getattr(self, "_diag_g_hetero", 0)), 6),
            "g_bottleneck": round(float(getattr(self, "_diag_g_bottleneck", 0)), 6),
            "d_ref": round(float(getattr(self, "_diag_D_ref", 1.0)), 2),
            "d0_top": int(getattr(self, "_diag_d0_top", 0)),
            "raw_d_min": int(getattr(self, "_diag_raw_d_min", 0)),
            "raw_d_max": int(getattr(self, "_diag_raw_d_max", 0)),
            "adapted_d_min": int(getattr(self, "_diag_adapted_d_min", 0)),
            "adapted_d_max": int(getattr(self, "_diag_adapted_d_max", 0)),
        }

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        fields = [
            "n4a_version", "alpha", "g_rare", "g_hetero", "g_bottleneck",
            "d_ref", "d0_top", "raw_d_min", "raw_d_max",
            "adapted_d_min", "adapted_d_max",
        ]
        return "N4AN " + ",".join("{}={}".format(k, row.get(k, "")) for k in fields)


class N5pPathScenario:
    def __init__(self, be_set=None, constraints=None, order_penalty=0.0, cost0=0.0):
        self.be_set = frozenset(be_set or [])
        self.constraints = tuple(sorted(constraints or []))
        self.order_penalty = float(order_penalty)
        self.cost0 = float(cost0)

    @property
    def constraints_sig(self):
        return self.constraints


class IfunN5pAtpeContext(IfunN4aContext):
    """Anchored Tempered Path-Ensemble RD-GT.

    This keeps N4A as the anchor progress and adds only a bounded positive
    correction from a Top-L tempered path ensemble.
    """

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, epsilon=1e-300, tuning=None,
                 alpha_min=0.1, rare_low=3.0, rare_high=8.0,
                 hetero_low=3.0, hetero_high=30.0,
                 bottleneck_low=0.40, bottleneck_high=0.70,
                 inf_mult=10.0, fixed_alpha_flag=0.0, fixed_alpha_value=0.0,
                 L=64, tau=0.5, W=6.0, tau_w=1.0, eta=0.5,
                 B_cap=0.05, N_eff_full=4.0):
        super().__init__(
            compiler, raw_tree, state_tree, mission_time=mission_time,
            scale=scale, d_scale=d_scale, epsilon=epsilon, tuning=tuning,
            alpha_min=alpha_min, rare_low=rare_low, rare_high=rare_high,
            hetero_low=hetero_low, hetero_high=hetero_high,
            bottleneck_low=bottleneck_low, bottleneck_high=bottleneck_high,
            inf_mult=inf_mult, fixed_alpha_flag=fixed_alpha_flag,
            fixed_alpha_value=fixed_alpha_value)
        self.L = max(1, int(L))
        self.tau = max(float(tau), 1e-9)
        self.W = max(float(W), 0.0)
        self.tau_w = max(float(tau_w), 1e-9)
        self.eta = max(float(eta), 0.0)
        self.B_cap = max(float(B_cap), 0.0)
        self.N_eff_full = max(float(N_eff_full), 1.0 + 1e-9)
        self.tau_cost = max(self.tau * self.d_scale, 1e-9)
        self.W_cost = max(self.W * self.d_scale, 0.0)
        self.tau_w_cost = max(self.tau_w * self.d_scale, 1e-9)
        self.fact_cache = {0: 0.0, 1: 0.0}
        self.pand_penalty = {}
        self.pand_child_count = {}
        self.pand_node_by_name = {}
        self.num_paths_raw = 0
        self.num_pand_constraints = 0
        self.has_pand = False
        self.path_entropy = 0.0
        self.N_eff = 1.0
        self.g_path = 0.0
        self.gap12 = 0.0
        self.D_PE0 = 0.0
        self.d_min0 = 0.0
        self.inf_path_cost = max(
            float(self.inf_mult) * float(max(1, self._d0(self.state_tree))),
            float(self.d_scale) * 1000.0,
        )
        self.paths = []
        self.valid = False
        self._build_context()

    def _fmt(self, value):
        return "{:.12g}".format(float(value))

    def _prod_expr(self, terms):
        terms = [str(t) for t in terms if str(t) not in {"", "1", "1.0"}]
        if not terms:
            return "1"
        if len(terms) == 1:
            return terms[0]
        return "(" + "*".join(terms) + ")"

    def _log_factorial(self, n):
        n = int(max(n, 0))
        if n not in self.fact_cache:
            start = max(self.fact_cache) + 1
            total = self.fact_cache[start - 1]
            for i in range(start, n + 1):
                total += math.log(float(i))
                self.fact_cache[i] = total
        return self.fact_cache[n]

    def _make_path(self, be_set=None, constraints=None):
        be_set = frozenset(be_set or [])
        constraints = tuple(sorted(set(constraints or [])))
        order_penalty = sum(self.pand_penalty.get(c, 0.0) for c in constraints)
        cost0 = sum(float(self.leaf_d.get(be, 1)) for be in be_set) + order_penalty
        return N5pPathScenario(be_set, constraints, order_penalty, cost0)

    def _empty_path(self):
        return self._make_path()

    def _merge_path(self, left, right):
        be_set = left.be_set | right.be_set
        constraints = tuple(sorted(set(left.constraints) | set(right.constraints)))
        return self._make_path(be_set, constraints)

    def _with_constraint(self, path, node):
        name = self._leaf_name(node)
        children = self._children(node)
        self.pand_child_count[name] = len(children)
        self.pand_node_by_name[name] = node
        self.pand_penalty[name] = self._log_factorial(len(children)) * float(self.d_scale)
        constraints = tuple(sorted(set(path.constraints) | {name}))
        return self._make_path(path.be_set, constraints)

    def _deduplicate(self, paths):
        best = {}
        for path in paths:
            key = (path.be_set, path.constraints_sig)
            if key not in best or path.cost0 < best[key].cost0:
                best[key] = path
        return list(best.values())

    def _prune_dominated(self, paths):
        kept = []
        for path in sorted(paths, key=lambda p: (p.cost0, len(p.be_set))):
            dominated = False
            for other in kept:
                if other.constraints_sig == path.constraints_sig and other.be_set.issubset(path.be_set):
                    dominated = True
                    break
            if not dominated:
                kept.append(path)
        return kept

    def _clean_paths(self, paths):
        self.num_paths_raw = max(self.num_paths_raw, len(paths))
        paths = self._deduplicate(paths)
        paths = self._prune_dominated(paths)
        return sorted(paths, key=lambda p: (p.cost0, len(p.be_set), p.constraints_sig))[:self.L]

    def _enumerate_paths(self, node):
        if node.data == "toplevel":
            return self._enumerate_paths(node.children[0])
        if node.data in {"be", "sbe"}:
            return [self._make_path({self._leaf_name(node)})]
        if node.data == "or_gate":
            paths = []
            for child in self._children(node):
                paths.extend(self._enumerate_paths(child))
            return self._clean_paths(paths)
        if node.data in {"and_gate", "spare_gate"}:
            paths = [self._empty_path()]
            for child in self._children(node):
                child_paths = self._enumerate_paths(child)
                merged = [self._merge_path(p, q) for p in paths for q in child_paths]
                paths = self._clean_paths(merged)
            return paths
        if node.data == "vot_gate":
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            k = max(0, min(k, len(self._children(node))))
            dp = [[] for _ in range(k + 1)]
            dp[0] = [self._empty_path()]
            for child in self._children(node):
                child_paths = self._enumerate_paths(child)
                new_dp = [list(dp[t]) for t in range(k + 1)]
                for t in range(k - 1, -1, -1):
                    for p in dp[t]:
                        for q in child_paths:
                            new_dp[t + 1].append(self._merge_path(p, q))
                for t in range(k + 1):
                    new_dp[t] = self._clean_paths(new_dp[t])
                dp = new_dp
            return dp[k]
        if node.data == "pand_gate":
            self.has_pand = True
            paths = [self._empty_path()]
            for child in self._children(node):
                child_paths = self._enumerate_paths(child)
                merged = [self._merge_path(p, q) for p in paths for q in child_paths]
                paths = self._clean_paths(merged)
            paths = [self._with_constraint(p, node) for p in paths]
            return self._clean_paths(paths)
        return []

    def _tempered_softmin_value(self, costs, tau_cost, W_cost):
        finite = [float(c) for c in costs if math.isfinite(float(c))]
        if not finite:
            return math.inf
        m = min(finite)
        active = [c for c in finite if c <= m + W_cost]
        if not active:
            return math.inf
        s = sum(math.exp(-(c - m) / tau_cost) for c in active)
        return max(0.0, m - tau_cost * math.log(max(s, self.epsilon)))

    def _initial_path_weights(self, paths):
        active = [p for p in paths if p.cost0 <= self.d_min0 + self.W_cost]
        if not active:
            return []
        vals = [math.exp(-(p.cost0 - self.d_min0) / self.tau_w_cost) for p in active]
        total = sum(vals)
        if total <= 0.0:
            return []
        return [v / total for v in vals]

    def _build_context(self):
        paths = self._enumerate_paths(self.state_tree)
        self.num_paths_raw = max(self.num_paths_raw, len(paths))
        paths = self._clean_paths(paths)
        if not paths:
            return
        self.paths = paths
        self.d_min0 = min(p.cost0 for p in paths)
        if self.d_min0 <= 0.0 or not math.isfinite(self.d_min0):
            return
        costs0 = [p.cost0 for p in paths]
        self.D_PE0 = self._tempered_softmin_value(costs0, self.tau_cost, self.W_cost)
        if not math.isfinite(self.D_PE0):
            return
        weights = self._initial_path_weights(paths)
        self.path_entropy = -sum(w * math.log(w) for w in weights if w > 0.0)
        self.N_eff = math.exp(self.path_entropy) if weights else 1.0
        self.g_path = max(0.0, min(1.0, (self.N_eff - 1.0) / (self.N_eff_full - 1.0)))
        if len(costs0) >= 2:
            sorted_costs = sorted(costs0)
            self.gap12 = sorted_costs[1] - sorted_costs[0]
        self.num_pand_constraints = sum(len(p.constraints) for p in paths)
        self.valid = True

    def _min_expr(self, terms, default=None):
        terms = [str(t) for t in terms if str(t)]
        if not terms:
            return str(default if default is not None else self.inf_path_cost)
        expr = terms[0]
        for term in terms[1:]:
            expr = "min({},({}))".format(expr, term)
        return expr

    def _path_order_penalty_expr(self, path):
        terms = []
        for name in path.constraints:
            penalty = self.pand_penalty.get(name, 0.0)
            if penalty <= 0.0:
                continue
            fresh = "(({}_st==0)*(1-({}_f0))*(1-({}_f1)))".format(name, name, name)
            terms.append("(({})*{})".format(fresh, self._fmt(penalty)))
        return self._sum_expr(terms)

    def _path_blocked_expr(self, path):
        terms = []
        for name in path.constraints:
            terms.append("({}_st==4)".format(name))
            terms.append("(({}_st==0)*({}_f1)*(1-({}_f0)))".format(name, name, name))
        return self._min_expr(["1", self._sum_expr(terms)], default="0") if terms else "0"

    def _path_residual_expr(self, path):
        terms = []
        for be in sorted(path.be_set):
            terms.append("((1-({}_broken>0))*{})".format(be, self.leaf_d.get(be, 1)))
        order_penalty = self._path_order_penalty_expr(path)
        if order_penalty != "0":
            terms.append(order_penalty)
        raw = self._sum_expr(terms)
        blocked = self._path_blocked_expr(path)
        if blocked == "0":
            return raw
        return "((({})*{})+(1-({}))*({}))".format(
            blocked, self._fmt(self.inf_path_cost), blocked, raw)

    def _tempered_softmin_expr(self, cost_exprs):
        if not cost_exprs:
            return self._fmt(self.inf_path_cost)
        m = self._min_expr(cost_exprs, default=self._fmt(self.inf_path_cost))
        terms = []
        for d in cost_exprs:
            finite = "(({})<{})".format(d, self._fmt(self.inf_path_cost))
            active = "(({})*(({})<=(({})+{})))".format(finite, d, m, self._fmt(self.W_cost))
            terms.append("(({})*exp(-(({})-({}))/{}))".format(
                active, d, m, self._fmt(self.tau_cost)))
        s = self._sum_expr(terms)
        return "max(0,(({})-({})*log(max({},({})))))".format(
            m, self._fmt(self.tau_cost), self._fmt(self.epsilon), s)

    def progress_expr(self, force_failed=None):
        n4a_progress, expr_max = IfunN4aContext.progress_expr(self, force_failed=force_failed)
        if (not self.valid) or self.g_path <= 0.0:
            return n4a_progress, expr_max
        cost_exprs = [self._path_residual_expr(path) for path in self.paths]
        d_pe = self._tempered_softmin_expr(cost_exprs)
        S = self.scale
        p_pe = "min({},max(0,floor((({}-({}))*{})/{})))".format(
            S, self._fmt(self.D_PE0), d_pe, S, self._fmt(self.d_min0))
        delta_raw = "max(0,({})-({}))".format(p_pe, n4a_progress)
        delta_cap = int(round(float(S) * min(self.B_cap, 1.0)))
        delta_gain = self._fmt(self.eta * self.g_path)
        delta = "min({},floor(({})*({})))".format(delta_cap, delta_gain, delta_raw)
        expr = "min({},max(0,({})+({})))".format(S, n4a_progress, delta)
        return expr, S

    def diagnostic_row(self):
        return {
            "model_id": os.environ.get("N5P_MODEL_ID", ""),
            "num_paths_raw": self.num_paths_raw,
            "num_paths_kept": len(self.paths),
            "d_min0": self.d_min0,
            "D_PE0": self.D_PE0,
            "N_eff": self.N_eff,
            "g_path": self.g_path,
            "path_entropy": self.path_entropy,
            "gap12": self.gap12,
            "has_pand": int(bool(self.has_pand)),
            "num_pand_constraints": self.num_pand_constraints,
        }

    def diagnostic_comment(self):
        row = self.diagnostic_row()
        fields = [
            "model_id", "num_paths_raw", "num_paths_kept", "d_min0",
            "D_PE0", "N_eff", "g_path", "path_entropy", "gap12",
            "has_pand", "num_pand_constraints",
        ]
        return ",".join("{}={}".format(k, row.get(k, "")) for k in fields)


class IfunN5cContext(IfunN4aContext):
    """Committor-Corrected RD-GT.

    Runtime expression estimates top-event probability recursively and maps
    D(x)=-log(pi_top(x)) to the standard [0, scale] progress range.
    """

    def __init__(self, compiler, raw_tree, state_tree, mission_time=1000.0,
                 scale=1000, d_scale=100.0, epsilon=1e-300, tuning=None,
                 alpha_min=0.1, rare_low=3.0, rare_high=8.0,
                 hetero_low=3.0, hetero_high=30.0,
                 bottleneck_low=0.40, bottleneck_high=0.70,
                 inf_mult=10.0, fixed_alpha_flag=0.0, fixed_alpha_value=0.0,
                 eps_prob=1e-30, pand_order_factor=False, shared_guard=False,
                 fallback_kind="n4a"):
        super().__init__(
            compiler, raw_tree, state_tree, mission_time=mission_time,
            scale=scale, d_scale=d_scale, epsilon=epsilon, tuning=tuning,
            alpha_min=alpha_min, rare_low=rare_low, rare_high=rare_high,
            hetero_low=hetero_low, hetero_high=hetero_high,
            bottleneck_low=bottleneck_low, bottleneck_high=bottleneck_high,
            inf_mult=inf_mult, fixed_alpha_flag=fixed_alpha_flag,
            fixed_alpha_value=fixed_alpha_value)
        self.eps_prob = max(float(eps_prob), 1e-38)
        self.pand_order_factor = bool(pand_order_factor)
        self.shared_guard = bool(shared_guard)
        self.fallback_kind = str(fallback_kind or "n4a").lower()
        self.prob_cache = {}
        self.support_cache = {}
        self.fact_cache = {0: 0.0, 1: 0.0}
        self.has_shared_event_overlap = self._has_shared_overlap(self.state_tree)
        self.d0_prob = max(self._static_difficulty(self.state_tree), 1e-12)

    def _fmt(self, value):
        return "{:.12g}".format(float(value))

    def _prod_expr(self, terms):
        terms = [str(t) for t in terms if str(t) not in {"", "1", "1.0"}]
        if not terms:
            return "1"
        if len(terms) == 1:
            return terms[0]
        return "(" + "*".join(terms) + ")"

    def _clamp_prob_expr(self, expr):
        return "min(1,max({},({})))".format(self._fmt(self.eps_prob), expr)

    def _neg_log_expr(self, prob_expr):
        # FIG's adhoc IFUN path is parsed by exprtk, where log(x) is natural log.
        return "(-log(max({},({}))))".format(self._fmt(self.eps_prob), prob_expr)

    def _log_factorial(self, n):
        n = int(max(n, 0))
        if n not in self.fact_cache:
            start = max(self.fact_cache) + 1
            total = self.fact_cache[start - 1]
            for i in range(start, n + 1):
                total += math.log(float(i))
                self.fact_cache[i] = total
        return self.fact_cache[n]

    def _support(self, node):
        nid = id(node)
        if nid in self.support_cache:
            return self.support_cache[nid]
        if node.data == "toplevel":
            s = self._support(node.children[0])
        elif node.data in {"be", "sbe"}:
            s = {self._leaf_name(node)}
        else:
            s = set()
            for child in self._children(node):
                s |= self._support(child)
        self.support_cache[nid] = s
        return s

    def _has_shared_overlap(self, node):
        if node.data == "toplevel":
            return self._has_shared_overlap(node.children[0])
        children = self._children(node) if node.data not in {"be", "sbe"} else []
        seen = set()
        for child in children:
            child_set = self._support(child)
            if seen & child_set:
                return True
            seen |= child_set
        return any(self._has_shared_overlap(c) for c in children)

    def _leaf_prob_const(self, name):
        d = float(self.leaf_d.get(name, 1)) / max(float(self.d_scale), 1.0)
        return max(self.eps_prob, min(1.0, math.exp(-d)))

    def _static_prob(self, node):
        if node.data == "toplevel":
            return self._static_prob(node.children[0])
        if node.data in {"be", "sbe"}:
            return self._leaf_prob_const(self._leaf_name(node))
        if node.data == "and_gate" or node.data == "spare_gate":
            p = 1.0
            for child in self._children(node):
                p *= self._static_prob(child)
            return max(self.eps_prob, min(1.0, p))
        if node.data == "or_gate":
            surv = 1.0
            for child in self._children(node):
                surv *= (1.0 - self._static_prob(child))
            return max(self.eps_prob, min(1.0, 1.0 - surv))
        if node.data == "vot_gate":
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            probs = [self._static_prob(c) for c in self._children(node)]
            n = len(probs)
            if k <= 0:
                return 1.0
            if k > n:
                return self.eps_prob
            dp = [0.0] * (n + 1)
            dp[0] = 1.0
            processed = 0
            for p in probs:
                processed += 1
                nxt = [0.0] * (n + 1)
                for j in range(0, processed):
                    nxt[j] += dp[j] * (1.0 - p)
                    nxt[j + 1] += dp[j] * p
                dp = nxt
            return max(self.eps_prob, min(1.0, sum(dp[k:])))
        if node.data == "pand_gate":
            children = self._children(node)
            p = 1.0
            for child in children:
                p *= self._static_prob(child)
            if self.pand_order_factor:
                p *= math.exp(-self._log_factorial(len(children)))
            return max(self.eps_prob, min(1.0, p))
        return self.eps_prob

    def _static_difficulty(self, node):
        return -math.log(max(self._static_prob(node), self.eps_prob))

    def _prob_expr(self, node):
        nid = id(node)
        if nid in self.prob_cache:
            return self.prob_cache[nid]

        if node.data == "toplevel":
            expr = self._prob_expr(node.children[0])
        elif node.data in {"be", "sbe"}:
            name = self._leaf_name(node)
            p = self._fmt(self._leaf_prob_const(name))
            expr = "(({}_broken>0)+(({}_broken==0)*{}))".format(name, name, p)
        elif node.data == "and_gate" or node.data == "spare_gate":
            failed = self._node_failed_expr(node)
            parts = []
            for child in self._children(node):
                c_failed = self._node_failed_expr(child)
                parts.append("(({})+((1-({}))*({})))".format(c_failed, c_failed, self._prob_expr(child)))
            raw = self._prod_expr(parts)
            expr = "(({})+((1-({}))*({})))".format(failed, failed, raw)
        elif node.data == "or_gate":
            failed = self._node_failed_expr(node)
            surv_terms = ["(1-({}))".format(self._prob_expr(c)) for c in self._children(node)]
            raw = "(1-({}))".format(self._prod_expr(surv_terms))
            expr = "(({})+((1-({}))*({})))".format(failed, failed, raw)
        elif node.data == "vot_gate":
            failed = self._node_failed_expr(node)
            try:
                k = int(node.children[1])
            except Exception:
                k = 1
            child_probs = [self._prob_expr(c) for c in self._children(node)]
            n = len(child_probs)
            if k <= 0:
                raw = "1"
            elif k > n:
                raw = self._fmt(self.eps_prob)
            else:
                dp = ["0"] * (n + 1)
                dp[0] = "1"
                processed = 0
                for p in child_probs:
                    processed += 1
                    nxt = ["0"] * (n + 1)
                    for j in range(0, processed):
                        false_term = "({})*(1-({}))".format(dp[j], p)
                        true_term = "({})*({})".format(dp[j], p)
                        nxt[j] = "(({})+({}))".format(nxt[j], false_term) if nxt[j] != "0" else false_term
                        nxt[j + 1] = "(({})+({}))".format(nxt[j + 1], true_term) if nxt[j + 1] != "0" else true_term
                    dp = nxt
                raw = self._sum_expr(dp[k:])
            expr = "(({})+((1-({}))*({})))".format(failed, failed, raw)
        elif node.data == "pand_gate":
            children = self._children(node)
            name = self._leaf_name(node)
            failed = self._node_failed_expr(node)
            blocked = "({}_st==4)".format(name)
            if len(children) < 2:
                raw = self._fmt(self.eps_prob)
            else:
                left, right = children[0], children[1]
                left_done = self._node_failed_expr(left)
                not_started = self._prod_expr([self._prob_expr(left), self._prob_expr(right)])
                armed = self._prob_expr(right)
                if self.pand_order_factor:
                    not_started = "({})*({})".format(not_started, self._fmt(math.exp(-self._log_factorial(2))))
                raw = "(({})*({})+(1-({}))*({}))".format(left_done, armed, left_done, not_started)
            block_p = math.exp(-max(self.inf_mult * self.d0_prob, 1e-12))
            block_p = max(self.eps_prob, min(1.0, block_p))
            expr = "(({})+((1-({}))*(({})*{}+(1-({}))*({}))))".format(
                failed, failed, blocked, self._fmt(block_p), blocked, raw)
        else:
            expr = self._fmt(self.eps_prob)

        expr = self._clamp_prob_expr(expr)
        self.prob_cache[nid] = expr
        return expr

    def progress_expr(self, force_failed=None):
        if self.shared_guard and self.has_shared_event_overlap:
            return IfunN4aContext.progress_expr(self, force_failed=force_failed)
        ptop = self._prob_expr(self.state_tree)
        d_expr = self._neg_log_expr(ptop)
        d0 = max(self.d0_prob, 1e-12)
        S = self.scale
        expr = "min({},max(0,floor((({}-({}))*{})/{})))".format(
            S, self._fmt(d0), d_expr, S, self._fmt(d0))
        return expr, S


class PwstrContext:

    def __init__(self, compiler, raw_tree, mission_time, tuning=None):
        self.compiler = compiler
        self.raw_tree = raw_tree
        self.mission_time = max(float(mission_time or 1.0), 1e-10)
        self.tuning = tuning or PwstrTuning()
        self.q_cache = {}
        self.a_weights = {}
        self.b_weights = {}
        self._node_map = {}
        self._build_node_map()
        self._compute_all_q()
        self._compute_all_weights()

    def _build_node_map(self):
        for kind in ["be", "sbe", "and_gate", "or_gate", "vot_gate", "pand_gate", "spare_gate"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                self._node_map[name] = node

    def _get_child_name(self, child):
        if isinstance(child, Token):
            return str(child)
        elif isinstance(child, Tree):
            return str(child.children[0]) if child.children else ""
        return ""

    def _extract_lambda(self, node):
        _proxy = 0.0
        for spec in node.children[1:]:
            if not isinstance(spec, Tree):
                continue
            if spec.data in {"fail", "dorm"}:
                _proxy = max(_proxy, _dist_tree_proxy(spec.children[0]))
        _name = str(node.children[0])
        _ovr = self.compiler.phase_overrides.get(_name, {})
        for _dist in _ovr.get('fail', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        for _dist in _ovr.get('dorm', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        return _proxy

    def _compute_all_q(self):
        eps = self.tuning.epsilon
        for kind in ["be", "sbe"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                lam = self._extract_lambda(node)
                if lam <= 0.0:
                    lam = 1.0
                q = 1.0 - math.exp(-lam * self.mission_time)
                q = max(eps, min(1.0 - eps, q))
                self.q_cache[name] = q
        tl = next(self.raw_tree.find_data("toplevel"))
        top_name = str(tl.children[0])
        self._compute_gate_q(top_name)

    def _compute_gate_q(self, name):
        if name in self.q_cache:
            return self.q_cache[name]
        node = self._node_map.get(name)
        if node is None:
            self.q_cache[name] = 0.0
            return 0.0
        kind = node.data
        if kind in {"be", "sbe"}:
            return self.q_cache.get(name, 0.0)
        child_qs = []
        if kind == "vot_gate":
            for child in node.children[3:]:
                cn = self._get_child_name(child)
                if cn:
                    child_qs.append(self._compute_gate_q(cn))
        else:
            for child in node.children[1:]:
                cn = self._get_child_name(child)
                if cn:
                    child_qs.append(self._compute_gate_q(cn))
        eps = self.tuning.epsilon
        if not child_qs:
            q = eps
        elif kind == "and_gate":
            q = 1.0
            for cq in child_qs:
                q *= cq
        elif kind == "or_gate":
            q = 1.0
            for cq in child_qs:
                q *= (1.0 - cq)
            q = 1.0 - q
        elif kind == "vot_gate":
            k = int(str(node.children[1]))
            q = self._vot_prob_at_least_k(child_qs, k)
        elif kind == "pand_gate":
            if len(child_qs) >= 2:
                q_l, q_r = child_qs[0], child_qs[1]
                rho_l = -math.log(max(1.0 - q_l, eps)) / self.mission_time
                rho_r = -math.log(max(1.0 - q_r, eps)) / self.mission_time
                denom = rho_l + rho_r
                theta = rho_l / denom if denom > 0.0 else 0.5
                q = q_l * q_r * theta
            else:
                q = 1.0
        elif kind == "spare_gate":
            q = 1.0
            for cq in child_qs:
                q *= cq
        else:
            q = eps
        q = max(eps, min(1.0 - eps, q))
        self.q_cache[name] = q
        return q

    @staticmethod
    def _vot_prob_at_least_k(probs, k):
        n = len(probs)
        if k <= 0:
            return 1.0
        if k > n:
            return 0.0
        dp = [0.0] * (k + 1)
        dp[0] = 1.0
        for p in probs:
            for j in range(min(k, len(dp) - 1), 0, -1):
                dp[j] = dp[j] * (1.0 - p) + dp[j - 1] * p
            dp[0] = dp[0] * (1.0 - p)
        result = sum(dp[i] for i in range(k, len(dp)))
        return result

    def _compute_all_weights(self):
        eps = self.tuning.epsilon
        for kind in ["and_gate", "or_gate", "vot_gate", "pand_gate", "spare_gate"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                if kind == "vot_gate":
                    children = node.children[3:]
                else:
                    children = node.children[1:]
                child_names = [self._get_child_name(c) for c in children]
                child_qs = [self.q_cache.get(cn, eps) for cn in child_names]
                child_qs_safe = [max(q, eps) for q in child_qs]
                if not child_qs_safe:
                    continue
                log_sum = sum(math.log(q) for q in child_qs_safe)
                q_ref = math.exp(log_sum / len(child_qs_safe))
                for cn, cq in zip(child_names, child_qs):
                    if not cn:
                        continue
                    cq_safe = max(cq, eps)
                    a_w = (q_ref / cq_safe) ** self.tuning.beta_req
                    a_w = max(self.tuning.a_min, min(self.tuning.a_max, a_w))
                    self.a_weights[(name, cn)] = a_w
                    b_w = (cq_safe / q_ref) ** self.tuning.beta_alt
                    b_w = max(self.tuning.b_min, min(self.tuning.b_max, b_w))
                    self.b_weights[(name, cn)] = b_w


class Tree2PWSTRIfun(Visitor):

    def __init__(self, active_guard=False, pwstr_context=None, tuning=None):
        self.active_guard = bool(active_guard)
        self.ctx = pwstr_context
        self.tuning = tuning or PwstrTuning()
        self.K = float(self.tuning.k)

    @staticmethod
    def _fmt(v):
        if isinstance(v, float):
            if v == int(v):
                return str(int(v))
            return "{:.8g}".format(v)
        return str(v)

    @staticmethod
    def _max_expr(terms):
        if len(terms) == 1:
            return str(terms[0])
        result = "max({},{})".format(terms[0], terms[1])
        for t in terms[2:]:
            result = "max({},{})".format(result, t)
        return result

    def _visit_children(self, tree, start_idx=1):
        for c in tree.children[start_idx:]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)

    def _gate_name(self, tree):
        return str(tree.children[0])

    def _child_name(self, child):
        if isinstance(child, Tree):
            return str(child.children[0])
        return str(child)

    def toplevel(self, tree):
        self.visit(tree.children[0])
        root = tree.children[0]
        tree.ifun = "floor({}*({})+0.5)".format(self._fmt(self.K), root.ifun)
        tree.maxi = self.tuning.k

    def be(self, tree):
        n = str(tree.children[0])
        tree.ifun = "({}_broken>0)".format(n)
        tree.maxi = 1

    def sbe(self, tree):
        n = str(tree.children[0])
        tree.ifun = "({}_broken>0)".format(n)
        tree.maxi = 1

    def and_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        a_list = []
        for c in children:
            cn = self._child_name(c)
            a = self.ctx.a_weights.get((name, cn), 1.0) if self.ctx else 1.0
            a_list.append(a)
        sum_a = sum(a_list) if a_list else 1.0
        terms = ["({}*({}))".format(self._fmt(a), c.ifun) for a, c in zip(a_list, children)]
        num = "(" + "+".join(terms) + ")"
        if abs(sum_a - 1.0) < 1e-12:
            tree.ifun = num
        else:
            tree.ifun = "({}/{})".format(num, self._fmt(sum_a))
        tree.maxi = 1

    def or_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        b_list = []
        for c in children:
            cn = self._child_name(c)
            b = self.ctx.b_weights.get((name, cn), 1.0) if self.ctx else 1.0
            b_list.append(b)
        b_max = max(b_list) if b_list else 1.0
        terms = [c.ifun for c in children]  # z_v = max(children's S values)
        for c, b in zip(children, b_list):
            if abs(b - b_max) < 1e-12:
                terms.append(c.ifun)
            else:
                terms.append("({}*({})/{})".format(self._fmt(b), c.ifun, self._fmt(b_max)))
        tree.ifun = self._max_expr(terms)
        tree.maxi = 1

    def vot_gate(self, tree):
        self._visit_children(tree, 3)
        name = self._gate_name(tree)
        k = int(str(tree.children[1]))
        children = [c for c in tree.children[3:] if isinstance(c, Tree)]
        n = len(children)
        if k <= 0:
            tree.ifun = "1"
            tree.maxi = 1
            return
        if k > n:
            tree.ifun = "0"
            tree.maxi = 1
            return
        combos = list(combinations(range(n), k))
        best_terms = []
        d_max_val = 0.0
        if self.ctx:
            for combo in combos:
                d_w = 1.0
                for idx in combo:
                    cn = self._child_name(children[idx])
                    b = self.ctx.b_weights.get((name, cn), 1.0)
                    d_w *= b
                d_max_val = max(d_max_val, d_w)
        if d_max_val <= 0.0:
            d_max_val = 1.0
        for combo in combos:
            combo_children = [children[i] for i in combo]
            if self.ctx:
                a_list = [self.ctx.a_weights.get((name, self._child_name(c)), 1.0) for c in combo_children]
            else:
                a_list = [1.0] * len(combo_children)
            sum_a = sum(a_list) if a_list else 1.0
            r_num = "+".join(["({}*({}))".format(self._fmt(a), children[i].ifun)
                             for i, a in zip(combo, a_list)])
            if abs(sum_a - 1.0) < 1e-12:
                r_w = "({})".format(r_num)
            else:
                r_w = "({})/({})".format(r_num, self._fmt(sum_a))
            if self.ctx:
                d_w = 1.0
                for idx in combo:
                    cn = self._child_name(children[idx])
                    b = self.ctx.b_weights.get((name, cn), 1.0)
                    d_w *= b
            else:
                d_w = 1.0
            if abs(d_w - d_max_val) < 1e-12:
                best_terms.append(r_w)
            else:
                best_terms.append("({}*({})/{})".format(self._fmt(d_w), r_w, self._fmt(d_max_val)))
        tree.ifun = self._max_expr(best_terms)
        tree.maxi = 1

    def pand_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        left = tree.children[1]
        right = tree.children[2]
        ln = self._child_name(left)
        rn = self._child_name(right)
        a_l = self.ctx.a_weights.get((name, ln), 1.0) if self.ctx else 1.0
        a_r = self.ctx.a_weights.get((name, rn), 1.0) if self.ctx else 1.0
        sum_a = a_l + a_r
        st = "{}_st".format(name)
        s_l = left.ifun
        s_r = right.ifun
        if abs(sum_a - 1.0) < 1e-12:
            state1_val = "({}*({})+{}*({}))".format(self._fmt(a_l), s_l, self._fmt(a_r), s_r)
            state0_num = "({}*({})-{}*({}))".format(self._fmt(a_l), s_l, self._fmt(a_r), s_r)
        else:
            state1_val = "({}*({})+{}*({}))".format(self._fmt(a_l / sum_a), s_l, self._fmt(a_r / sum_a), s_r)
            state0_num = "({}*({})-{}*({}))".format(self._fmt(a_l / sum_a), s_l, self._fmt(a_r / sum_a), s_r)
        state2 = "({}==2)".format(st)
        state1 = "({}==1)*({})".format(st, state1_val)
        state0 = "({}==0)*max(0,({}))".format(st, state0_num)
        tree.ifun = "max({},({})+({}))".format(state2, state1, state0)
        tree.maxi = 1

    def spare_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        a_list = []
        for c in children:
            cn = self._child_name(c)
            a = self.ctx.a_weights.get((name, cn), 1.0) if self.ctx else 1.0
            a_list.append(a)
        sum_a = sum(a_list) if a_list else 1.0
        terms = ["({}*({}))".format(self._fmt(a), c.ifun) for a, c in zip(a_list, children)]
        num = "(" + "+".join(terms) + ")"
        if abs(sum_a - 1.0) < 1e-12:
            avg = num
        else:
            avg = "({}/{})".format(num, self._fmt(sum_a))
        z_v = "({}_state==4)".format(name)
        tree.ifun = "max({},{})".format(z_v, avg)
        tree.maxi = 1


class T3Context:

    def __init__(self, compiler, raw_tree, mission_time, tuning=None):
        self.compiler = compiler
        self.raw_tree = raw_tree
        self.mission_time = max(float(mission_time or 1.0), 1e-10)
        self.tuning = tuning or T3Tuning()
        self.p_constants = {}
        self._node_map = {}
        self._build_node_map()
        self._compute_all_p()

    def _build_node_map(self):
        for kind in ["be", "sbe", "and_gate", "or_gate", "vot_gate", "pand_gate", "spare_gate"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                self._node_map[name] = node

    def _get_child_name(self, child):
        if isinstance(child, Token):
            return str(child)
        elif isinstance(child, Tree):
            return str(child.children[0]) if child.children else ""
        return ""

    def _extract_lambda(self, node):
        _proxy = 0.0
        for spec in node.children[1:]:
            if not isinstance(spec, Tree):
                continue
            if spec.data in {"fail", "dorm"}:
                _proxy = max(_proxy, _dist_tree_proxy(spec.children[0]))
        _name = str(node.children[0])
        _ovr = self.compiler.phase_overrides.get(_name, {})
        for _dist in _ovr.get('fail', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        for _dist in _ovr.get('dorm', {}).values():
            _proxy = max(_proxy, _dist_str_proxy(_dist))
        return _proxy

    def _compute_all_p(self):
        eps = self.tuning.epsilon
        for kind in ["be", "sbe"]:
            for node in self.raw_tree.find_data(kind):
                name = str(node.children[0])
                lam = self._extract_lambda(node)
                if lam <= 0.0:
                    lam = 1.0
                p = 1.0 - math.exp(-lam * self.mission_time)
                p = max(eps, min(1.0 - eps, p))
                self.p_constants[name] = p


class Tree2T3Ifun(Visitor):

    def __init__(self, active_guard=False, t3_context=None, tuning=None):
        self.active_guard = bool(active_guard)
        self.ctx = t3_context
        self.tuning = tuning or T3Tuning()
        self.K = float(self.tuning.k)
        self.phys_k = int(self.tuning.phys_k)
        self.eta = float(self.tuning.eta)

    @staticmethod
    def _fmt(v):
        if isinstance(v, float):
            if v == int(v):
                return str(int(v))
            return "{:.8g}".format(v)
        return str(v)

    @staticmethod
    def _max_expr(terms):
        if len(terms) == 1:
            return str(terms[0])
        result = "max({},{})".format(terms[0], terms[1])
        for t in terms[2:]:
            result = "max({},{})".format(result, t)
        return result

    def _visit_children(self, tree, start_idx=1):
        for c in tree.children[start_idx:]:
            if isinstance(c, Tree) and not hasattr(c, "ifun"):
                self.visit(c)

    def _gate_name(self, tree):
        return str(tree.children[0])

    def _child_name(self, child):
        if isinstance(child, Tree):
            return str(child.children[0])
        return str(child)

    def toplevel(self, tree):
        self.visit(tree.children[0])
        root = tree.children[0]
        phys_score = root.ifun  # P_TOP in [0, 1]
        l_expr = "floor({}*{})".format(self._fmt(float(self.phys_k)), phys_score)
        multiplier = "(1+{}*({}))".format(self._fmt(self.eta), l_expr)
        tree.ifun = "floor({}*({})*({})+0.5)".format(
            self._fmt(self.K), phys_score, multiplier)
        tree.maxi = self.tuning.k

    def be(self, tree):
        n = str(tree.children[0])
        p_i = self.ctx.p_constants.get(n, 0.5) if self.ctx else 0.5
        tree.ifun = "({}+(1-({}))*({}_broken>0))".format(self._fmt(p_i), self._fmt(p_i), n)
        tree.maxi = 1

    def sbe(self, tree):
        n = str(tree.children[0])
        p_i = self.ctx.p_constants.get(n, 0.5) if self.ctx else 0.5
        tree.ifun = "({}+(1-({}))*({}_broken>0))".format(self._fmt(p_i), self._fmt(p_i), n)
        tree.maxi = 1

    def and_gate(self, tree):
        self._visit_children(tree, 1)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        terms = ["({})".format(c.ifun) for c in children]
        # P_AND = product of children pushabilities
        tree.ifun = "(" + "*".join(terms) + ")"
        tree.maxi = 1

    def or_gate(self, tree):
        self._visit_children(tree, 1)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        terms = ["(1-({}))".format(c.ifun) for c in children]
        # P_OR = 1 - product(1 - P_child)
        tree.ifun = "(1-(" + "*".join(terms) + "))"
        tree.maxi = 1

    def vot_gate(self, tree):
        self._visit_children(tree, 3)
        k = int(str(tree.children[1]))
        children = [c for c in tree.children[3:] if isinstance(c, Tree)]
        n = len(children)
        if k <= 0:
            tree.ifun = "1"
            tree.maxi = 1
            return
        if k > n:
            tree.ifun = "0"
            tree.maxi = 1
            return
        terms = ["({})".format(c.ifun) for c in children]
        tree.ifun = "(1-(1-({}))*({}))".format(
            ")*".join(terms[:k]), ")*(1-".join(terms[k:]) if k < n else "1")
        tree.maxi = 1

    def pand_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        st = "{}_st".format(name)
        left = tree.children[1]
        right = tree.children[2]
        p_l = left.ifun
        p_r = right.ifun
        state2_val = "1"
        state4_val = "0"
        state1_val = p_r
        state0_val = "({})*(1-({}))".format(p_l, p_r)
        tree.ifun = "max(({}==2)*{},max(({}==4)*{},max(({}==1)*{},({}==0)*{})))".format(
            st, state2_val, st, state4_val, st, state1_val, st, state0_val)
        tree.maxi = 1

    def spare_gate(self, tree):
        self._visit_children(tree, 1)
        name = self._gate_name(tree)
        children = [c for c in tree.children[1:] if isinstance(c, Tree)]
        terms = ["({})".format(c.ifun) for c in children]
        p_and = "(" + "*".join(terms) + ")"
        z_v = "({}_state==4)".format(name)
        tree.ifun = "max({},{})".format(z_v, p_and)
        tree.maxi = 1


class SplitFormulaBuilder:

    def __init__(self, compiler, raw_tree, tree, transient_horizon=0.0, tuning=None):
        self.compiler = compiler
        self.raw_tree = raw_tree
        self.tree = tree
        self.transient_horizon = max(float(transient_horizon or 0.0), 0.0)
        self.tuning = tuning or HeuristicTuning()
        self.nodes = {}
        self.leaves = {}
        for node in raw_tree.find_pred(lambda t: isinstance(t, Tree) and getattr(t, "children", None)):
            _name = node.children[0]
            if isinstance(_name, Token):
                _name = _name.value
            elif not isinstance(_name, str):
                continue
            self.nodes[_name] = node
            if node.data in {"be", "sbe"}:
                self.leaves[_name] = self._leaf_info(node)
        self.root = self.tree.children[0] if self.tree.data == "toplevel" else self.tree
        self.max_cache = {}
        self.fail_prob_cache = {}
        self.dist_cache = {}
        self.frontier_cache = {}
        self.root_max = max(int(self._max_steps(self.root)), 1)
        self.inf_value = self.root_max + 1

    def _fmt(self, value):
        if isinstance(value, str):
            return value
        return "{:.12g}".format(float(value))

    def _sum_expr(self, terms):
        terms = [str(t) for t in terms if str(t) not in {"", "0", "0.0"}]
        if not terms:
            return "0"
        if len(terms) == 1:
            return terms[0]
        return "(" + "+".join(terms) + ")"

    def _prod_expr(self, terms):
        terms = [str(t) for t in terms if str(t) not in {"", "1", "1.0"}]
        if not terms:
            return "1"
        if len(terms) == 1:
            return terms[0]
        return "(" + "*".join(terms) + ")"

    def _min_expr(self, terms, default="0"):
        terms = [str(t) for t in terms if str(t) not in {""}]
        if not terms:
            return str(default)
        _expr = terms[0]
        for term in terms[1:]:
            _expr = "min({},{})".format(_expr, term)
        return _expr

    def _max_expr(self, terms, default="0"):
        terms = [str(t) for t in terms if str(t) not in {""}]
        if not terms:
            return str(default)
        _expr = terms[0]
        for term in terms[1:]:
            _expr = "max({},{})".format(_expr, term)
        return _expr

    def _if_expr(self, cond, when_true, when_false):
        return "(({})*({})+((1-({}))*({})))".format(cond, when_true, cond, when_false)

    def _node_name(self, node):
        return str(node.children[0])

    def _node_failed_expr(self, node):
        if node.data == "toplevel":
            return self._node_failed_expr(node.children[0])
        if node.data in {"be", "sbe"}:
            return "({}_broken>0)".format(self._node_name(node))
        if node.data == "or_gate":
            return "({}_count!=0)".format(self._node_name(node))
        if node.data == "and_gate":
            return "({}_count=={})".format(self._node_name(node), len(node.children) - 1)
        if node.data == "vot_gate":
            return "({}_count>={})".format(self._node_name(node), int(node.children[1]))
        if node.data == "pand_gate":
            _name = self._node_name(node)
            return "(({}_st==1)|({}_st==2))".format(_name, _name)
        if node.data == "spare_gate":
            return "({}_state==4)".format(self._node_name(node))
        raise Exception("Split formula unsupported for node type {}".format(node.data))

    def _max_steps(self, node):
        try:
            return self.max_cache[id(node)]
        except:
            pass
        if node.data == "toplevel":
            _maxv = self._max_steps(node.children[0])
        elif node.data in {"be", "sbe"}:
            _maxv = 1
        elif node.data == "or_gate":
            _maxv = max([self._max_steps(c) for c in node.children[1:]]) if node.children[1:] else 0
        elif node.data == "and_gate":
            _maxv = sum([self._max_steps(c) for c in node.children[1:]])
        elif node.data == "vot_gate":
            _need = int(node.children[1])
            _vals = sorted([self._max_steps(c) for c in node.children[3:]], reverse=True)
            _maxv = sum(_vals[:_need]) if _need > 0 else 0
        elif node.data == "pand_gate":
            _maxv = self._max_steps(node.children[1]) + self._max_steps(node.children[2])
        elif node.data == "spare_gate":
            _maxv = sum([self._max_steps(c) for c in node.children[1:]])
        else:
            raise Exception("Split formula unsupported for node type {}".format(node.data))
        self.max_cache[id(node)] = int(_maxv)
        return self.max_cache[id(node)]

    def _leaf_info(self, node):
        _name = str(node.children[0])
        _base = {"name": _name, "kind": node.data, "fail": 0.0, "dorm": 0.0, "active": {}}
        for spec in node.children[1:]:
            if not isinstance(spec, Tree):
                continue
            if spec.data == "fail":
                _base["fail"] = _dist_tree_proxy(spec.children[0])
            elif spec.data == "dorm":
                _base["dorm"] = _dist_tree_proxy(spec.children[0])
        _phases = max(int(self.compiler.num_phases), 1)
        _ovr = self.compiler.phase_overrides.get(_name, {})
        _active_map = None
        if self.compiler.has_active_lists:
            _active_map = {p: (_name in self.compiler.phase_actives.get(p, set())) for p in range(1, _phases + 1)}
        for p in range(1, _phases + 1):
            _fail = _base["fail"]
            if p in _ovr.get("fail", {}):
                _fail = _dist_str_proxy(_ovr["fail"][p])
            if node.data == "sbe":
                _rate = _base["dorm"]
                if p in _ovr.get("dorm", {}):
                    _rate = _dist_str_proxy(_ovr["dorm"][p])
                if _active_map is None or _active_map.get(p, True):
                    _rate = _fail
            else:
                _rate = _fail if (_active_map is None or _active_map.get(p, True)) else 0.0
            _base["active"][p] = float(max(_rate, 0.0))
        _base["inherit"] = self.compiler.inherit_map.get(_name)
        return _base

    def _leaf_integral(self, info):
        _phases = max(int(self.compiler.num_phases), 1)
        if _phases <= 1:
            return self._prod_expr([self._fmt(info["active"].get(1, info.get("fail", 0.0))), "clkTO"])
        _terms = []
        _fixed_tail = max(self.transient_horizon - sum(self.compiler.phase_durations), 0.0)
        for p in range(1, _phases + 1):
            _parts = []
            _rate = info["active"].get(p, 0.0)
            if 0.0 < _rate:
                _rem = "clkTO" if p == _phases else "PHASE_clk"
                _parts.append(self._prod_expr([self._fmt(_rate), _rem]))
            for j in range(p + 1, _phases + 1):
                _future_rate = info["active"].get(j, 0.0)
                if _future_rate <= 0.0:
                    continue
                if j == _phases:
                    _dur = _fixed_tail
                else:
                    _dur = self.compiler.phase_durations[j - 1]
                if _dur <= 0.0:
                    continue
                _parts.append(self._fmt(_future_rate * _dur))
            if not _parts:
                continue
            _terms.append(self._prod_expr(["(PHASE_phase=={})".format(p), self._sum_expr(_parts)]))
        return self._sum_expr(_terms)

    def _leaf_future_fail(self, name):
        info = self.leaves[name]
        _raw = "(1-exp(-({})))".format(self._leaf_integral(info))
        _broken = "({}_broken>0)".format(name)
        _expr = "(({})+(({}_broken==0)*({})))".format(_broken, name, _raw)
        _inherit = info.get("inherit")
        if _inherit is not None and self.compiler.num_phases > 1:
            _pred, _phase = _inherit
            _guard = "(({}_pred_failed)*(PHASE_phase<{}))".format(name, int(_phase))
            _expr = "(({})+((1-({}))*({})))".format(_guard, _guard, _expr)
        return _expr

    def _fail_prob_expr(self, node):
        try:
            return self.fail_prob_cache[id(node)]
        except:
            pass
        if node.data == "toplevel":
            _expr = self._fail_prob_expr(node.children[0])
        elif node.data in {"be", "sbe"}:
            _expr = self._leaf_future_fail(self._node_name(node))
        elif node.data == "or_gate":
            _failed = self._node_failed_expr(node)
            _children = [self._fail_prob_expr(c) for c in node.children[1:]]
            _surv = ["(1-({}))".format(c) for c in _children]
            _raw = "(1-{})".format(self._prod_expr(_surv))
            _expr = self._if_expr(_failed, "1", _raw)
        elif node.data == "and_gate":
            _failed = self._node_failed_expr(node)
            _parts = []
            for c in node.children[1:]:
                _c_failed = self._node_failed_expr(c)
                _parts.append("(({})+((1-({}))*({})))".format(_c_failed, _c_failed, self._fail_prob_expr(c)))
            _raw = self._prod_expr(_parts)
            _expr = self._if_expr(_failed, "1", _raw)
        elif node.data == "vot_gate":
            _name = self._node_name(node)
            _need = int(node.children[1])
            _failed = self._node_failed_expr(node)
            _children = node.children[3:]
            _cases = [self._prod_expr([_failed, "1"])]
            for _r in range(1, _need + 1):
                _count_val = _need - _r
                _combo_terms = []
                for combo in combinations(_children, _r):
                    _combo_prob = []
                    for c in combo:
                        _c_failed = self._node_failed_expr(c)
                        _combo_prob.append(self._prod_expr(["(1-({}))".format(_c_failed), self._fail_prob_expr(c)]))
                    _combo_terms.append(self._prod_expr(_combo_prob))
                _raw = "(1-{})".format(self._prod_expr(["(1-({}))".format(x) for x in _combo_terms])) if _combo_terms else "0"
                _cases.append(self._prod_expr(["({}_count=={})".format(_name, _count_val), _raw]))
            _expr = self._sum_expr(_cases)
        elif node.data == "pand_gate":
            _name = self._node_name(node)
            _left = self._fail_prob_expr(node.children[1])
            _right = self._fail_prob_expr(node.children[2])
            _failed = self._node_failed_expr(node)
            _armed = self._prod_expr(["(({}_st==3)+(({}_st==0)*({}_f0)*((1-({}_f1)))))".format(_name, _name, _name, _name), _right])
            _fresh = self._prod_expr(["(({}_st==0)*((1-({}_f0))*((1-({}_f1)))))".format(_name, _name, _name), _left, _right, "pow((1-({})),{})".format(_right, self._fmt(self.tuning.pand_eta))])
            _expr = self._sum_expr([_failed, _armed, _fresh])
        elif node.data == "spare_gate":
            _failed = self._node_failed_expr(node)
            _parts = []
            for c in node.children[1:]:
                _c_failed = self._node_failed_expr(c)
                _parts.append("(({})+((1-({}))*({})))".format(_c_failed, _c_failed, self._fail_prob_expr(c)))
            _raw = self._prod_expr(_parts)
            _expr = self._if_expr(_failed, "1", _raw)
        else:
            raise Exception("Split formula unsupported for node type {}".format(node.data))
        self.fail_prob_cache[id(node)] = _expr
        return _expr

    def _dist_expr(self, node):
        try:
            return self.dist_cache[id(node)]
        except:
            pass
        _inf = str(self.inf_value)
        if node.data == "toplevel":
            _expr = self._dist_expr(node.children[0])
        elif node.data in {"be", "sbe"}:
            _name = self._node_name(node)
            _p = self._leaf_future_fail(_name)
            _expr = self._sum_expr([
                self._prod_expr(["({}_broken==0)".format(_name), "({}>0)".format(_p)]),
                self._prod_expr(["({}_broken==0)".format(_name), "({}<=0)".format(_p), _inf]),
            ])
        elif node.data == "or_gate":
            _failed = self._node_failed_expr(node)
            _raw = self._min_expr([self._dist_expr(c) for c in node.children[1:]], default=_inf)
            _expr = self._if_expr(_failed, "0", _raw)
        elif node.data == "and_gate":
            _failed = self._node_failed_expr(node)
            _parts = []
            for c in node.children[1:]:
                _parts.append(self._prod_expr(["(1-({}))".format(self._node_failed_expr(c)), self._dist_expr(c)]))
            _raw = self._sum_expr(_parts)
            _expr = self._if_expr(_failed, "0", _raw)
        elif node.data == "vot_gate":
            _name = self._node_name(node)
            _failed = self._node_failed_expr(node)
            _need = int(node.children[1])
            _children = node.children[3:]
            _cases = [self._prod_expr([_failed, "0"])]
            for _r in range(1, _need + 1):
                _count_val = _need - _r
                _combo_terms = []
                for combo in combinations(_children, _r):
                    _valid = self._prod_expr(["(1-({}))".format(self._node_failed_expr(c)) for c in combo])
                    _dist = self._sum_expr([self._dist_expr(c) for c in combo])
                    _combo_terms.append(self._sum_expr([
                        self._prod_expr([_valid, _dist]),
                        self._prod_expr(["(1-({}))".format(_valid), _inf]),
                    ]))
                _raw = self._min_expr(_combo_terms, default=_inf)
                _cases.append(self._prod_expr(["({}_count=={})".format(_name, _count_val), _raw]))
            _expr = self._sum_expr(_cases)
        elif node.data == "pand_gate":
            _name = self._node_name(node)
            _failed = self._node_failed_expr(node)
            _unreach = "(({}_st==4)+(({}_st==0)*((1-({}_f0)))*({}_f1)))".format(_name, _name, _name, _name)
            _wait_right = self._sum_expr([
                self._prod_expr(["({}_st==3)".format(_name), self._dist_expr(node.children[2])]),
                self._prod_expr(["({}_st==0)".format(_name), "({}_f0)".format(_name), "(1-({}_f1))".format(_name), self._dist_expr(node.children[2])]),
            ])
            _fresh = self._prod_expr(["({}_st==0)".format(_name), "(1-({}_f0))".format(_name), "(1-({}_f1))".format(_name), self._sum_expr([self._dist_expr(node.children[1]), self._dist_expr(node.children[2])])])
            _expr = self._sum_expr([
                self._prod_expr([_failed, "0"]),
                self._prod_expr(["(1-({}))".format(_failed), _unreach, _inf]),
                self._prod_expr(["(1-({}))".format(_failed), "(1-({}))".format(_unreach), self._sum_expr([_wait_right, _fresh])]),
            ])
        elif node.data == "spare_gate":
            _failed = self._node_failed_expr(node)
            _parts = []
            for c in node.children[1:]:
                _parts.append(self._prod_expr(["(1-({}))".format(self._node_failed_expr(c)), self._dist_expr(c)]))
            _raw = self._sum_expr(_parts)
            _expr = self._if_expr(_failed, "0", _raw)
        else:
            raise Exception("Split formula unsupported for node type {}".format(node.data))
        self.dist_cache[id(node)] = _expr
        return _expr

    def _gate_score_expr(self, node):
        if node.data in {"be", "sbe"}:
            return self._leaf_future_fail(self._node_name(node))
        if node.data == "or_gate":
            _child_d = [self._dist_expr(c) for c in node.children[1:]]
            _best = self._min_expr(_child_d, default=str(self.inf_value))
            _terms = []
            for c, _d in zip(node.children[1:], _child_d):
                _terms.append(self._prod_expr(["(({})==({}))".format(_d, _best), self._fail_prob_expr(c)]))
            return "(1-{})".format(self._prod_expr(["(1-({}))".format(x) for x in _terms])) if _terms else "0"
        if node.data == "vot_gate":
            _name = self._node_name(node)
            _need = int(node.children[1])
            _children = node.children[3:]
            _cases = []
            for _r in range(1, _need + 1):
                _count_val = _need - _r
                _combo_terms = []
                for combo in combinations(_children, _r):
                    _valid = self._prod_expr(["(1-({}))".format(self._node_failed_expr(c)) for c in combo])
                    _prob = self._prod_expr([self._fail_prob_expr(c) for c in combo])
                    _combo_terms.append(self._prod_expr([_valid, _prob]))
                _raw = "(1-{})".format(self._prod_expr(["(1-({}))".format(x) for x in _combo_terms])) if _combo_terms else "0"
                _cases.append(self._prod_expr(["({}_count=={})".format(_name, _count_val), _raw]))
            return self._sum_expr(_cases)
        if node.data == "pand_gate":
            _name = self._node_name(node)
            _left = self._fail_prob_expr(node.children[1])
            _right = self._fail_prob_expr(node.children[2])
            return self._sum_expr([
                self._prod_expr(["({}_st==3)".format(_name), _right]),
                self._prod_expr(["({}_st==0)".format(_name), "({}_f0)".format(_name), "(1-({}_f1))".format(_name), _right]),
                self._prod_expr(["({}_st==0)".format(_name), "(1-({}_f0))".format(_name), "(1-({}_f1))".format(_name), _left, "pow((1-({})),{})".format(_right, self._fmt(self.tuning.pand_eta))]),
            ])
        if node.data == "spare_gate":
            return self._fail_prob_expr(node)
        raise Exception("Split formula unsupported for node type {}".format(node.data))

    def _frontier_stats(self, node):
        try:
            return self.frontier_cache[id(node)]
        except:
            pass
        if node.data == "toplevel":
            _stats = self._frontier_stats(node.children[0])
        elif node.data in {"be", "sbe"}:
            _dist = self._dist_expr(node)
            _active = self._prod_expr(["(({})>0)".format(_dist), "(({})<{})".format(_dist, self.inf_value)])
            _stats = (self._prod_expr([_active, self._gate_score_expr(node)]), _active)
        elif node.data == "and_gate":
            _failed = self._node_failed_expr(node)
            _gate_active = self._prod_expr(["(1-({}))".format(_failed), "(({})<{})".format(self._dist_expr(node), self.inf_value)])
            _sum_terms = []
            _count_terms = []
            for c in node.children[1:]:
                _child_sum, _child_count = self._frontier_stats(c)
                _unsat = "(1-({}))".format(self._node_failed_expr(c))
                _sum_terms.append(self._prod_expr([_unsat, _child_sum]))
                _count_terms.append(self._prod_expr([_unsat, _child_count]))
            _stats = (self._prod_expr([_gate_active, self._sum_expr(_sum_terms)]), self._prod_expr([_gate_active, self._sum_expr(_count_terms)]))
        else:
            _failed = self._node_failed_expr(node)
            _active = self._prod_expr(["(1-({}))".format(_failed), "(({})<{})".format(self._dist_expr(node), self.inf_value)])
            _stats = (self._prod_expr([_active, self._gate_score_expr(node)]), _active)
        self.frontier_cache[id(node)] = _stats
        return _stats

    def _its_expr(self):
        _root_failed = self._node_failed_expr(self.root)
        _dist = self._dist_expr(self.root)
        return self._sum_expr([
            self._prod_expr([_root_failed, str(self.root_max + 1)]),
            self._prod_expr(["(1-({}))".format(_root_failed), "(({})<{})".format(_dist, self.inf_value), "({}-({})+1)".format(self.root_max, _dist)]),
        ])

    def _frontier_layer_expr(self):
        _sum_scores, _count_active = self._frontier_stats(self.root)
        _k = max(int(self.tuning.phys_k), 2)
        _has_frontier = "(({})>0)".format(_count_active)
        _micro = "floor(({}*({}))/max(1,{}))".format(_k - 1, _sum_scores, _count_active)
        return self._sum_expr([
            self._prod_expr([_has_frontier, "1"]),
            self._prod_expr([_has_frontier, _micro]),
        ])

    def _frontier_mean_expr(self):
        _sum_scores, _count_active = self._frontier_stats(self.root)
        return "min(1,max(0,({})/max(1,{})))".format(_sum_scores, _count_active)

    def time_formula(self):
        return self._its_expr(), 0, self.root_max + 1

    def phys_formula(self):
        return self._frontier_layer_expr(), 0, max(int(self.tuning.phys_k), 2)

    def timephys_formula(self):
        _k = max(int(self.tuning.phys_k), 2)
        _its = self._its_expr()
        _frontier_layer = self._frontier_layer_expr()
        _its_progress = self._prod_expr(["(({})>1)".format(_its), "(({})-1)".format(_its), str(_k)])
        return self._sum_expr([_its_progress, _frontier_layer]), 0, (self.root_max + 1) * _k

    def split_formula(self, m_max):
        _mmax = int(m_max)
        if _mmax <= 1:
            return ""
        _its = self._its_expr()
        _frontier_mean = self._frontier_mean_expr()
        _root_max = max(1, self.root_max)
        _phys_tilt = "pow({}, {})".format(_frontier_mean, self._fmt(self.tuning.phys_gamma))
        return "min({}, 1+floor(({})/{}))".format(
            _mmax,
            self._prod_expr([str(_mmax - 1), _its, _phys_tilt]),
            _root_max,
        )


# FIXME NODE names are expected to be all different in the input 
# (can't have and gate and a be with same name for instance)

# FIXME there is a library called simply which allows to simplify 
# arithmetic formulas with variables.
# Maybe we can use it to get simple ifuns? will it be useful? 

# FIXME we parse two times because I have not been able to
# produce a proper deep copy of a parsed Tree and thus the
# compiler and IF generator corrupt the original parsed Tree
# If we want to avoid this we may want to either find a way
# to deep copy the tree or we should make "pure" functions
# that visit the tree and generate the IF and Iosa modules

class Kepler2IOSA():
    
    def __init__(self, phys_k=10, phys_beta=1.0, phys_gamma=1.0, pand_eta=1.0, transient_horizon=0.0,
                 pwstr_k=1000, pwstr_beta_req=0.5, pwstr_beta_alt=0.5,
                 pwstr_a_min=0.5, pwstr_a_max=2.0, pwstr_b_min=0.5, pwstr_b_max=2.0,
                 t3_phys_k=10, t3_eta=1.0, t3_k=1000,
                 rdgt_alpha_min=None, rdgt_rare_low=None, rdgt_rare_high=None,
                 rdgt_hetero_low=None, rdgt_hetero_high=None,
                 rdgt_bottleneck_low=None, rdgt_bottleneck_high=None,
                 rdgt_inf_mult=None, rdgt_fixed_alpha_flag=None, rdgt_fixed_alpha_value=None):
        self.tuning = HeuristicTuning(phys_k=phys_k, phys_beta=phys_beta, phys_gamma=phys_gamma, pand_eta=pand_eta)
        self.transient_horizon = max(float(transient_horizon or 0.0), 0.0)
        self.pwstr_tuning = PwstrTuning(k=pwstr_k, beta_req=pwstr_beta_req, beta_alt=pwstr_beta_alt,
                                         a_min=pwstr_a_min, a_max=pwstr_a_max,
                                         b_min=pwstr_b_min, b_max=pwstr_b_max)
        self.t3_tuning = T3Tuning(phys_k=t3_phys_k, eta=t3_eta, k=t3_k)
        self.rdgt_params = RDGTParams(
            alpha_min=rdgt_alpha_min,
            rare_low=rdgt_rare_low, rare_high=rdgt_rare_high,
            hetero_low=rdgt_hetero_low, hetero_high=rdgt_hetero_high,
            bottleneck_low=rdgt_bottleneck_low, bottleneck_high=rdgt_bottleneck_high,
            inf_mult=rdgt_inf_mult,
            fixed_alpha_flag=rdgt_fixed_alpha_flag,
            fixed_alpha_value=rdgt_fixed_alpha_value,
            read_env=True)

    def compile(self, input):
        """
        @param <input>: a string with a kepler/galileo valid DFT description
        @returns: a string with a IOSA model of <input>
        """
        tree = kepler_parser.parse(input)
        # tree = Rename(tree).visit(tree)
        tree = TransformFDEP(tree).visit(tree)
        compiler = CompilePass(tree)
        iosa = compiler.transform(tree)
        model = '\n\n'.join([str(x) for x in iosa.children])
        ph = compiler.api.make_phase_controller()
        if ph:
            return ph + "\n\n" + model
        return model

    def _prepare_ifun_tree(self, input):
        tree = kepler_parser.parse(input)
        tree = TransformFDEP(tree).visit(tree)
        compiler = CompilePass(tree)
        ktree = Kepler2Tree(tree).visit(tree)
        return tree, compiler, ktree
        
    def ifun_structural(self, input):
        """ 
        @param <input>: a string with a kepler/galileo valid DFT description
        """
        tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        ifun = Tree2StructuralIfun(active_guard=active_guard).visit(tree)
        return ifun.ifun, 0, ifun.maxi
        
    def ifun_mcs(self, input):
        tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        ifun = Tree2MCSIfun(active_guard=active_guard).ifun(tree)
        # return ifun.ifun, 0, ifun.maxi
        return ifun

    def ifun_mcsn(self, input):
        tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        ifun = Tree2MCSIfun(active_guard=active_guard).ifun_n(tree)
        # return ifun.ifun, 0, ifun.maxi
        return ifun

    def _ifun_heuristic(self, input, mode):
        raw_tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        physical_scores = IfunHeuristicContext(compiler, raw_tree, tuning=self.tuning).physical_scores()
        ifun = Tree2HeuristicIfun(mode, num_phases=compiler.num_phases, active_guard=active_guard, physical_scores=physical_scores, tuning=self.tuning).visit(tree)
        expr, maxi = ifun.ifun, ifun.maxi
        if mode in {"TIME", "TIMEPHYS"} and compiler.num_phases > 1:
            expr = "({}+PHASE_phase)".format(expr)
            maxi = int(maxi) + int(compiler.num_phases)
        return expr, 0, maxi

    def ifun_time(self, input):
        return self._ifun_heuristic(input, "TIME")

    def ifun_phys(self, input):
        return self._ifun_heuristic(input, "PHYS")

    def ifun_timephys(self, input):
        return self._ifun_heuristic(input, "TIMEPHYS")

    def ifun_pwstr(self, input):
        raw_tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        pwstr_ctx = PwstrContext(compiler, raw_tree, self.transient_horizon, tuning=self.pwstr_tuning)
        ifun_visitor = Tree2PWSTRIfun(active_guard=active_guard, pwstr_context=pwstr_ctx, tuning=self.pwstr_tuning)
        ifun_visitor.visit(tree)
        return tree.ifun, 0, tree.maxi

    def ifun_t3(self, input):
        raw_tree, compiler, tree = self._prepare_ifun_tree(input)
        active_guard = bool(compiler.num_phases > 1 and compiler.has_active_lists)
        t3_ctx = T3Context(compiler, raw_tree, self.transient_horizon, tuning=self.t3_tuning)
        ifun_visitor = Tree2T3Ifun(active_guard=active_guard, t3_context=t3_ctx, tuning=self.t3_tuning)
        ifun_visitor.visit(tree)
        return tree.ifun, 0, tree.maxi

    def ifun_m3p(self, input):
        """
        M3+ / M3P: structural-state backbone plus contribution-aware physical
        refinement.

        The original PHYS/M3 IFUN uses physical leaf weights as the main
        progress score.  On shallow static trees this can let raw component
        rates override the structural distance-to-top-event information.  M3P
        keeps a runtime structural progress term as the high-order component
        and uses the T3 approximate top-event probability only as a low-order
        tie-break/refinement term:

            I_M3P = I_state_struct * (K + 1) + I_T3

        where I_state_struct is built from the same gate semantics as PHYS but
        with unit leaf weights, and I_T3 is the existing contribution-aware
        probability/pushability score in [0, K].  This preserves M1-like
        structural ordering while improving within-layer guidance in dynamic or
        heterogeneous cases.
        """
        raw_tree_s, compiler_s, state_tree = self._prepare_ifun_tree(input)
        active_guard_s = bool(compiler_s.num_phases > 1 and compiler_s.has_active_lists)
        state_ifun = Tree2HeuristicIfun(
            "STATE",
            num_phases=compiler_s.num_phases,
            active_guard=active_guard_s,
            physical_scores={},
            tuning=self.tuning,
        ).visit(state_tree)

        raw_tree_t, compiler_t, t3_tree = self._prepare_ifun_tree(input)
        active_guard_t = bool(compiler_t.num_phases > 1 and compiler_t.has_active_lists)
        t3_ctx = T3Context(compiler_t, raw_tree_t, self.transient_horizon, tuning=self.t3_tuning)
        t3_visitor = Tree2T3Ifun(active_guard=active_guard_t, t3_context=t3_ctx, tuning=self.t3_tuning)
        t3_visitor.visit(t3_tree)

        stride = int(self.t3_tuning.k) + 1
        expr = "(({})*({})+({}))".format(state_ifun.ifun, stride, t3_tree.ifun)
        maxi = int(state_ifun.maxi) * stride + int(t3_tree.maxi)
        return expr, 0, maxi

    def _rdgt_preset_params(self):
        return {
            "alpha_min": float(os.environ.get("N4A_ALPHA_MIN", "0.1")),
            "rare_low": float(os.environ.get("N4A_RARE_LOW", "3")),
            "rare_high": float(os.environ.get("N4A_RARE_HIGH", "8")),
            "hetero_low": float(os.environ.get("N4A_HETERO_LOW", "3")),
            "hetero_high": float(os.environ.get("N4A_HETERO_HIGH", "30")),
            "bottleneck_low": float(os.environ.get("N4A_BOTTLENECK_LOW", "0.40")),
            "bottleneck_high": float(os.environ.get("N4A_BOTTLENECK_HIGH", "0.70")),
            "inf_mult": float(os.environ.get("N4A_INF_MULT", os.environ.get("N5C_INF_MULT", "10"))),
            "fixed_alpha_flag": float(os.environ.get("N4A_FIXED_ALPHA_FLAG", "0")),
            "fixed_alpha_value": float(os.environ.get("N4A_FIXED_ALPHA_VALUE", "0")),
        }

    def ifun_n4(self, input):
        """N4 / RD-IF: probability-semantics log-difficulty IFUN."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N4_K", "500")))
        scale = max(10, int(os.environ.get("N4_SCALE", "1000")))
        d_scale = float(os.environ.get("N4_D_SCALE", "100"))
        ctx = IfunN4Context(
            compiler, raw_tree, state_tree,
            mission_time=self.transient_horizon,
            scale=scale, d_scale=d_scale,
            inf_mult=10.0,
            tuning=self.tuning)
        expr, expr_max = ctx.progress_expr()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def ifun_n4p(self, input):
        """N4P: frozen N4 with explicitly unreachable blocked PAND."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N4_K", "500")))
        scale = max(10, int(os.environ.get("N4_SCALE", "1000")))
        d_scale = float(os.environ.get("N4_D_SCALE", "100"))
        ctx = IfunN4pContext(
            compiler, raw_tree, state_tree,
            mission_time=self.transient_horizon,
            scale=scale, d_scale=d_scale,
            tuning=self.tuning)
        expr, expr_max = ctx.progress_expr()
        self._last_n4p_diagnostic = ctx.diagnostic_comment()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def n4p_diagnostic(self):
        return getattr(self, "_last_n4p_diagnostic", "")

    def ifun_n4q(self, input):
        """N4Q-v1: conditional top-event reachability approximation."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N4_K", "500")))
        scale = max(10, int(os.environ.get("N4_SCALE", "1000")))
        d_scale = float(os.environ.get("N4_D_SCALE", "100"))
        ctx = IfunN4qContext(
            compiler, raw_tree, state_tree,
            mission_time=self.transient_horizon,
            scale=scale, d_scale=d_scale,
            tuning=self.tuning)
        expr, expr_max = ctx.progress_expr()
        self._last_n4q_diagnostic = ctx.diagnostic_comment()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def n4q_diagnostic(self):
        return getattr(self, "_last_n4q_diagnostic", "")

    def ifun_n4a(self, input):
        """Pure N4A adaptive physical temperature IFUN — historical frozen semantics."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N4_K", "500")))
        scale = max(10, int(os.environ.get("N4_SCALE", "1000")))
        d_scale = float(os.environ.get("N4_D_SCALE", "100"))
        alpha_min = float(os.environ.get("N4A_ALPHA_MIN", "0.1"))
        mission_time = float(os.environ.get("N4_MISSION_TIME", str(self.transient_horizon)))
        ctx = IfunN4aContext(
            compiler, raw_tree, state_tree,
            mission_time=mission_time,
            scale=scale, d_scale=d_scale,
            alpha_min=alpha_min,
            inf_mult=10.0,
            tuning=self.tuning)
        self._last_n4a_diagnostic = ctx.diagnostic_comment()
        expr, expr_max = ctx.progress_expr()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def ifun_ardiv2(self, input):
        '''ARDI-v2 inverse low-temperature IFUN.'''
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get('N4_K', '500')))
        scale = max(10, int(os.environ.get('N4_SCALE', '1000')))
        d_scale = float(os.environ.get('N4_D_SCALE', '100'))
        mission_time = float(os.environ.get(
            'N4_MISSION_TIME', str(self.transient_horizon)))
        ctx = IfunArdiV2Context(
            compiler, raw_tree, state_tree,
            mission_time=mission_time, scale=scale, d_scale=d_scale,
            alpha_min=float(os.environ.get('ARDI_ALPHA_MIN', '0.10')),
            alpha_max=float(os.environ.get('ARDI_ALPHA_MAX', '0.25')),
            alpha_law=os.environ.get(
                'ARDI_ALPHA_LAW', 'inverse_low_temp'),
            fixed_alpha_flag=float(os.environ.get(
                'ARDI_FIXED_ALPHA_FLAG', '0')),
            fixed_alpha_value=float(os.environ.get(
                'ARDI_FIXED_ALPHA_VALUE', '0.10')),
            tuning=self.tuning,
        )
        self._last_ardiv2_diagnostic = ctx.diagnostic_comment()
        expr, expr_max = ctx.progress_expr()
        out = '(1+floor((({})*{})/{}))'.format(
            expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def ardiv2_diagnostic(self):
        return getattr(self, '_last_ardiv2_diagnostic', '')

    def ifun_n4an(self, input):
        """N4AN: normalized/parameterized adaptive physical temperature IFUN."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N4_K", "500")))
        scale = max(10, int(os.environ.get("N4_SCALE", "1000")))
        d_scale = float(os.environ.get("N4_D_SCALE", "100"))
        mission_time = float(os.environ.get("N4_MISSION_TIME", str(self.transient_horizon)))
        ctx = IfunN4anContext(
            compiler, raw_tree, state_tree,
            mission_time=mission_time, scale=scale,
            d_scale=d_scale, alpha_min=self.rdgt_params.alpha_min,
            tuning=self.tuning, rdgt_params=self.rdgt_params,
        )
        self._last_n4an_diagnostic = ctx.diagnostic_comment()
        expr, expr_max = ctx.progress_expr()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def n4a_diagnostic(self):
        return getattr(self, "_last_n4a_diagnostic", "")

    def n4an_diagnostic(self):
        return getattr(self, "_last_n4an_diagnostic", "")

    def ifun_n5c(self, input):
        """N5C / CC-RD-GT: recursive committor-corrected RD-GT."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N5C_K", os.environ.get("N4_K", "500"))))
        scale = max(10, int(os.environ.get("N5C_SCALE", os.environ.get("N4_SCALE", "1000"))))
        d_scale = float(os.environ.get("N5C_D_SCALE", os.environ.get("N4_D_SCALE", "100")))
        mission_time = float(os.environ.get("N5C_MISSION_TIME", os.environ.get("N4_MISSION_TIME", str(self.transient_horizon))))
        order_factor = str(os.environ.get("N5C_ORDER_FACTOR", "0")).lower() in {"1", "true", "yes", "on"}
        shared_guard = str(os.environ.get("N5C_SHARED_GUARD", "0")).lower() in {"1", "true", "yes", "on"}
        preset_params = self._rdgt_preset_params()
        preset_params["inf_mult"] = float(os.environ.get("N5C_INF_MULT", preset_params["inf_mult"]))
        ctx = IfunN5cContext(
            compiler, raw_tree, state_tree,
            mission_time=mission_time,
            scale=scale, d_scale=d_scale,
            eps_prob=float(os.environ.get("N5C_EPS_PROB", "1e-38")),
            pand_order_factor=order_factor,
            shared_guard=shared_guard,
            fallback_kind=os.environ.get("N5C_FALLBACK", "n4a"),
            tuning=self.tuning,
            **preset_params)
        expr, expr_max = ctx.progress_expr()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def ifun_n5p(self, input):
        """N5P-ATPE: N4A anchored tempered path-ensemble RD-GT."""
        raw_tree, compiler, state_tree = self._prepare_ifun_tree(input)
        K = max(10, int(os.environ.get("N5P_K", os.environ.get("N4_K", "500"))))
        scale = max(10, int(os.environ.get("N5P_SCALE", os.environ.get("N4_SCALE", "1000"))))
        d_scale = float(os.environ.get("N5P_D_SCALE", os.environ.get("N4_D_SCALE", "100")))
        mission_time = float(os.environ.get("N5P_MISSION_TIME", os.environ.get("N4_MISSION_TIME", str(self.transient_horizon))))
        preset_params = self._rdgt_preset_params()
        preset_params["inf_mult"] = float(os.environ.get("N5P_INF_MULT", preset_params["inf_mult"]))
        ctx = IfunN5pAtpeContext(
            compiler, raw_tree, state_tree,
            mission_time=mission_time,
            scale=scale,
            d_scale=d_scale,
            L=int(os.environ.get("N5P_L", "64")),
            tau=float(os.environ.get("N5P_TAU", "0.5")),
            W=float(os.environ.get("N5P_W", "6.0")),
            tau_w=float(os.environ.get("N5P_TAU_W", "1.0")),
            eta=float(os.environ.get("N5P_ETA", "0.5")),
            B_cap=float(os.environ.get("N5P_B_CAP", "0.05")),
            N_eff_full=float(os.environ.get("N5P_N_EFF_FULL", "4.0")),
            tuning=self.tuning,
            **preset_params)
        self.last_n5p_diagnostic = ctx.diagnostic_comment()
        expr, expr_max = ctx.progress_expr()
        out = "(1+floor((({})*{})/{}))".format(expr, K, max(expr_max, 1))
        return out, 1, K + 1

    def n5p_diagnostic(self):
        return getattr(self, "last_n5p_diagnostic", "")

    def yu_sbdd_report(self, input, horizon=None, max_css=2000, state_limit=20000):
        from yu_sbdd import YuHaiyueAnalyzer
        if horizon is None:
            horizon = self.transient_horizon if self.transient_horizon > 0 else None
        analyzer = YuHaiyueAnalyzer(
            horizon=horizon,
            max_css=max_css,
            state_limit=state_limit,
        )
        return analyzer.analyze_text(input)

    def split_timephys(self, input, m_max):
        raw_tree, compiler, tree = self._prepare_ifun_tree(input)
        try:
            return SplitFormulaBuilder(compiler, raw_tree, tree, transient_horizon=self.transient_horizon, tuning=self.tuning).split_formula(m_max)
        except:
            return ""

    def split_phys(self, input, m_max):
        return self.split_timephys(input, m_max)
        
    def top_level_event(self, input):
        """ Return the characteristic state formula for the top
        level event module the top level gate.
        @param <input>: a string with a kepler/galileo valid DFT description
        """
        tree = kepler_parser.parse(input)
        tl_node = next(tree.find_data("toplevel"))
        tl_gate_name = tl_node.children[0]
        tl_gate = next(tree.find_pred(lambda t: t.children[0] == tl_gate_name))
        tl_type = tl_gate.data
        tl_prefix = type_to_prefix(tl_type)
        n = get_n_inputs(tl_gate)
        k = tl_gate.children[1] if tl_type == "vot_gate" else 0
        return IosaApi().fail_condition(tl_gate_name, tl_type, n, k)
        
      
        
if __name__ == "__main__":
    
    kepler = '''toplevel "AND1"; 
    "AND1" and "B1" "OR1"; 
    "B1" fail~exponential(1.2); 
    "OR1" or "B2" "PAND1"; 
    "B2" fail~exponential(2.2); 
    "PAND1" pand "B3" "V1"; 
    "B3" fail~exponential(3.2);
    "V1" 3 of 4 "B4" "B5" "B6" "SP1";
    "B4" fail~exponential(4.2);
    "B5" fail~exponential(5.2);
    "B6" fail~exponential(6.2);
    "SP1" spare "B7" "SBE1" "SBE2";
    "B7" fail~exponential(7.2);
    "SBE1" dorm~exponential(7.2) fail~exponential(7.2);
    "SBE2" dorm~exponential(7.2) fail~exponential(7.2);
    '''

    parsed = kepler_parser.parse(kepler)
    print(parsed.pretty())
    
    print(";".join([str(x) for x in Kepler2IOSA().ifun_structural(kepler)]))
    
    hecs = '''
    toplevel "HECS";
    "HECS" or "IF" "MEM" "BUS" "PROC";

    "IF" or "SW" "HW";
    "SW"  fail~exponential(1.2);
    "HW"  fail~exponential(1.2);

    "MEM" 3of5 "M1" "M2" "M3" "M4" "M5";

    "MI3" and "MI1" "MI2";
    "MI2" fail~exponential(1.2);
    "MI1" fail~exponential(1.2);
    "M1"  fail~exponential(1.2);
    "M2"  fail~exponential(1.2);
    "M3"  fail~exponential(1.2);
    "M4"  fail~exponential(1.2);
    "M5"  fail~exponential(1.2);

    "BUS" and "B1" "B2" "B3" "B4" "B5" "B6" "B7" "B8";
    "B1"  fail~exponential(1.2);
    "B2"  fail~exponential(1.2);
    "B3"  fail~exponential(1.2);
    "B4"  fail~exponential(1.2);
    "B5"  fail~exponential(1.2);
    "B6"  fail~exponential(1.2);
    "B7"  fail~exponential(1.2); //sfasfas
    "B8"  fail~exponential(1.2);
    
    /* sfasfas */

    "PROC" and "PROC1" "PROC2";
    "PROC1" spare "P1" "PS1" "PS2" "PS3" "PS4";
    "PROC2" spare "P2" "PS1" "PS2" "PS3" "PS4";
    "P1"  fail~exponential(1.2);
    "P2"  fail~exponential(1.2);
    "PS1" dorm~exponential(1.1) fail~exponential(1.2);
    "PS2" dorm~exponential(1.1) fail~exponential(1.2);
    "PS3" dorm~exponential(1.1) fail~exponential(1.2);
    "PS4" dorm~exponential(1.1) fail~exponential(1.2);
    '''
    
    print(";".join([str(x) for x in Kepler2IOSA().ifun_structural(hecs)]))

    
    # TODO s:
    # [DONE] minimal cut sets imp fun (Automated Rare Event Simulation for Fault Tree Analysis via Minimal Cut Sets)
    #   Transformar fdep en or y spare en and. Pand prohibido. DNF de la funcion logica que queda. Cada clausula es un MCS. 
    #   Table 1: I_MCS and I_MCS-N
    # [DONE] la función me tendría que devolver la condición que hace que el nodo emita una falla, o sea "<variable_name>=<value>"
    #   entonces, por ejemplo, puedo hacer la siguente llamada desde el kompilator:
    #   property = "S(" + (Kepler2IOSA().toplevelevent(FT)) + ")"

    # [0] Interchangable dorm fail and repair rates for SBE and BE
    # [1] Check that the SBE of the new compilation are eq to that of the old compilation (MARCO's)
    
    # use frsteq for array in iosa translation
    # remove prefix for module variables
          
