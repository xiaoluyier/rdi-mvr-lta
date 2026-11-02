#!/usr/bin/env python
#
# IOSA elements templates
#
# Author:  Raul E. Monti
# Date:    15.10.2021
# License: GPLv3
#

import re

BE_REPAIRABLE=r"""
/* A BE named {name} */
module BE_{name}
    {name}_fc : clock;
    {name}_rc : clock;
    {name}_inform : [0..2] init 0;
    {name}_broken : [0..2] init 0;

    [{name}_fl!] {name}_broken==0 @ {name}_fc -> ({name}_inform'=1) & ({name}_broken'=1);
    [{name}_r??] {name}_broken==1 -> ({name}_broken'=2) & ({name}_rc'={repair});
    [{name}_up!] {name}_broken==2 @ {name}_rc -> ({name}_inform'=2) & ({name}_broken'=0) & ({name}_fc'={fail});

    [{name}_f!!] {name}_inform==1 -> ({name}_inform'=0);
    [{name}_u!!] {name}_inform==2 -> ({name}_inform'=0);
endmodule
"""

BE_NOREPAIR=r"""
/* A BE named {name} */
module BE_{name}
    {name}_fc: clock;
    {name}_inform : [0..1] init 0;
    {name}_broken : [0..1] init 0;
    
    [{name}_fl!] {name}_broken==0 @ {name}_fc -> ({name}_inform'=1) & ({name}_broken'=1);
    [{name}_f!!] {name}_inform==1 -> ({name}_inform'=0);
endmodule
"""

AND=r"""
/* A AND gate named {name} */
module AND_{name}
    {name}_informf: bool init false;
    {name}_informu: bool init false;
    {name}_count: [0..{n_inputs}] init 0;

{body}
    [{name}_f!!]  {name}_informf & {name}_count=={n_inputs}  -> ({name}_informf'=false);
    [{name}_u!!]  {name}_informu & {name}_count!={n_inputs} -> ({name}_informu'=false);
endmodule
"""

OR=r"""
/* A OR gate named {name} */
module OR_{name}
    {name}_informf: bool init false;
    {name}_informu: bool init false;
    {name}_count: [0..{n_inputs}] init 0;

{body}
    [{name}_f!!]  {name}_informf & {name}_count!=0  -> ({name}_informf'=false);
    [{name}_u!!]  {name}_informu & {name}_count==0  -> ({name}_informu'=false);
endmodule
"""

RBOX = r"""
/* A RBOX named {name} */
module RBOX_{name}
    {name}_broken[{n_in}]: bool init false;
    {name}_busy: bool init false;

{body}
endmodule
"""

VOT = r"""
/* A VOT gate named {name} */
module VOT_{name}
    {name}_count: [0..{n}] init 0;
    {name}_inform: bool init false;
    
{body}
    [{name}_f!!] {name}_inform & {name}_count >= {k} -> ({name}_inform'=false);
    [{name}_u!!] {name}_inform & {name}_count < {k} -> ({name}_inform'=false);
endmodule
"""

