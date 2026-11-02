#!/usr/bin/env python
#
# Compiler of IOSA modules from FTs written Kepler:
# This is the CLI access point of the compiler created by Raul E. Monti.
#
# Authors: Carlos E. Budde
# Date:    15.10.2021
# License: GPLv3
#

from kepler import Kepler2IOSA
from os import path
import argparse
import sys

A=b'aEYWORDKEYWORDKEYWORDKKUYA\\HGKWWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWeRDKEYWORDKEYWORDKEW[A^^\x02\x0c\x10\x1e\x06\x1b\r\x02^C[ORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKoYWORDKEYWORDKEYWO^^EKCMTI^Q^BLTH^QIWWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWOxDKEYWORDKEYWORDKICYC\x1b\x10Z^W[U\\JKEYYA\\JGKYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEsWORDKEYWORDKEYWA^JG_H\x03\x1b\x1bHE\x0c\x10LU\\DKEYWA^DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORnKEYWORDKEYWORDKKYWUCU\x1f\x11\rF\x06\x06\x10Z\x0cBMARDKKUYORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEY}ORDKEYWORDKEYWO\\DE^\x10LT\x1b\rZT\x10LTI_QIUYO\\DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDaEYWORDKEYWORDKKUWAHJEIUM^IJEKU[C^HEEYYA\\DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWeRDKEYWORDKEYWO\\HKI\x10LTCUZ\x11BL\x06\x1b\rP_C[CRDEIWWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKoYWORDKEYWORDKEYWAHUZT\rF^\x06\rP\x0cHF\x06\x1b_QKWYORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWOxDKEYWORDKEYWORDKKULT\x1bU\x02_C[C\x1bU\x02^C[C\\HGKWWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEsWORDKEYWORDKEYWORHP^\x10F\x06\x1b_P^BLTH^GIU[C\\JEEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORnKEYWORDKEYWORDKEWWU\x1b\r\x02^BLTHHP_CMU^HGEYYORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEY}ORDKEYWORDKEYWORJEKBL\x06\x1b\r\x02\x0cBLTHHGIU[ARDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDaEYWORDKEYWORDKEYYARHQ_\x10\x1e\x06I_P_U[C^HGIYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWeRDKEYWORDKEYWORDKEWFT^HGIU[C^HGKWYC^DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKoYWORDKEYWORDKEYWAI(\x1f^B[A\\JEKWYA\\JGIULU\\DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWOxDKEYWORDKEYWO\\^Z&:\x11^\x1b\r\x02^UYA\\JEKWYC^HPT\x10LU\\DKEWYA\\DKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEsWORDKEYWORDGT54,\x14U\x02T\rF^\x1b_GKYWA\\JEKWMT\x1b\x10\x1f\x03\x1f\x03^\x14\x02\r)5;#\x14U\x02_WWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORnKEYWORDKKCF#1(\x1fT\x10\x1e\x06C\x10\x1f\x11\rF\x06I_P^BL\x06I^P^\x10\x03#>\'(&:0(5\'\')5;#>\x02Z^UYORDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKEY}ORDKEYWCIU\x1f\x03\r\x11\x1b\x14(\'\x03\x1fF^\x06(\rTHF^\x06\x10\x1f\x03\rFT\x1b\x10\r\x03\x1f\x03\x1b\x06\x10\x1f\x11\r\x03^C\r\x02\x0c\x10\x1e^CUZ_YWORDKEYWORDKEYWORDKEYWORDKEYWORDKEYM\x1b>\x02aEYWORDE^\r\x03\x1b\x06\x02\'\x03\r\x11\t\x06\x10ZTH\x03\x1b\x06UZTH\x1e\x06I^Q^\x10F\x1b\x06UZ\x0c\x10\x1e\x06\x1b\r\x02\x0c\x10\x1eTI^GIC\x1e\x1b\x14\x10\r\x03\x1f\x03^\x1bHEEYWORDKEYWORDKEYWORDKEYWC\x06\x02\'\x03\x1f\x03eRDKEYM^\x14\x10ZT\r\x03\x1b\x14\x10ZT\x10\x1e\x06C\x10Z\x11H\x1e\x06\x1b_Q_B\x1e\x1b\x06\x10Z\x0c\x10F^CUZTHF^C\r\x02\x0cB[CIU\x1f\x03>0(1\'(&>4#\x06_GEYWORDKEYWORDKEYWORDKKH\x03^CU\x02oYWOR^\x02\x0cH\x1e\x06\x1b\r\x02T\x1f;\x1bCU\x02^\x10FT\x1b_P^CMT\x1bU\x1f\x11\rF\x06\x1b_Z\x03\r\x03^CUZTH\x1e\x06\x1b_QIH\x03^C\x02\')5;#>(\'):4\tCHKEYWORDKEYYC^HQ_CL\x06C\x10ZTHF^xDKEULTI_P^\x10\x1e^\x06\x02\r\x11H\x1e\x06I^Q_C[C^JQT\r\x03^C\r\x02\x0c\x10\x1eTC\x02\x1fTHF^\x1b\rP^CMC^^P^\x10\x1e^\x06\x10\r)54,1(\r\x03\rFCRDKEWM\x06\x06\x02\'">0(5#(&\x1fF\x06CUZ\x0csWO\\HQ^BLTI_P\x0cH\x1e\x06\x1b\rP^BMU^HEKUMTIU\x1fT\x10\x1e\x06I_P^B\x1e^CU\x02^BLUHHGIU[UH^P^\x10\x1e\x06CU\x1f\x03\x1f\x11\x1bCUZ)HF\x1b\x14\'(":4(1\'()5;\t\x14\x10Z\x0cCMORnKEW[UH^Q^BLTI_P^B\x1e\x06\x1b\rP\x0cHF^\x14\x02\x1f\x03\x1f\x03\x06\x1b_P^BMUH^Q^BLUH^QIU[C\\HQ\x0c\x10MUH^P^B\x1e\x06CU\x1f\x11\x1f\x11#1#,":;#>(\')\x1f\x03\x1b\x06\x10ZT\x10LTI_KEY}O\\HQICMUH^Q^BLTI_P^\x10\x1e^\x06UZT\r\x11\x1b\x06\x02\r\x11BMUHHGIU[C^^Q_CMC^HGKW[CHU\x02^BMUI\r\x02\x0cHF\x1b\x14\x02\r\x03\r\x11\x1b\x06\x10\x1f\x11\r\x03^C\r\x02\x0c\x10\x1eTIDKEYWORDaEWMT^HQ_CMUH^Q_CLT\x1b\x10\x1f\x11HF^C\x10\x1f\x03\x1f\x11\t\x14\x10\x02_U[C^HGIUYC^HGIU[ARJEIC\x1e\x06CU\x02\x0c\x10\x1e^C\x10\x1fT\x10\x1e\x06CUZTHF\x1bC\r\x02\x0c\x10LT\x1b_P^YWORDKEYWeRHQ^C[UH^Q_CMUH_\x02\x0cHF^CUZ\x11\r\x11\t\x14(\'\x03\r\x03\x06IHEIU[A\\HGKWYC^HEICYA\\HQ^\x10\x1e\x06\x1b_P^B\x1e\x06CUZ\x0c\x10\x1e\x06I_P^\x10LTI_PEYWORDKEYWORDKoYYCH^QICMTI\rZ\x0cBLT\x1b\r\x02\x0cH\x03#>\x02\x1f\x11\r\x11\x1b\x06\x10\x02^CMA\\JEKW[C^^QKWYCH_PIUYCH^Q_CL\x06\x06\x02Z^C[UI_Q^CMCH^KEYWORDKEYWORDKEYWOxDKEU[C^JG_CL\x06\x1b^P^BLTI_\x02T\r\x11\t\x06UZT\r\x03\x1b\x14\x02\x1f\x11HF\x1b\x06\x10\x1f\x11HF\x06IHG_BLT\x1b_P_CMUI\r\'">4,>\x02Z_UWORDKEYWORDKEYWORDKEYWORDKEsWORDEKWYA\\HQ^BMUH^Q_CMUH^PT\x1f\x03\x06\x1bUZ\x11\r\x03\x1b\x06\x02\')5;#>(\')\x1f\x11\x1b\x14\x02\r)\x1f\x11\x1b\x06\x02\r\x0354,>\x02\x1f\x11\r\x03\x1bC_PEYWORDKEYWORDKEYWORDKEYWORnKEYWORDEIUYCH^Q_CMC^HQ_CMUH\r\x1f\x0c\x10\x1e\x06\x1b_P^B\x1e\x06\x1bU\x1f\x0354#>\x02\r\x03\x1f\x11\t\x14\x02\x1f\x11\x1f\x11\x1b\x14\x02\r\x03\rF^\x1b\rP^CMTRDKEYWORDKEYWORDKEYWORDKEY}ORDKEYWO\\HGKW[UH^Q_CMUH^P^C[UI^Q_U[UH^P^\x10\x1e\x06C\x10\x1f\x11\r\x03\x1b\x06\x10\x1f\x11\r\x03^CUZTHF\x06\x1b_P_CMUHDKEYWORDKEYWORDKEYWORDKEYWORDaEYWORDKEYYC^JEKU[UH^Q_CMUH^Q_CMUH_P^BLTI\r\x02\x0c\x10\x1e\x06\x1b\r\x02\x0cBLTI_P^BLTI_\x02\x0cBMUH^KEYWORDKEYWORDKEYWORDKEYWORDKEYWeRDKEYWORDKKU[A\\JEIU[CH^QIU[UH^Q^\x10\x1e\x06\x1b_P^BLTI_P^BLTI^Q^CLUH^Q_CMTIDKEYWORDKEYWORDKEYWORDKEYWORDKEYWORDKoZW\x07\x06\x10\x1b\x16CX@\x13\x17\x08\x0c\x10Y\x0c\x1dJ\x1e\x0eV\x16\x01\x1b\t\n\x11\x1c\x13B\x13\x16\x1fJ\x1f\x1b\n\n\r\x05\x02T\x16\x1d\x1c\x0b\x07\x01T\x04\x0c\x1a\x13\n\x17\x03\x12\x01\x17\x03\x0c\x00\x0bZ\x0e\x1c\r\x06\x04\r\x12\x0b_\x05\x18\x06\x10\x1eB\x13\x16\x1fK\x11\x03\x02\x1eDKEYWORDKEYWOx'

