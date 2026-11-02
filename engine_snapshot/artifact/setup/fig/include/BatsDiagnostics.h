//==============================================================================
//
//  BatsDiagnostics.h
//
//  Small opt-in diagnostics and experiment controls for RD-GT/BATS studies.
//
//==============================================================================

#ifndef BATSDIAGNOSTICS_H
#define BATSDIAGNOSTICS_H

#include <cstdlib>
#include <cerrno>
#include <cmath>
#include <limits>
#include <string>
#include <sstream>
#include <vector>
#include <map>

#include <FigException.h>
#include <XiWangPopulation.h>
#include <ModelSuite.h>

namespace fig
{

inline bool
bats_diag_enabled()
{
	const char* raw = std::getenv("FIG_ISPLIT_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}


inline double
bats_target_p()
{
	const char* raw = std::getenv("BATS_TARGET_P");
	if (nullptr == raw || raw[0] == '\0')
		return 0.0;
	char* err(nullptr);
	const double value = std::strtod(raw, &err);
	if ((nullptr != err && err[0] != '\0') || value <= 0.0 || value >= 1.0)
		return 0.0;
	return value;
}


inline size_t
bats_level_population()
{
	const char* raw = std::getenv("BATS_LEVEL_POPULATION");
	if (nullptr == raw || raw[0] == '\0')
		return 0ul;
	const std::string text(raw);
	if (text == "0")
		return 0ul;
	if (!text.empty() && text[0] == '-')
		throw_FigException("BATS_LEVEL_POPULATION must be unset, 0, or a positive integer");
	char* err(nullptr);
	errno = 0;
	const unsigned long long value = std::strtoull(raw, &err, 10);
	if (errno == ERANGE || (nullptr != err && err[0] != '\0') ||
	    value == 0ull ||
	    value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
		throw_FigException("BATS_LEVEL_POPULATION must be unset, 0, or a positive integer");
	return static_cast<size_t>(value);
}


inline bool
bats_adaptive_level_population_enabled()
{
	const char* raw = std::getenv("BATS_ADAPTIVE_LEVEL_POPULATION");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}

inline std::string
bats_alp_config()
{
	const char* raw = std::getenv("BATS_ALP_CONFIG");
	if (nullptr == raw || raw[0] == '\0')
		return "ALP0";
	const std::string value(raw);
	if (value != "ALP0" && value != "ALP1" && value != "ALP2")
		throw_FigException("BATS_ALP_CONFIG must be ALP0, ALP1, or ALP2");
	return value;
}

inline size_t
bats_adaptive_calibration_population()
{
	return 24ul;
}

inline size_t
bats_adaptive_calibration_sweeps()
{
	return 3ul;
}


inline double
bats_env_double(const char* name, const double defaultValue)
{
	const char* raw = std::getenv(name);
	if (nullptr == raw || raw[0] == '\0')
		return defaultValue;
	char* err(nullptr);
	const double value = std::strtod(raw, &err);
	if ((nullptr != err && err[0] != '\0') || value != value)
		return defaultValue;
	return value;
}

inline bool
bats_bsrtp_enabled()
{
	const char* raw = std::getenv("BATS_BSRTP");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}

inline double bats_bsrtp_budget_multiplier()
{
	return bats_env_double("BATS_BSRTP_BUDGET_MULTIPLIER", 1.0);
}

inline double bats_bsrtp_risk_relaxation()
{
	return bats_env_double("BATS_BSRTP_RISK_RELAXATION", 0.05);
}

inline double bats_bsrtp_max_change()
{
	return bats_env_double("BATS_BSRTP_MAX_CHANGE", 0.20);
}

inline double bats_bsrtp_history_decay()
{
	return bats_env_double("BATS_BSRTP_HISTORY_DECAY", 1.0);
}

inline size_t bats_bsrtp_replan_interval()
{
	const double value = bats_env_double("BATS_BSRTP_REPLAN_INTERVAL", 5.0);
	return value >= 1.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 5ul;
}

inline size_t bats_bsrtp_min_population()
{
	const double value = bats_env_double("BATS_BSRTP_MIN_POPULATION", 12.0);
	return value >= 1.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 12ul;
}

inline size_t bats_bsrtp_max_population()
{
	const double value = bats_env_double("BATS_BSRTP_MAX_POPULATION", 360.0);
	return value >= 1.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 360ul;
}

inline size_t bats_bsrtp_reference_population()
{
	const double value = bats_env_double(
		"BATS_BSRTP_REFERENCE_POPULATION", 288.0);
	return value >= 1.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 288ul;
}

inline bool
bats_rolling_level_optimization_enabled()
{
	const char* raw = std::getenv("BATS_ROLLING_LEVEL_OPT");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_vcstp_enabled()
{
	const char* raw = std::getenv("BATS_VCSTP");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline double
bats_vcstp_epsilon()
{
	const double value = bats_env_double("BATS_VCSTP_EPSILON", 0.05);
	return value >= 0.0 && std::isfinite(value) ? value : 0.05;
}


inline bool
bats_vcstp_protect_fixed_variance_enabled()
{
	const char* raw = std::getenv("BATS_VCSTP_PROTECT_FIXED_VARIANCE");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_vcstp_require_cost_reduction_enabled()
{
	const char* raw = std::getenv("BATS_VCSTP_REQUIRE_COST_REDUCTION");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_vcstp_compact_log_enabled()
{
	const char* raw = std::getenv("BATS_VCSTP_COMPACT_LOG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_vcstp_v3_enabled()
{
	const char* raw = std::getenv("BATS_VCSTP_V3");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline double
bats_vcstp_variance_tolerance()
{
	const double value = bats_env_double("BATS_VCSTP_VARIANCE_TOLERANCE", 0.01);
	return value >= 0.0 && std::isfinite(value) ? value : 0.01;
}


inline size_t
bats_vcstp_replan_interval()
{
	const double value = bats_env_double("BATS_VCSTP_REPLAN_INTERVAL", 5.0);
	return value >= 1.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 5ul;
}


inline double
bats_vcstp_kappa()
{
	const double value = bats_env_double("BATS_VCSTP_KAPPA", 0.20);
	return value >= 0.0 && value <= 1.0 && std::isfinite(value)
	       ? value : 0.20;
}


inline double
bats_vcstp_delta_path()
{
	const double value = bats_env_double("BATS_VCSTP_DELTA_PATH", 0.0025);
	return value > 0.0 && value < 1.0 && std::isfinite(value)
	       ? value : 0.0025;
}


inline size_t
bats_vcstp_min_samples()
{
	const double value = bats_env_double("BATS_VCSTP_MIN_SAMPLES", 48.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 48ul;
}


inline size_t
bats_vcstp_min_population()
{
	const double value = bats_env_double("BATS_VCSTP_MIN_POPULATION", 8.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 8ul;
}


inline size_t
bats_vcstp_max_population()
{
	const double value = bats_env_double("BATS_VCSTP_MAX_POPULATION", 360.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 360ul;
}


inline bool
bats_csla_enabled()
{
	const char* raw = std::getenv("BATS_CSLA");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline double
bats_csla_budget_eta()
{
	return bats_env_double("BATS_CSLA_BUDGET_ETA", 0.90);
}


inline double
bats_csla_max_extinction_risk()
{
	const double value = bats_env_double(
		"BATS_CSLA_MAX_EXTINCTION_RISK", 1.0);
	return value >= 0.0 && value <= 1.0 && std::isfinite(value)
	       ? value : 1.0;
}


inline double
bats_csla_fixed_event_budget()
{
	const double value = bats_env_double("BATS_CSLA_FIXED_EVENT_BUDGET", 0.0);
	return value > 0.0 && std::isfinite(value) ? value : 0.0;
}


inline double
bats_csla_max_batch_change()
{
	return bats_env_double("BATS_CSLA_MAX_BATCH_CHANGE", 0.25);
}


inline size_t
bats_csla_min_samples()
{
	const double value = bats_env_double("BATS_CSLA_MIN_SAMPLES", 48.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 0ul;
}


inline size_t
bats_csla_min_population()
{
	const double value = bats_env_double("BATS_CSLA_MIN_POPULATION", 8.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 0ul;
}


inline size_t
bats_csla_max_population()
{
	const double value = bats_env_double("BATS_CSLA_MAX_POPULATION", 360.0);
	return value > 0.0 && std::isfinite(value)
	       ? static_cast<size_t>(value) : 0ul;
}

inline double
bats_rolling_budget_multiplier()
{
	const double value = bats_env_double("BATS_ROLLING_BUDGET_MULTIPLIER", 1.0);
	return value > 0.0 ? value : 1.0;
}

inline double
bats_rolling_epsilon()
{
	const double value = bats_env_double("BATS_ROLLING_EPSILON", 0.05);
	return value >= 0.0 ? value : 0.05;
}

inline double
bats_rolling_stage_change()
{
	const double value = bats_env_double("BATS_ROLLING_STAGE_CHANGE", 0.50);
	return value >= 0.0 ? value : 0.50;
}


inline std::string
bats_repair_mode()
{
	const char* raw = std::getenv("BATS_REPAIR_MODE");
	if (nullptr == raw || raw[0] == '\0')
		return "fixed";
	return std::string(raw);
}


inline std::string
bats_effort_mode()
{
	const char* raw = std::getenv("BATS_EFFORT_MODE");
	if (nullptr == raw || raw[0] == '\0')
		return "uniform";
	return std::string(raw);
}


inline std::string
bats_sfe_pool_mode()
{
	const char* raw = std::getenv("SFE_POOL_MODE");
	if (nullptr == raw || raw[0] == '\0')
		return "modal";
	const std::string value(raw);
	if (value == "all_success" || value == "adaptive_clf" ||
	    value == "structural_clf")
		return value;
	return "modal";
}


inline double
bats_clf_risk_threshold()
{
	return bats_env_double("CLF_RISK_THRESHOLD", 0.20);
}


inline bool
bats_sfe_pool_diag_enabled()
{
	if (bats_diag_enabled())
		return true;
	const char* raw = std::getenv("SFE_POOL_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}


inline bool
bats_sfe_count_only_enabled()
{
	const char* raw = std::getenv("SFE_COUNT_ONLY");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}


inline bool
bats_sfe_level_agg_diag_enabled()
{
	const char* raw = std::getenv("SFE_LEVEL_AGG_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}


inline bool
bats_rolling_level_opt_needs_event_counts()
{
	return bats_rolling_level_optimization_enabled() || bats_csla_enabled() ||
	       bats_bsrtp_enabled();
}


inline bool
bats_ssta_stage0_weighted_enabled()
{
	const char* raw = std::getenv("BATS_SSTA_STAGE0_WEIGHTED");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_ssta_stage0_diag_enabled()
{
	const char* raw = std::getenv("BATS_SSTA_STAGE0_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}


inline bool
bats_sfe_calibration_diag_enabled()
{
	const char* raw = std::getenv("SFE_CALIBRATION_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}


inline bool
bats_sfe_parent_diag_enabled()
{
	const char* raw = std::getenv("SFE_PARENT_DIAG");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" &&
	       value != "FALSE";
}


inline bool
bats_branch_fk_observe_enabled()
{
	const char* raw = std::getenv("BATS_BRANCH_FK_OBSERVE");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" &&
	       value != "FALSE";
}

inline bool
bats_kbranch_production_enabled()
{
	const char* raw = std::getenv("BATS_KBRANCH");
	if (nullptr == raw)
		raw = std::getenv("BATS_KBRANCH_PRODUCTION");
	if (nullptr == raw) return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}

inline bool
bats_tvcp_enabled()
{
	const char* raw = std::getenv("BATS_TVCP");
	if (nullptr == raw) return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}

inline bool
bats_tvcp_nested_diagnostic_enabled()
{
	const char* raw = std::getenv("BATS_TVCP_NESTED_DIAGNOSTIC");
	if (nullptr == raw) return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}

inline bool
bats_tvcp_diagnostic_only_enabled()
{
	const char* raw = std::getenv("BATS_TVCP_DIAGNOSTIC_ONLY");
	if (nullptr == raw) return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}

inline size_t
bats_tvcp_nested_replicates(const char* name, const size_t defaultValue)
{
	const char* raw = std::getenv(name);
	if (nullptr == raw || raw[0] == '\0') return defaultValue;
	if (raw[0] == '-') throw_FigException(std::string(name) + " must be >=2");
	char* end(nullptr); errno = 0;
	const unsigned long long value = std::strtoull(raw, &end, 10);
	if (errno == ERANGE || nullptr == end || end[0] != '\0' || value < 2ull ||
	    value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
		throw_FigException(std::string(name) + " must be an integer >=2");
	return static_cast<size_t>(value);
}

inline size_t bats_tvcp_nested_outer_replicates()
{
	return bats_tvcp_nested_replicates("BATS_TVCP_NESTED_OUTER", 8ul);
}

inline size_t bats_tvcp_nested_inner_replicates()
{
	return bats_tvcp_nested_replicates("BATS_TVCP_NESTED_INNER", 4ul);
}

inline size_t bats_tvcp_nested_root_replicates()
{
	return bats_tvcp_nested_replicates("BATS_TVCP_NESTED_ROOTS", 64ul);
}

inline bool
bats_tvcp_degenerate_fixed_enabled()
{
	const char* raw = std::getenv("BATS_TVCP_DEGENERATE_FIXED");
	if (nullptr == raw) return false;
	const std::string value(raw);
	return bats_tvcp_enabled() && !value.empty() && value != "0" &&
	       value != "false" && value != "FALSE";
}

inline std::map<unsigned, size_t>
bats_tvcp_population_map()
{
	std::map<unsigned, size_t> result;
	if (!bats_tvcp_enabled()) return result;
	const char* raw = std::getenv("BATS_TVCP_PLAN");
	if (nullptr == raw || raw[0] == '\0')
		throw_FigException("BATS_TVCP requires BATS_TVCP_PLAN=level:population,...");
	std::stringstream input(raw); std::string token;
	while (std::getline(input, token, ',')) {
		const size_t colon=token.find(':');
		if(token.empty()||colon==std::string::npos||colon==0ul||colon+1ul>=token.size())
			throw_FigException("invalid TVCP level:population entry");
		const std::string levelText=token.substr(0,colon), populationText=token.substr(colon+1ul);
		if(levelText[0]=='-'||populationText[0]=='-') throw_FigException("invalid TVCP level:population entry");
		char *levelEnd(nullptr),*populationEnd(nullptr);errno=0;
		const unsigned long long level=std::strtoull(levelText.c_str(),&levelEnd,10);
		const unsigned long long population=std::strtoull(populationText.c_str(),&populationEnd,10);
		if(errno==ERANGE||nullptr==levelEnd||levelEnd[0]!='\0'||nullptr==populationEnd||populationEnd[0]!='\0'||population==0ull||level>std::numeric_limits<unsigned>::max())
			throw_FigException("invalid TVCP level:population entry");
		if(!result.emplace(static_cast<unsigned>(level),static_cast<size_t>(population)).second)
			throw_FigException("duplicate TVCP level");
	}
	if(result.empty()) throw_FigException("empty TVCP plan");
	return result;
}

inline std::vector<unsigned long long>
bats_tvcp_threshold_chain()
{
	std::vector<unsigned long long> result;
	if(!bats_tvcp_enabled()) return result;
	const char* raw=std::getenv("BATS_TVCP_THRESHOLD_CHAIN");
	if(nullptr==raw||raw[0]=='\0') throw_FigException("BATS_TVCP requires BATS_TVCP_THRESHOLD_CHAIN");
	std::stringstream input(raw);std::string token;
	while(std::getline(input,token,',')){
		if(token.empty()||token[0]=='-')throw_FigException("invalid TVCP threshold chain");
		char* end(nullptr);errno=0;const unsigned long long value=std::strtoull(token.c_str(),&end,10);
		if(errno==ERANGE||nullptr==end||end[0]!='\0')throw_FigException("invalid TVCP threshold chain");
		if(!result.empty()&&value<=result.back())throw_FigException("TVCP threshold chain must be strictly increasing");
		result.push_back(value);
	}
	if(result.empty())throw_FigException("empty TVCP threshold chain");return result;
}


inline size_t
bats_branch_fk_keep_k()
{
	const char* raw = std::getenv("BATS_KBRANCH_K");
	if (nullptr == raw)
		raw = std::getenv("BATS_BRANCH_FK_K");
	if (nullptr == raw || raw[0] == '\0')
		return 2ul;
	if (raw[0] == '-')
		throw_FigException("BATS_KBRANCH_K must be a positive integer");
	char* err(nullptr);
	errno = 0;
	const unsigned long long value = std::strtoull(raw, &err, 10);
	if (errno == ERANGE || (nullptr != err && err[0] != '\0') ||
	    value == 0ull ||
	    value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
		throw_FigException("BATS_KBRANCH_K must be a positive integer");
	return static_cast<size_t>(value);
}


inline bool
bats_branch_fk_shadow_plan_enabled()
{
	const char* raw = std::getenv("BATS_BRANCH_FK_SHADOW_PLAN");
	if (nullptr == raw)
		return false;
	const std::string value(raw);
	return !value.empty() && value != "0" && value != "false" &&
	       value != "FALSE";
}


inline void
bats_branch_fk_log(const std::string& msg)
{
	if (bats_branch_fk_observe_enabled())
		ModelSuite::tech_log("BRANCH_FK_OBSERVE " + msg + "\n");
}


inline double
bats_effort_min_mult()
{
	const double value = bats_env_double("BATS_EFFORT_MIN_MULT", 1.0);
	return value > 0.0 ? value : 1.0;
}


inline double
bats_effort_max_mult()
{
	const double value = bats_env_double("BATS_EFFORT_MAX_MULT", 4.0);
	return value > 0.0 ? value : 4.0;
}


inline double
bats_effort_gap_power()
{
	const double value = bats_env_double("BATS_EFFORT_GAP_POWER", 1.0);
	return value > 0.0 ? value : 1.0;
}


inline double
bats_effort_min_p()
{
	const double value = bats_env_double("BATS_EFFORT_MIN_P", 0.02);
	if (value <= 0.0)
		return 0.02;
	if (value >= 0.99)
		return 0.99;
	return value;
}


inline void
bats_diag_log(const std::string& msg)
{
	if (bats_diag_enabled())
		ModelSuite::tech_log("FIG_ISPLIT_DIAG " + msg + "\n");
}


inline void
bats_sfe_pool_diag_log(const std::string& msg)
{
	if (bats_sfe_pool_diag_enabled())
		ModelSuite::tech_log("FIG_ISPLIT_DIAG " + msg + "\n");
}


inline void
bats_sfe_calibration_diag_log(const std::string& msg)
{
	if (bats_sfe_calibration_diag_enabled())
		ModelSuite::tech_log("FIG_ISPLIT_DIAG " + msg + "\n");
}


inline void
bats_sfe_parent_diag_log(const std::string& msg)
{
	if (bats_sfe_parent_diag_enabled())
		ModelSuite::tech_log("FIG_ISPLIT_DIAG " + msg + "\n");
}

} // namespace fig

#endif // BATSDIAGNOSTICS_H