PAND = r"""
/* A PAND gate named {name} */
module PAND_{name}
    {name}_f0: bool init false;
    {name}_f1: bool init false;
    {name}_st: [0..4] init 0; // up, {name}_inform fail, failed,
                                // {name}_inform up, unbreakable
    
    [_?] {name}_st==0 & {name}_f1 & !{name}_f0 -> ({name}_st'=4);
    
    [{in0}_f??] {name}_st==0 & !{name}_f0 & !{name}_f1-> ({name}_f0'=true);
    [{in0}_f??] {name}_st==0 & !{name}_f0 & {name}_f1 -> ({name}_st'=1) & ({name}_f0'=true);
    [{in0}_f??] {name}_st!=0 & !{name}_f0 -> ({name}_f0'=true);
    [{in0}_f??] {name}_f0 -> ;
    
    [{in1}_f??] {name}_st==0 & !{name}_f0 & !{name}_f1 -> ({name}_f1'=true);
    [{in1}_f??] {name}_st==0 & {name}_f0 & !{name}_f1 -> ({name}_st'=1) & ({name}_f1'=true);
    [{in1}_f??] {name}_st==3 & !{name}_f1 -> ({name}_st'=2) & ({name}_f1'=true);
    [{in1}_f??] ({name}_st==1|{name}_st==2|{name}_st==4) & !{name}_f1 -> ({name}_f1'=true);
    [{in1}_f??] {name}_f1 -> ;
    
    [{in0}_u??] {name}_st!=1 & {name}_f0 -> ({name}_f0'=false);
    [{in0}_u??] {name}_st==1 & {name}_f0 -> ({name}_st'=0) & ({name}_f0'=false);
    [{in0}_u??] !{name}_f0 -> ;
    
    [{in1}_u??] ({name}_st==0|{name}_st==3) & {name}_f1 -> ({name}_f1'=false);
    [{in1}_u??] ({name}_st==1|{name}_st==4) & {name}_f1 -> ({name}_st'=0) & ({name}_f1'=false);
    [{in1}_u??] {name}_st==2 & {name}_f1 -> ({name}_st'=3) & ({name}_f1'=false);
    
    [{name}_f!!] {name}_st==1 -> ({name}_st'=2);
    [{name}_u!!] {name}_st==3 -> ({name}_st'=0);
    
endmodule
"""

SBE = r"""
/* A SBE named {name} */
module SBE_{name}
    {name}_fc: clock;
    {name}_dfc: clock;
    {name}_rc : clock;
    {name}_inform : [0..2] init 0;
    {name}_active : bool init false;
    {name}_broken : [0..2] init 0;
    
    [{name}_e??] !{name}_active -> ({name}_active'=true) & ({name}_fc'={fail});
    [{name}_d??] {name}_active -> ({name}_active'=false) & ({name}_dfc'={dorm});
    
    [{name}_fl!] {name}_active & {name}_broken==0 @ {name}_fc -> ({name}_inform'=1) & ({name}_broken'=1);
    [{name}_fl!] !{name}_active & {name}_broken==0 @ {name}_dfc -> ({name}_inform'=1) & ({name}_broken'=1);
    [{name}_r??] ({name}_broken==1) -> ({name}_broken'=2) & ({name}_rc'={repair});
    [{name}_up!] {name}_active & {name}_broken==2 @ {name}_rc -> ({name}_inform'=2) & ({name}_broken'=0) & ({name}_fc'={fail});
    [{name}_up!] !{name}_active & {name}_broken==2 @ {name}_rc -> ({name}_inform'=2) & ({name}_broken'=0) & ({name}_dfc'={dorm});
    
    [{name}_f!!] {name}_inform==1 -> ({name}_inform'=0);
    [{name}_u!!] {name}_inform==2 -> ({name}_inform'=0);
endmodule
"""

MUX = r"""
/* A MUX named {muxname} */
module MUX_{muxname}
    {muxname}_queue[{n}]: [0..3] init 0;  // idle, requesting, reject, using
    {muxname}_avail: bool init true;
    {muxname}_broken: bool init false;
    {muxname}_enable: [0..2] init 0;
    
    [{name}_fl?] -> ({muxname}_broken'=true);
    [{name}_up?] -> ({muxname}_broken'=false);
    [{name}_e!!] {muxname}_enable==1 -> ({muxname}_enable'=0);
    [{name}_d!!] {muxname}_enable==2 -> ({muxname}_enable'=0);
        
{body}
endmodule
"""

SPARE = r"""
/* A SPARE gate named {name} */
module SPARE_{name}
    {name}_state: [0..4] init 0; // on main, request, wait, on spare, broken
    {name}_inform: [0..2] init 0;
    {name}_release: [-{n}..{n}] init 0;
    {name}_idx: [1..{n}] init 1;

{body}
    [{name}_f!!] {name}_inform == 1 -> ({name}_inform'=0);
    [{name}_u!!] {name}_inform == 2 -> ({name}_inform'=0);
endmodule
"""