TIMER_TEMPLATE="""
const float TO = {:.1f};  // Time-out limit for reliability

module ReliabilityTimer
	timeout: [0..2] init 0;
	clkTO: clock;
	[TOstart!!] timeout == 0         -> (timeout'=1) & (clkTO'=dirac(TO));
	[TOend!   ] timeout == 1 @ clkTO -> (timeout'=2);
endmodule
"""

if __name__ == "__main__":

	# Easter egg
	if (len(sys.argv[1:]) == 1) and (sys.argv[1] == "--T800"):
		mask = b'keyword'   # [stackoverflow.com/a/7489718]
		lmask = len(mask)
		print(bytes(c ^ mask[i % lmask] for i, c in enumerate(A)).decode())
		exit(0)

	# CLI arguments
	CLI = argparse.ArgumentParser(description="Compile IOSA model from FT written in Kepler")
	CLI.add_argument("input",
                     type    = argparse.FileType('r'),
	                 default = sys.stdin,
                     metavar = "FT",
	                 help    = "Input file (path): a fault tree written in Kepler")
	CLI.add_argument("output",
	                 nargs   = '?',
	                 type    = argparse.FileType('a', encoding='UTF-8'),
	                 default = sys.stdout,
	            	 metavar = "IOSA",
	                 help    = "Output file (path): an IOSA model")
	CLI.add_argument("-f", "--force",
	                 dest    = "overwrite",
	                 action  = 'store_true',
	                 help    = "Overwrite output file")
	CLI.add_argument("-s", "--steadystate", "--availability",
	                 dest    = "prop_ss",
	                 action  = 'store_true',
	                 help    = "Create steady-state property query")
	CLI.add_argument("-t", "--transient", "--reliability",
	                 dest    = "prop_tr",
	                 type    = int,
	            	 metavar = "T",
	                 help    = "Create transient property query for time horizon T")
	CLI.add_argument("-i", "--ifun",
	                 nargs   = '+',
	                 type    = str,
	                 metavar = "TYPE",
	                 help    = "Derive importance function(s) of TYPE: "\
	                           "STR (structural), "\
	                           "MCS (minimal cut sets), "\
	                           "MCSN (normalised MCS), "\
	                           "TIME (phase-aware heuristic), "\
	                           "PHYS (physics-weight heuristic), "\
	                           "TIMEPHYS (phase+physics heuristic), "\
                           "PWSTR (physically-weighted structural), "\
                           "T3 (physical pushability composite), "\
                           "M3P (M3+ structural backbone + T3 refinement)")
	CLI.add_argument("--phys-k",
	                 dest    = "phys_k",
	                 type    = int,
	                 default = 10,
	                 help    = "Number of physical micro-layers K for heuristic IFUNs")
	CLI.add_argument("--phys-beta",
	                 dest    = "phys_beta",
	                 type    = float,
	                 default = 1.0,
	                 help    = "Physical discretisation exponent beta for heuristic IFUNs")
	CLI.add_argument("--phys-gamma",
	                 dest    = "phys_gamma",
	                 type    = float,
	                 default = 1.0,
	                 help    = "Physical bias strength gamma for heuristic IFUNs")
	CLI.add_argument("--pand-eta",
	                 dest    = "pand_eta",
	                 type    = float,
	                 default = 1.0,
	                 help    = "PAND order bonus parameter eta for heuristic IFUNs")
	CLI.add_argument("--split-mmax",
	                 dest    = "split_mmax",
	                 type    = int,
	                 default = 0,
	                 help    = "Emit TIMEPHYS split formula using m_max as the maximum offspring count")

	CLI.add_argument("--pwstr-k",
	                 dest    = "pwstr_k",
	                 type    = int,
	                 default = 1000,
	                 help    = "Number of output importance levels K for PW-STR IFUN")
	CLI.add_argument("--pwstr-beta-req",
	                 dest    = "pwstr_beta_req",
	                 type    = float,
	                 default = 0.5,
	                 help    = "Required-type weight exponent for PW-STR IFUN")
	CLI.add_argument("--pwstr-beta-alt",
	                 dest    = "pwstr_beta_alt",
	                 type    = float,
	                 default = 0.5,
	                 help    = "Alternative-type weight exponent for PW-STR IFUN")
	CLI.add_argument("--pwstr-a-min",
	                 dest    = "pwstr_a_min",
	                 type    = float,
	                 default = 0.5,
	                 help    = "Minimum A (required-type) weight for PW-STR IFUN")
	CLI.add_argument("--pwstr-a-max",
	                 dest    = "pwstr_a_max",
	                 type    = float,
	                 default = 2.0,
	                 help    = "Maximum A (required-type) weight for PW-STR IFUN")
	CLI.add_argument("--pwstr-b-min",
	                 dest    = "pwstr_b_min",
	                 type    = float,
	                 default = 0.5,
	                 help    = "Minimum B (alternative-type) weight for PW-STR IFUN")
	CLI.add_argument("--pwstr-b-max",
	                 dest    = "pwstr_b_max",
	                 type    = float,
	                 default = 2.0,
	                 help    = "Maximum B (alternative-type) weight for PW-STR IFUN")
	CLI.add_argument("--t3-phys-k",
	                 dest    = "t3_phys_k",
	                 type    = int,
	                 default = 10,
	                 help    = "Number of physical micro-layers for T3 IFUN")
	CLI.add_argument("--t3-eta",
	                 dest    = "t3_eta",
	                 type    = float,
	                 default = 1.0,
	                 help    = "Physical pushability multiplier eta for T3 composite IFUN")
	CLI.add_argument("--t3-k",
	                 dest    = "t3_k",
	                 type    = int,
	                 default = 1000,
	                 help    = "Number of output importance levels K for T3 IFUN")
	CLI.add_argument("--yu-sbdd",
	                 dest    = "yu_sbdd",
	                 action  = "store_true",
	                 help    = "Append Yu Haiyue improved-SBDD analytic report")
	CLI.add_argument("--yu-only",
	                 dest    = "yu_only",
	                 action  = "store_true",
	                 help    = "Only run Yu Haiyue analytic report and skip IOSA compilation")
	CLI.add_argument("--yu-report",
	                 dest    = "yu_report",
	                 type    = str,
	                 metavar = "PATH",
	                 help    = "Write Yu Haiyue analytic report to PATH")
	CLI.add_argument("--yu-horizon",
	                 dest    = "yu_horizon",
	                 type    = float,
	                 default = None,
	                 help    = "Mission horizon for Yu exact phase-CTMC probability; defaults to -t/--transient when present")
	CLI.add_argument("--yu-max-css",
	                 dest    = "yu_max_css",
	                 type    = int,
	                 default = 2000,
	                 help    = "Maximum symbolic first-hit prefixes to emit in Yu report")
	CLI.add_argument("--yu-state-limit",
	                 dest    = "yu_state_limit",
	                 type    = int,
	                 default = 20000,
	                 help    = "Maximum reachable states per phase for Yu exact phase-CTMC calculation")
	
	# CLI parsing
	args = CLI.parse_args()
	IOSA = args.output
	kepler = Kepler2IOSA(phys_k=args.phys_k,
		                    phys_beta=args.phys_beta,
		                    phys_gamma=args.phys_gamma,
		                    pand_eta=args.pand_eta,
		                    transient_horizon=(args.prop_tr or 0),
		                    pwstr_k=args.pwstr_k,
		                    pwstr_beta_req=args.pwstr_beta_req,
		                    pwstr_beta_alt=args.pwstr_beta_alt,
		                    t3_eta=args.t3_eta,
		                    pwstr_a_min=args.pwstr_a_min,
		                    pwstr_a_max=args.pwstr_a_max,
		                    pwstr_b_min=args.pwstr_b_min,
		                    pwstr_b_max=args.pwstr_b_max,
		                    t3_phys_k=args.t3_phys_k,
		                    t3_k=args.t3_k)
	if path.isfile(IOSA.name) and (path.getsize(IOSA.name) > 0) and (not args.overwrite):
		print("File \""+IOSA.name+"\" exists; call with -f to overwrite. Aborting.")
		exit(1)
	FT = "".join([s for s in args.input.readlines()])

	if args.yu_only:
		_yu_horizon = args.yu_horizon if args.yu_horizon is not None else args.prop_tr
		_yu_report = kepler.yu_sbdd_report(
			FT,
			horizon=_yu_horizon,
			max_css=args.yu_max_css,
			state_limit=args.yu_state_limit)
		if args.yu_report:
			with open(args.yu_report, "w", encoding="utf-8") as yu_out:
				yu_out.write(_yu_report)
				yu_out.write("\n")
		else:
			if path.isfile(IOSA.name):
				IOSA.truncate(0)
			IOSA.write(_yu_report)
			IOSA.write("\n")
		if (IOSA != sys.stdout):
			IOSA.close()
			print("Wrote Yu Haiyue analytic report \"" + (args.yu_report or IOSA.name) + "\"")
		exit(0)

	# Compile FT and dump resulting IOSA
	if path.isfile(IOSA.name):
		IOSA.truncate(0)
	IOSA.write(kepler.compile(FT))

	# Create and append requested properties queries
	properties=""
	if args.prop_ss:
		properties += "\n  S( " + kepler.top_level_event(FT) + " )";
	if args.prop_tr:
		properties += "\n  P( (timeout<2) U (" + kepler.top_level_event(FT) + ") )";
		IOSA.write(TIMER_TEMPLATE.format(args.prop_tr))  # add timer IOSA module
	if 0 < len(properties):
		IOSA.write("\nproperties" + properties + "\nendproperties\n")
	
	# Derive and dump requested importance functions
	requested_ifuns = args.ifun or []
	valid_ifuns = ["NONE", "STR", "MCS", "MCSN", "TIME", "PHYS", "TIMEPHYS", "PWSTR", "T3", "M3P", "N4", "N4P", "N4Q", "N4A", "N4AH", "N4AN", "N5C", "N5P"]
	valid_ifuns.append('ARDIV2')
	for ifun in [i for i in requested_ifuns if i not in valid_ifuns]:
		print("Ignoring unrecognised IFUN requested \"" + ifun + "\"", file=sys.stderr)
	if "STR" in requested_ifuns:
		IOSA.write("\n// IFUN STRUCTURAL: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_structural(FT)]))
	if "MCS" in requested_ifuns:
		IOSA.write("\n// IFUN MCS: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_mcs(FT)]))
	if "MCSN" in requested_ifuns:
		IOSA.write("\n// IFUN MCSN: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_mcsn(FT)]))
	if "TIME" in requested_ifuns:
		IOSA.write("\n// IFUN TIME: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_time(FT)]))
	if "PHYS" in requested_ifuns:
		IOSA.write("\n// IFUN PHYS: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_phys(FT)]))
	if ("PHYS" in requested_ifuns) and (args.split_mmax is not None) and (args.split_mmax > 1):
		split_spec = kepler.split_phys(FT, args.split_mmax)
		if split_spec:
			IOSA.write("\n// SPLIT PHYS: ")
			IOSA.write(split_spec)
	if "TIMEPHYS" in requested_ifuns:
		IOSA.write("\n// IFUN TIMEPHYS: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_timephys(FT)]))
	if ("TIMEPHYS" in requested_ifuns) and (args.split_mmax is not None) and (args.split_mmax > 1):
		split_spec = kepler.split_timephys(FT, args.split_mmax)
		if split_spec:
			IOSA.write("\n// SPLIT TIMEPHYS: ")
			IOSA.write(split_spec)
	if "PWSTR" in requested_ifuns:
		IOSA.write("\n// IFUN PWSTR: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_pwstr(FT)]))
	if "T3" in requested_ifuns:
		IOSA.write("\n// IFUN T3: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_t3(FT)]))
	if "M3P" in requested_ifuns:
		IOSA.write("\n// IFUN M3P: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_m3p(FT)]))
	if "N4" in requested_ifuns:
		IOSA.write("\n// IFUN N4: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_n4(FT)]))
	if "N4P" in requested_ifuns:
		_n4p_spec = kepler.ifun_n4p(FT)
		IOSA.write("\n// N4P DIAG: ")
		IOSA.write(kepler.n4p_diagnostic())
		IOSA.write("\n// IFUN N4P: ")
		IOSA.write(";".join([str(x) for x in _n4p_spec]))
	if "N4Q" in requested_ifuns:
		_n4q_spec = kepler.ifun_n4q(FT)
		IOSA.write("\n// N4Q DIAG: ")
		IOSA.write(kepler.n4q_diagnostic())
		IOSA.write("\n// IFUN N4Q: ")
		IOSA.write(";".join([str(x) for x in _n4q_spec]))
	if "N4A" in requested_ifuns:
		_n4a_spec = kepler.ifun_n4a(FT)
		IOSA.write("\n// N4A DIAG: ")
		IOSA.write(kepler.n4a_diagnostic())
		IOSA.write("\n// IFUN N4A: ")
		IOSA.write(";".join([str(x) for x in _n4a_spec]))
	if "N4AH" in requested_ifuns:
		_n4ah_spec = kepler.ifun_n4a(FT)
		IOSA.write("\n// N4AH DIAG: ")
		IOSA.write(kepler.n4a_diagnostic())
		IOSA.write("\n// IFUN N4AH: ")
		IOSA.write(";".join([str(x) for x in _n4ah_spec]))
	if "N4AN" in requested_ifuns:
		_n4an_spec = kepler.ifun_n4an(FT)
		IOSA.write("\n// N4AN DIAG: ")
		IOSA.write(kepler.n4an_diagnostic())
		IOSA.write("\n// IFUN N4AN: ")
		IOSA.write(";".join([str(x) for x in _n4an_spec]))
	if "N5C" in requested_ifuns:
		IOSA.write("\n// IFUN N5C: ")
		IOSA.write(";".join([str(x) for x in kepler.ifun_n5c(FT)]))
	if "N5P" in requested_ifuns:
		IOSA.write("\n// N5P DIAG: ")
		_n5p_spec = kepler.ifun_n5p(FT)
		IOSA.write(kepler.n5p_diagnostic())
		IOSA.write("\n// IFUN N5P: ")
		IOSA.write(";".join([str(x) for x in _n5p_spec]))
	if 'ARDIV2' in requested_ifuns:
		_ardiv2_spec = kepler.ifun_ardiv2(FT)
		IOSA.write('\n// ARDIV2 DIAG: ')
		IOSA.write(kepler.ardiv2_diagnostic())
		IOSA.write('\n// IFUN ARDIV2: ')
		IOSA.write(';'.join([str(x) for x in _ardiv2_spec]))
	if args.yu_sbdd or args.yu_report:
		_yu_horizon = args.yu_horizon if args.yu_horizon is not None else args.prop_tr
		_yu_report = kepler.yu_sbdd_report(
			FT,
			horizon=_yu_horizon,
			max_css=args.yu_max_css,
			state_limit=args.yu_state_limit)
		if args.yu_sbdd:
			IOSA.write("\n/*\n")
			IOSA.write(_yu_report)
			IOSA.write("\n*/\n")
		if args.yu_report:
			with open(args.yu_report, "w", encoding="utf-8") as yu_out:
				yu_out.write(_yu_report)
				yu_out.write("\n")
	IOSA.write("\n")

	# Hasta la vista
	if (IOSA != sys.stdout):
		IOSA.close()
		print("Compiled IOSA file \"" + IOSA.name + "\"")