class IosaApi:

    def __init__(self, num_phases=0, phase_durations=None):
        self.num_phases = int(num_phases) if num_phases else 0
        self.phase_durations = list(phase_durations) if phase_durations else []

    def _dist_for_phase(self, dist, phase_idx):
        if isinstance(dist, dict):
            return dist.get(int(phase_idx), "")
        return dist

    def _frozen_dist(self, dist):
        if not dist:
            return dist
        m = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\((.*)\)\s*$", str(dist))
        if not m:
            return dist
        kind = m.group(1).lower()
        raw_params = [p.strip() for p in m.group(2).split(",")]
        if kind == "exponential":
            return "exponential(1e-12)"
        if kind == "erlang" and len(raw_params) == 2:
            return "erlang({},1e-12)".format(raw_params[0])
        if kind == "uniform" and len(raw_params) == 2:
            return "uniform(99999999.0,100000000.0)"
        if kind == "normal" and len(raw_params) == 2:
            return "normal(99999999.0,{})".format(raw_params[1])
        if kind == "lognormal" and len(raw_params) == 2:
            return "lognormal(18,{})".format(raw_params[1])
        if kind == "gamma" and len(raw_params) == 2:
            return "gamma(99999999.0,{})".format(raw_params[1])
        if kind == "weibull" and len(raw_params) == 2:
            return "weibull(99999999.0,{})".format(raw_params[1])
        if kind == "rayleigh":
            return "rayleigh(99999999.0)"
        if kind == "dirac" and len(raw_params) == 1:
            return "dirac(99999999.0)"
        return dist

    def make_phase_controller(self):
        if self.num_phases <= 1:
            return ""
        if len(self.phase_durations) != self.num_phases - 1:
            raise Exception("Phase durations must have length num_phases-1")
        k = self.num_phases
        lines = []
        lines.append("module PHASE_Controller")
        lines.append("    PHASE_phase: [1..{k}] init 1;".format(k=k))
        lines.append("    PHASE_started: bool init false;")
        lines.append("    PHASE_emit: [0..{k}] init 0;".format(k=k))
        lines.append("    PHASE_clk: clock;")
        lines.append("")
        lines.append("    [PH_start!!] !PHASE_started -> (PHASE_started'=true) & (PHASE_clk'=dirac({d}));".format(d=self.phase_durations[0]))
        for i in range(1, k):
            # Timed phase advance (tau), followed by committed broadcast
            if i < k - 1:
                lines.append(
                    "    [] PHASE_phase=={i} @ PHASE_clk -> (PHASE_phase'={j}) & (PHASE_emit'={j}) & (PHASE_clk'=dirac({d}));".format(
                        i=i, j=i + 1, d=self.phase_durations[i]
                    )
                )
            else:
                lines.append(
                    "    [] PHASE_phase=={i} @ PHASE_clk -> (PHASE_phase'={j}) & (PHASE_emit'={j});".format(
                        i=i, j=i + 1
                    )
                )

        lines.append("")

        for p in range(2, k + 1):
            lines.append(
                "    [PH_{p}!!] PHASE_emit=={p} -> (PHASE_emit'=0);".format(p=p)
            )
        lines.append("endmodule")
        return "\n".join(lines)

    def make_be (self, name, fail, repair, active_map=None, inherit_fail_from=None, inherit_at_phase=None) :
        if not repair:
            raise Exception("Found non-repairable BE \"{}\": unsupported, aborting".format(name))
        if self.num_phases <= 1:
            return BE_REPAIRABLE.format(name=name, fail=fail, repair=repair)

        k = self.num_phases
        enabled_init = True
        enabled_by_phase = None
        if active_map is not None:
            enabled_by_phase = {int(p): bool(v) for p, v in active_map.items()}
            enabled_init = bool(enabled_by_phase.get(1, True))
        lines = []
        lines.append("/* A BE named {name} */".format(name=name))
        lines.append("module BE_{name}".format(name=name))
        lines.append("    {name}_fc : clock;".format(name=name))
        lines.append("    {name}_rc : clock;".format(name=name))
        lines.append("    {name}_inform : [0..2] init 0;".format(name=name))
        lines.append("    {name}_broken : [0..2] init 0;".format(name=name))
        lines.append("    {name}_phase: [1..{k}] init 1;".format(name=name, k=k))
        if enabled_by_phase is not None:
            lines.append("    {name}_enabled: bool init {init};".format(name=name, init=str(enabled_init).lower()))
        if inherit_fail_from is not None:
            lines.append("    {name}_pred_failed: bool init false;".format(name=name))
        lines.append("")

        if enabled_by_phase is not None:
            fail_1 = self._dist_for_phase(fail, 1)
            init_clk = fail_1 if enabled_init else self._frozen_dist(fail_1)
            lines.append(
                "    [PH_start??] {n}_broken==0 -> ({n}_fc'={f});".format(
                    n=name, f=init_clk
                )
            )
            lines.append("")

        if inherit_fail_from is not None:
            lines.append(
                "    [{pred}_f??] -> ({n}_pred_failed'=true);".format(
                    pred=inherit_fail_from, n=name
                )
            )
            lines.append(
                "    [{pred}_u??] -> ({n}_pred_failed'=false);".format(
                    pred=inherit_fail_from, n=name
                )
            )
            lines.append("")

        # Failure
        lines.append(
            "    [{n}_fl!] {guard}{n}_broken==0 @ {n}_fc -> ({n}_inform'=1) & ({n}_broken'=1);".format(
                n=name,
                guard=("{n}_enabled & ".format(n=name) if enabled_by_phase is not None else "")
            )
        )

        # Repair start (phase-dependent distribution)
        for p in range(1, k + 1):
            rep_p = self._dist_for_phase(repair, p)
            lines.append(
                "    [{n}_r??] {n}_broken==1 & {n}_phase=={p} -> ({n}_broken'=2) & ({n}_rc'={r});".format(
                    n=name, p=p, r=rep_p
                )
            )

        # Repair end (phase-dependent re-arming of failure clock)
        for p in range(1, k + 1):
            fail_p = self._dist_for_phase(fail, p)
            lines.append(
                "    [{n}_up!] {n}_broken==2 & {n}_phase=={p} @ {n}_rc -> ({n}_inform'=2) & ({n}_broken'=0) & ({n}_fc'={f});".format(
                    n=name, p=p, f=fail_p
                )
            )

        lines.append("")

        # Phase updates (synchronized with PHASE_Controller)
        for p in range(2, k + 1):
            fail_p = self._dist_for_phase(fail, p)
            rep_p = self._dist_for_phase(repair, p)
            en_p = None
            if enabled_by_phase is not None:
                en_p = bool(enabled_by_phase.get(p, True))
                fail_p = fail_p if en_p else self._frozen_dist(fail_p)

            # Optional: phase-boundary inheritance (Ai -> A{i+1})
            if inherit_fail_from is not None and inherit_at_phase is not None and int(inherit_at_phase) == p:
                allow_inherit = True
                if enabled_by_phase is not None and not en_p:
                    allow_inherit = False
                if allow_inherit:
                    lines.append(
                        "    [PH_{p}??] ({n}_phase=={prev}) & ({n}_broken==0) & ({n}_pred_failed) -> ({n}_phase'={p}){en} & ({n}_broken'=1) & ({n}_inform'=1);".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] ({n}_phase=={prev}) & ({n}_broken==0) & !({n}_pred_failed) -> ({n}_phase'={p}){en} & ({n}_fc'={f});".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            f=fail_p,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==2 -> ({n}_phase'={p}){en} & ({n}_rc'={r});".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            r=rep_p,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==1 -> ({n}_phase'={p}){en};".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    continue
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==0 -> ({n}_phase'={p}){en} & ({n}_fc'={f});".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    f=fail_p,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==2 -> ({n}_phase'={p}){en} & ({n}_rc'={r});".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    r=rep_p,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==1 -> ({n}_phase'={p}){en};".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
        lines.append("")
        lines.append("    [{n}_f!!] {n}_inform==1 -> ({n}_inform'=0);".format(n=name))
        lines.append("    [{n}_u!!] {n}_inform==2 -> ({n}_inform'=0);".format(n=name))
        lines.append("endmodule")
        return "\n".join(lines)
        
    def make_and(self, name, args) :
        n_in = len(args) 
        body = "";
        for arg in args:
            body += "    [{0}_f??] {name}_count=={1} -> ({name}_count'={name}_count+1) & ({name}_informf'=true);\n".format(arg, n_in-1, name=name)
            body += "    [{0}_f??] {name}_count<{1} -> ({name}_count'={name}_count+1);\n".format(arg, n_in-1, name=name)
            body += "    [{0}_u??] {name}_count=={1} -> ({name}_count'={name}_count-1) & ({name}_informu'=true);\n".format(arg, n_in, name=name)
            body += "    [{0}_u??] {name}_count<{1} & {name}_count>0 -> ({name}_count'={name}_count-1);\n".format(arg, n_in, name=name)
        return AND.format(name=name, n_inputs=n_in, body=body)
    
    def make_or(self, name, args) :
        n_in = len(args) 
        body = "";
        for arg in args:
            body += "    [{0}_f??] {name}_count==0 -> ({name}_count'={name}_count+1) & ({name}_informf'=true);\n".format(arg, name=name)
            body += "    [{0}_f??] {name}_count>0  & {name}_count<{1} -> ({name}_count'={name}_count+1);\n".format(arg, n_in-1, name=name)
            body += "    [{0}_u??] {name}_count>1  -> ({name}_count'={name}_count-1);\n".format(arg, n_in, name=name)
            body += "    [{0}_u??] {name}_count==1 -> ({name}_count'={name}_count-1) & ({name}_informu'=true);\n".format(arg, name=name)
        return OR.format(name=name, n_inputs=n_in, body=body)
        
    def make_vot(self, name, k, children):
        n = len(children)
        body = ""
        for c in children:
            body += "    [{0}_f??] {1}_count<{3} -> ({1}_count'={1}_count+1) & ({1}_inform'=({1}_count+1=={2}));\n".format(c, name, k, n)
            body += "    [{0}_u??] {1}_count>0 -> ({1}_count'={1}_count-1) & ({1}_inform'=({1}_count=={2}));\n".format(c, name, k)
        return VOT.format(name=name, k=k, n=n, body=body)
        
    def make_rbox (self, name, mode, inputs) :
        if mode == "prio":
            body = ""
            for i, item in enumerate(inputs):
                body += "    [{item}_fl?] -> ({name}_broken[{i}]'=true);\n"
                body += "    [{item}_r!!] !{name}_busy & fsteq({name}_broken, true) == {i} -> ({name}_busy'=true);\n"
                body += "    [{item}_up?] -> ({name}_broken[{i}]'=false) & ({name}_busy'=false);\n"
                body = body.format(item=item, i=i, name=name)
            result = RBOX.format(name=name, n_in=len(inputs), body=body)
        else:
            raise Exception("Currently we only support priority \
                 mode repair boxes. Found {0}".format(mode))
        return result
        
    def make_pand (self, name, inputs) :
        if len(inputs) > 2:
            raise Exception("There's support for binary PAND gates only.\
                More inputs can be cascaded e.g. PAND(A,B,C)=PAND(PAND(A,B),C)")
        return PAND.format(name=name, in0=inputs[0], in1=inputs[1])
        
    def make_sbe(self, name, dorm, fail, repair, gates, active_map=None, inherit_fail_from=None, inherit_at_phase=None):

        if not repair:
            raise Exception("Found non-repairable SBE \"{}\": unsupported, aborting".format(name))

        n = len(gates)
        muxname = name + "_MUX"
        body = ""
        for i, item in enumerate(gates):
            body += "    [{item}_{name}_rq??] {muxname}_queue[{i}]==0 & ({muxname}_broken | !{muxname}_avail) -> ({muxname}_queue[{i}]'=2);\n"
            body += "    [{item}_{name}_rq??] {muxname}_queue[{i}]==0 & !{muxname}_broken & {muxname}_avail -> ({muxname}_queue[{i}]'=1);\n"
            body += "    [{name}_{item}_asg!!] {muxname}_queue[{i}]==1"
            aux = " & ".join(["{0}_queue[{1}]==0".format(muxname,e) for e in range(0,i)])
            body += " & " + aux if aux else ""
            body += " & !{muxname}_broken & {muxname}_avail -> ({muxname}_queue[{i}]'=3) & ({muxname}_avail'=false);\n"
            body += "    [{name}_{item}_rj!!] {muxname}_queue[{i}]==2 -> ({muxname}_queue[{i}]'=1);\n"
            body += "    [{item}_{name}_rel??] {muxname}_queue[{i}]==3 -> ({muxname}_queue[{i}]'=0) & ({muxname}_avail'=true) & ({muxname}_enable'=2);\n"
            body += "    [{item}_{name}_acc??] -> ({muxname}_enable'=1);\n"
            body = body.format(muxname=muxname,item=item, name=name, i=i)
        _mux = MUX.format(muxname=muxname, name=name, n=n, body=body)

        if self.num_phases <= 1:
            _sbe = SBE.format(name=name, dorm=dorm, fail=fail, repair=repair)
            return _mux + "\n\n" + _sbe

        k = self.num_phases
        enabled_init = True
        enabled_by_phase = None
        if active_map is not None:
            enabled_by_phase = {int(p): bool(v) for p, v in active_map.items()}
            enabled_init = bool(enabled_by_phase.get(1, True))
        lines = []
        lines.append("/* A SBE named {name} */".format(name=name))
        lines.append("module SBE_{name}".format(name=name))
        lines.append("    {name}_fc: clock;".format(name=name))
        lines.append("    {name}_dfc: clock;".format(name=name))
        lines.append("    {name}_rc : clock;".format(name=name))
        lines.append("    {name}_inform : [0..2] init 0;".format(name=name))
        lines.append("    {name}_active : bool init false;".format(name=name))
        lines.append("    {name}_broken : [0..2] init 0;".format(name=name))
        lines.append("    {name}_phase: [1..{k}] init 1;".format(name=name, k=k))
        if enabled_by_phase is not None:
            lines.append("    {name}_enabled: bool init {init};".format(name=name, init=str(enabled_init).lower()))
        if inherit_fail_from is not None:
            lines.append("    {name}_pred_failed: bool init false;".format(name=name))
        lines.append("")

        if enabled_by_phase is not None:
            dorm_1 = self._dist_for_phase(dorm, 1)
            init_dorm = dorm_1 if enabled_init else self._frozen_dist(dorm_1)
            lines.append(
                "    [PH_start??] {n}_broken==0 -> ({n}_dfc'={d});".format(
                    n=name, d=init_dorm
                )
            )
            lines.append("")
        if inherit_fail_from is not None:
            lines.append(
                "    [{pred}_f??] -> ({n}_pred_failed'=true);".format(
                    pred=inherit_fail_from, n=name
                )
            )
            lines.append(
                "    [{pred}_u??] -> ({n}_pred_failed'=false);".format(
                    pred=inherit_fail_from, n=name
                )
            )
            lines.append("")
        for p in range(1, k + 1):
            fail_p = self._dist_for_phase(fail, p)
            lines.append(
                "    [{n}_e??] {guard}!{n}_active & {n}_phase=={p} -> ({n}_active'=true) & ({n}_fc'={f});".format(
                    n=name,
                    p=p,
                    f=fail_p,
                    guard=("{n}_enabled & ".format(n=name) if enabled_by_phase is not None else "")
                )
            )
        for p in range(1, k + 1):
            dorm_p = self._dist_for_phase(dorm, p)
            lines.append(
                "    [{n}_d??] {guard}{n}_active & {n}_phase=={p} -> ({n}_active'=false) & ({n}_dfc'={d});".format(
                    n=name,
                    p=p,
                    d=dorm_p,
                    guard=("{n}_enabled & ".format(n=name) if enabled_by_phase is not None else "")
                )
            )

        lines.append("")
        lines.append(
            "    [{n}_fl!] {guard}{n}_active & {n}_broken==0 @ {n}_fc -> ({n}_inform'=1) & ({n}_broken'=1);".format(
                n=name,
                guard=("{n}_enabled & ".format(n=name) if enabled_by_phase is not None else "")
            )
        )
        lines.append(
            "    [{n}_fl!] {guard}!{n}_active & {n}_broken==0 @ {n}_dfc -> ({n}_inform'=1) & ({n}_broken'=1);".format(
                n=name,
                guard=("{n}_enabled & ".format(n=name) if enabled_by_phase is not None else "")
            )
        )

        for p in range(1, k + 1):
            rep_p = self._dist_for_phase(repair, p)
            lines.append(
                "    [{n}_r??] ({n}_broken==1) & {n}_phase=={p} -> ({n}_broken'=2) & ({n}_rc'={r});".format(
                    n=name, p=p, r=rep_p
                )
            )

        for p in range(1, k + 1):
            fail_p = self._dist_for_phase(fail, p)
            lines.append(
                "    [{n}_up!] {n}_active & {n}_broken==2 & {n}_phase=={p} @ {n}_rc -> ({n}_inform'=2) & ({n}_broken'=0) & ({n}_fc'={f});".format(
                    n=name, p=p, f=fail_p
                )
            )
        for p in range(1, k + 1):
            dorm_p = self._dist_for_phase(dorm, p)
            lines.append(
                "    [{n}_up!] !{n}_active & {n}_broken==2 & {n}_phase=={p} @ {n}_rc -> ({n}_inform'=2) & ({n}_broken'=0) & ({n}_dfc'={d});".format(
                    n=name, p=p, d=dorm_p
                )
            )

        lines.append("")

        # Phase updates (synchronized with PHASE_Controller)
        for p in range(2, k + 1):
            fail_p = self._dist_for_phase(fail, p)
            dorm_p = self._dist_for_phase(dorm, p)
            rep_p = self._dist_for_phase(repair, p)
            en_p = None
            if enabled_by_phase is not None:
                en_p = bool(enabled_by_phase.get(p, True))
                fail_p = fail_p if en_p else self._frozen_dist(fail_p)
                dorm_p = dorm_p if en_p else self._frozen_dist(dorm_p)

            # Optional: phase-boundary inheritance (Ai -> A{i+1})
            if inherit_fail_from is not None and inherit_at_phase is not None and int(inherit_at_phase) == p:
                allow_inherit = True
                if enabled_by_phase is not None and not en_p:
                    allow_inherit = False
                if allow_inherit:
                    lines.append(
                        "    [PH_{p}??] ({n}_phase=={prev}) & ({n}_broken==0) & ({n}_pred_failed) -> ({n}_phase'={p}){en} & ({n}_broken'=1) & ({n}_inform'=1);".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] ({n}_phase=={prev}) & ({n}_broken==0) & {n}_active & !({n}_pred_failed) -> ({n}_phase'={p}){en} & ({n}_fc'={f});".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            f=fail_p,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] ({n}_phase=={prev}) & ({n}_broken==0) & !{n}_active & !({n}_pred_failed) -> ({n}_phase'={p}){en} & ({n}_dfc'={d});".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            d=dorm_p,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==2 -> ({n}_phase'={p}){en} & ({n}_rc'={r});".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            r=rep_p,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    lines.append(
                        "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==1 -> ({n}_phase'={p}){en};".format(
                            p=p,
                            prev=p - 1,
                            n=name,
                            en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                        )
                    )
                    continue
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==0 & {n}_active -> ({n}_phase'={p}){en} & ({n}_fc'={f});".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    f=fail_p,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==0 & !{n}_active -> ({n}_phase'={p}){en} & ({n}_dfc'={d});".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    d=dorm_p,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==2 -> ({n}_phase'={p}){en} & ({n}_rc'={r});".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    r=rep_p,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )
            lines.append(
                "    [PH_{p}??] {n}_phase=={prev} & {n}_broken==1 -> ({n}_phase'={p}){en};".format(
                    p=p,
                    prev=p - 1,
                    n=name,
                    en=(" & ({n}_enabled'={v})".format(n=name, v=str(en_p).lower()) if enabled_by_phase is not None else "")
                )
            )

        lines.append("")
        lines.append("    [{n}_f!!] {n}_inform==1 -> ({n}_inform'=0);".format(n=name))
        lines.append("    [{n}_u!!] {n}_inform==2 -> ({n}_inform'=0);".format(n=name))
        lines.append("endmodule")

        return _mux + "\n\n" + "\n".join(lines)
    
    def make_spare(self, name, inputs):
        if len(inputs) < 2:
            raise Exception("To build a SPARE you need at least the main input and a spare.")
        n = len(inputs) - 1
        main = inputs[0]
        body = ""
        body += "    [{main}_fl?] {name}_state==0 -> ({name}_state'=1) & ({name}_idx'=1);\n"
        body += "    [{main}_up?] {name}_state==4 -> ({name}_state'=0) & ({name}_inform'=2);\n"
        for i in range(1,n+1):
            body += "    [{main}_up?] {name}_state==3 & {name}_idx=={i} -> ({name}_state'=0) & ({name}_idx'=1) & ({name}_release'={i});\n"
            body = body.format(main=main, name=name, i=i)
        for i, item in enumerate(inputs[1::], 1):
            body += "    [{item}_fl?] {name}_state==3 & {name}_idx=={i} -> ({name}_release'={i});\n"
            body += "    [{name}_{item}_rq!!] {name}_state==1 & {name}_idx=={i} -> ({name}_state'=2);\n"
            body += "    [{item}_{name}_asg??] {name}_state==0 | {name}_state==1 | {name}_state==3 -> ({name}_release'={i});\n"
            body += "    [{item}_{name}_asg??] {name}_state==2 & {name}_idx=={i} -> ({name}_release'=-{i}) & ({name}_state'=3);\n"
            body += "    [{item}_{name}_asg??] {name}_state==4 -> ({name}_release'=-{i}) & ({name}_state'=3) & ({name}_idx'={i}) & ({name}_inform'=2);\n"
            if i < n:
                body += "    [{item}_{name}_rj??] {name}_state==2 & {name}_idx=={i} -> ({name}_idx'={ip1}) & ({name}_state'=1);\n"
            else:
                body += "    [{item}_{name}_rj??] {name}_state==2 & {name}_idx=={i} -> ({name}_state'=4) & ({name}_idx'=1) & ({name}_inform'=1);\n"
            body += "    [{name}_{item}_rel!!] {name}_release=={i} & !({name}_state==3 & {name}_idx=={i}) -> ({name}_release'=0);\n"
            body += "    [{name}_{item}_rel!!] {name}_release=={i} & {name}_state==3 & {name}_idx=={i} -> ({name}_release'= 0) & ({name}_state'=1) & ({name}_idx'=1);\n"
            body += "    [{name}_{item}_acc!!] {name}_release==-{i} -> ({name}_release'=0);\n"
            body = body.format(name=name, item=item, i=i, ip1=i+1)
        return SPARE.format(name=name, n=n, body=body)

    def make_dist_exponential(self, l) :
        return r"exponential({})".format(l)

    def make_dist_uniform(self, l, u) :
        return r"uniform({},{})".format(l, u)

    def make_dist_weibull(self, l, u) :
        return r"weibull({},{})".format(l, u)
        
    def make_dist_normal(self, l, u) :
        return r"normal({},{})".format(l, u)

    def make_dist_lognormal(self, l, u) :
        return r"lognormal({},{})".format(l, u)
        
    def make_dist_rayleigh(self, l):
        return r"rayleigh({})".format(l)
        
    def make_dist_gamma(self, l, u) :
        return r"gamma({},{})".format(l, u)
        
    def make_dist_erlang(self, l, u) :
        return r"erlang({},{})".format(l, u)

        
    def make_dist_dirac(self, l):
        return r"dirac({})".format(l)
                
    def fail_condition(self, name, t, n, k) :
        result = ""
        if t == "be":
            result = "{0}_broken== | {0}_broken==2".format(name)
        elif t == "and_gate":
            result = "{0}_count=={1}".format(name, n)
        elif t == "or_gate":
            result = "{0}_count!=0".format(name)
        elif t == "pand_gate":
            result = "{0}_st==2".format(name)
        elif t == "vot_gate":
            result = "{0}_count>={1}".format(name, k)
        elif t == "spare_gate":
            result = "{0}_state==4".format(name)
        else:
            print("Non supported gate type {} for TLE.".format(t))
            # raise Exception("Non supported node type {}".format(t))
            result = "ERROR_"
        return result
            
