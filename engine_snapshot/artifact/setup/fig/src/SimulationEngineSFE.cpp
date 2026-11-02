//==============================================================================
//
//  SimulationEngineSFE.cpp
//
//  Copyleft 2017-
//  Authors:
//  - Carlos E. Budde <cbudde@famaf.unc.edu.ar> (Universidad Nacional de Córdoba)
//
//------------------------------------------------------------------------------
//
//  This file is part of FIG.
//
//  The Finite Improbability Generator (FIG) project is free software;
//  you can redistribute it and/or modify it under the terms of the GNU
//  General Public License as published by the Free Software Foundation;
//  either version 3 of the License, or (at your option) any later version.
//
//  FIG is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with FIG; if not, write to the Free Software Foundation,
//	Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//==============================================================================

// C++
#include <unordered_map>
#include <algorithm>   // std::fill(), std::max_element(), std::move()
#include <functional>  // std::bind()
#include <sstream>
#include <cmath>
#include <cctype>
#include <chrono>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <set>
#include <unordered_set>
#include <atomic>
// FIG
#include <SimulationEngineSFE.h>
#include <ThresholdsBuilderAdaptive.h>
#include <PropertyTransient.h>
#include <TraialPool.h>
#include <ModelSuite.h>
#include <BatsDiagnostics.h>
#include <SequentialStateAllocation.h>
#include <AdaptiveClfDecision.h>
#include <StructuralClfDecision.h>
#include <WeightedSfeKernel.h>
#include <BranchFkObservation.h>
#include <BranchFkPlanner.h>
#include <KBranchEstimator.h>
#include <KBranchFrontier.h>
#include <TVCPPlanner.h>
#include <TVCPLevelMap.h>
#include <TVCPNestedEstimator.h>
#include <Clock.h>
#include <ModelSuite.h>

// ADL
using std::begin;
using std::end;


namespace   // // // // // // // // // // // // // // // // // // // // // // //
{

using fig::Traial;
using fig::ImportanceValue;
using fig::ImportanceFunction;
typedef std::vector< fig::Reference< Traial > > TraialVec;
typedef fig::SimulationEngine::ReachabilityCount ReachabilityCount;

std::atomic<unsigned long long> parentDiagRunCounter(0ull);
std::atomic<unsigned long long> parentDiagInvocationCounter(0ull);

std::string
next_parent_diag_run_id()
{
	const char* configured = std::getenv("SFE_DIAGNOSTIC_RUN_ID");
	if (configured != nullptr && configured[0] != '\0')
		return std::string(configured);
	std::ostringstream out;
	out << "sfe-diag-" << parentDiagRunCounter.fetch_add(1ull);
	return out.str();
}

thread_local std::string activeParentDiagRunId;
thread_local size_t activeParentDiagBatchId = 0ul;
thread_local std::string activeFixedEffortInvocationId;
thread_local std::string completedFixedEffortInvocationId;

std::string
next_fixed_effort_invocation_id()
{
	const auto now = std::chrono::high_resolution_clock::now()
		.time_since_epoch().count();
	const auto serial = parentDiagInvocationCounter.fetch_add(1ull);
	std::ostringstream out;
	out << (activeParentDiagRunId.empty() ? "sfe-diag" : activeParentDiagRunId)
	    << "-fe-" << now << "-" << serial;
	return out.str();
}


bool
oracle_env_flag(const char* name)
{
	const char* value = std::getenv(name);
	if (nullptr == value)
		return false;
	const std::string text(value);
	return text == "1" || text == "true" || text == "TRUE" ||
	       text == "yes" || text == "YES";
}


std::string
oracle_env(const char* name, const std::string& fallback = "")
{
	const char* value = std::getenv(name);
	return nullptr == value ? fallback : std::string(value);
}


size_t
oracle_env_size(const char* name, size_t fallback)
{
	const std::string value = oracle_env(name);
	if (value.empty())
		return fallback;
	try {
		return static_cast<size_t>(std::stoull(value));
	} catch (...) {
		return fallback;
	}
}


double
oracle_env_double(const char* name, double fallback)
{
	const std::string value = oracle_env(name);
	if (value.empty())
		return fallback;
	try {
		return std::stod(value);
	} catch (...) {
		return fallback;
	}
}


std::string
oracle_safe_text(std::string value)
{
	for (auto& ch: value)
		if (ch == ',' || ch == '\n' || ch == '\r')
			ch = ';';
	return value;
}


std::string
oracle_file_token(std::string value)
{
	for (auto& ch: value)
		if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_')
			ch = '_';
	return value;
}


std::string
oracle_state_values(const Traial& traial,
	                const std::vector<std::string>& names)
{
	std::ostringstream out;
	for (size_t i = 0ul; i < traial.state.size(); ++i) {
		if (i > 0ul)
			out << "|";
		out << (i < names.size() ? names[i] : std::to_string(i))
		    << "=" << traial.state[i];
	}
	return out.str();
}


std::string
oracle_path_signature(const Traial& traial,
	                  const std::vector<std::string>& names)
{
	std::ostringstream out;
	bool first = true;
	for (size_t i = 0ul; i < traial.state.size() && i < names.size(); ++i) {
		const std::string& name(names[i]);
		if (name.find("_broken") == std::string::npos &&
		    name.find("_count") == std::string::npos &&
		    name.find("_st") == std::string::npos)
			continue;
		if (!first)
			out << "|";
		first = false;
		out << name << "=" << traial.state[i];
	}
	return first ? oracle_state_values(traial, names) : out.str();
}


std::string
fb_ssta_state_signature(const Traial& traial,
	                    const std::vector<std::string>& names,
	                    const ImportanceValue level)
{
	std::ostringstream out;
	out << "L=" << level;
	size_t failedCount = 0ul;
	size_t emittedFailed = 0ul;
	for (size_t i = 0ul; i < traial.state.size() && i < names.size(); ++i) {
		const std::string& name(names[i]);
		if (fig::ssta_has_suffix(name, "_st")) {
			out << "|" << name << "=" << traial.state[i];
		} else if (name.find("_broken") != std::string::npos &&
		           traial.state[i] != 0) {
			++failedCount;
			if (emittedFailed < 3ul) {
				out << "|F=" << name;
				++emittedFailed;
			}
		}
	}
	out << "|FC=" << std::min<size_t>(failedCount, 3ul);
	// Coarse remaining-horizon bin.  It is deliberately based only on the
	// elapsed trajectory time, so STR and RDI share identical group semantics.
	const double elapsed = std::max(0.0, static_cast<double>(traial.lifeTime));
	const size_t timeBin = elapsed < 1.0 ? 0ul
		: (elapsed < 10.0 ? 1ul : (elapsed < 100.0 ? 2ul : 3ul));
	out << "|TB=" << timeBin;
	return out.str();
}


std::string
oracle_clock_values(const Traial& traial)
{
	const auto clocks(fig::TraialPool::get_timeouts(traial));
	std::ostringstream out;
	out << std::setprecision(9);
	for (size_t i = 0ul; i < clocks.size(); ++i) {
		if (i > 0ul)
			out << "|";
		out << clocks[i].gpos << ":" << clocks[i].name << ":"
		    << clocks[i].sampled << ":" << clocks[i].value;
	}
	return out.str();
}


std::string
oracle_full_signature(const Traial& traial,
	                  const std::vector<std::string>& names)
{
	std::ostringstream out;
	out << oracle_state_values(traial, names) << "#"
	    << oracle_clock_values(traial) << "#"
	    << std::setprecision(12) << traial.lifeTime;
	return out.str();
}


bool
oracle_same_timeouts(const Traial& left, const Traial& right)
{
	const auto lhs(fig::TraialPool::get_timeouts(left));
	const auto rhs(fig::TraialPool::get_timeouts(right));
	if (lhs.size() != rhs.size())
		return false;
	for (size_t i = 0ul; i < lhs.size(); ++i)
		if (lhs[i].gpos != rhs[i].gpos || lhs[i].name != rhs[i].name ||
		    lhs[i].sampled != rhs[i].sampled || lhs[i].value != rhs[i].value)
			return false;
	return true;
}


std::pair<double,double>
oracle_wilson_interval(size_t hits, size_t trials)
{
	if (0ul == trials)
		return std::make_pair(0.0, 1.0);
	const double z = 1.96;
	const double n = static_cast<double>(trials);
	const double p = static_cast<double>(hits) / n;
	const double z2 = z*z;
	const double denominator = 1.0 + z2/n;
	const double center = (p + z2/(2.0*n)) / denominator;
	const double half = z * std::sqrt((p*(1.0-p) + z2/(4.0*n))/n) / denominator;
	return std::make_pair(std::max(0.0, center-half),
	                      std::min(1.0, center+half));
}


typedef std::unordered_map<std::string, std::vector<unsigned long> > OracleSeedPlan;


OracleSeedPlan
oracle_load_seed_plan(const std::string& path)
{
	std::ifstream input(path.c_str());
	if (!input)
		throw_FigException("cannot open N4Q oracle seed plan: " + path);
	OracleSeedPlan plan;
	std::string line;
	while (std::getline(input, line)) {
		std::istringstream row(line);
		std::string stateId, repText, continuationSeed;
		if (!std::getline(row, stateId, ',') ||
		    !std::getline(row, repText, ',') ||
		    !std::getline(row, continuationSeed, ','))
			continue;
		try {
			const size_t rep = static_cast<size_t>(std::stoull(repText));
			const unsigned long seed = static_cast<unsigned long>(std::stoull(continuationSeed));
			if (0ul == rep || 0ul == seed)
				continue;
			auto& seeds(plan[stateId]);
			if (seeds.size() < rep)
				seeds.resize(rep, 0ul);
			seeds[rep-1ul] = seed;
		} catch (...) {
			// Header or malformed row: ignored; completeness is checked per state.
		}
	}
	return plan;
}


bool
oracle_claim_run(const std::string& key)
{
	static std::set<std::string> claimed;
	return claimed.insert(key).second;
}


struct OracleContinuationOutcome
{
	bool ok;
	bool hit;
	bool initialPreserved;
	size_t resampledClockCount;
	double lifeAfter;
	std::string finalSignature;
	std::string status;
	OracleContinuationOutcome() :
		ok(false), hit(false), initialPreserved(false), resampledClockCount(0ul), lifeAfter(0.0),
		finalSignature(), status("not_run") {}
};


struct ClfOracleSampleMeta
{
	size_t poolDecision;
	size_t candidateIndex;
	size_t stratum;
	ImportanceValue level;
	ImportanceValue candidateReachedValue;
	ImportanceValue modalReachedValue;
	bool retained;
	bool rare;
	size_t candidateCount;
	size_t modalCount;
	size_t groupCount;
	size_t groupCountTotal;
	size_t overshootCount;
	double candidateWeight;
	double groupWeight;
	double allWeight;
	double keepWeight;
	double dropWeight;
	std::string thresholdLow;
	std::string thresholdHigh;
	std::string configuredPoolMode;
	std::string effectivePoolMode;
	std::string pathSignature;
};


struct SaclfOracleCandidateMeta
{
	size_t poolDecision;
	size_t candidateIndex;
	size_t stratum;
	ImportanceValue level;
	ImportanceValue candidateReachedValue;
	ImportanceValue modalReachedValue;
	bool retained;
	bool rare;
	size_t candidateCount;
	size_t modalCount;
	size_t reachedGroupCount;
	size_t reachedGroupTotal;
	size_t structuralSignatureCount;
	size_t modalSignatureCount;
	size_t discardedSignatureCount;
	size_t novelDiscardedSignatureCount;
	bool structuralOpportunity;
	bool saclfActivated;
	size_t overshootCount;
	double candidateWeight;
	double candidateQuality;
	double inclusionProbability;
	size_t frameSize;
	size_t stratumSize;
	size_t sampleSize;
	std::string thresholdLow;
	std::string thresholdHigh;
	std::string configuredPoolMode;
	std::string effectivePoolMode;
	std::string reachedHist;
	std::string structuralSignature;
};


struct ClfAtlasCandidateMeta
{
	size_t poolDecision;
	size_t candidateIndex;
	size_t side;
	size_t stratum;
	ImportanceValue level;
	ImportanceValue candidateReachedValue;
	ImportanceValue modalReachedValue;
	bool retained;
	bool rare;
	size_t candidateCount;
	size_t modalCount;
	size_t reachedGroupCount;
	size_t reachedGroupTotal;
	size_t pathSignatureCount;
	size_t modalSignatureCount;
	size_t discardedSignatureCount;
	size_t novelDiscardedSignatureCount;
	size_t overshootCount;
	size_t overshootLayer;
	size_t frameSize;
	size_t stratumSize;
	size_t sampleSize;
	double inclusionProbability;
	double candidateWeight;
	double allWeight;
	double modalWeight;
	double discardedWeight;
	std::string thresholdLow;
	std::string thresholdHigh;
	std::string configuredPoolMode;
	std::string effectivePoolMode;
	std::string reachedHist;
	std::string pathSignature;
	std::string ancestorGate;
};


struct ClfAtlasPathMeta
{
	size_t poolDecision;
	ImportanceValue level;
	std::string pathSignature;
	std::string ancestorGate;
	size_t count;
	size_t modalCount;
	size_t discardedCount;
	size_t maxOvershootLayer;
	double totalWeight;
};

/**
 * @brief Select the most likely "next step value" from several possibilities
 *
 *        The vector given as argument contains several Traial instances
 *        that could be the next step of a Fixed Effort sweep.
 *        Check which threshold/importance value occurs most often,
 *        and delete from the vector any Traial with different value.
 *
 * @param traials    Traials to analyse and filter
 * @param reachCount How often each importance/threshold was reached,
 *                   from among the values that occurr in \p traials
 *
 * @return Next step value, viz. ImportanceValue of the chosen (surviving) Traials
 */
ImportanceValue
filter_reached_value(TraialVec& traials,
				 const ImportanceValue nextValue)
{
    assert(!traials.empty());
	TraialVec chosenTraials;
    const auto N(traials.size());
    chosenTraials.reserve(N);
	// Filter the traials vectors
    for (auto i = 0ul ; i < N ; i++) {
        Traial& t(traials.back());
        traials.pop_back();
		if (t.level == nextValue)
			chosenTraials.push_back(t);
        else
			fig::TraialPool::get_instance().return_traial(std::move(t));
    }
	assert(traials.empty());
	assert(!chosenTraials.empty());
	assert(chosenTraials.size() <= N);
	chosenTraials.swap(traials);
	return nextValue;
}


ImportanceValue
filter_next_value(TraialVec& traials,
				  const ReachabilityCount& reachCount)
{
	assert(!reachCount.empty());
	using pair_t = std::remove_reference<decltype(reachCount)>::type::value_type;
	// Find most frequent ImportanceValue. ReachabilityCount is ordered by
	// level, so std::max_element retains the lower level on a count tie.
	auto nextStep = std::max_element(begin(reachCount), end(reachCount),
	                                 [](const pair_t& p1, const pair_t& p2)
	                                 { return p1.second < p2.second; });
	return filter_reached_value(traials, nextStep->first);
}


std::string
reach_hist_to_string(const ReachabilityCount& reachCount)
{
	std::ostringstream out;
	bool first = true;
	for (const auto& item: reachCount) {
		if (!first)
			out << ",";
		first = false;
		out << item.first << ":" << item.second;
	}
	return first ? "-" : out.str();
}


std::string
size_vector_to_string(const std::vector<size_t>& values)
{
	std::ostringstream out;
	for (size_t i = 0ul; i < values.size(); ++i) {
		if (i > 0ul)
			out << ",";
		out << values[i];
	}
	return values.empty() ? "-" : out.str();
}


std::string
parent_reach_hist_to_string(const std::vector<ReachabilityCount>& values)
{
	std::ostringstream out;
	for (size_t i = 0ul; i < values.size(); ++i) {
		if (i > 0ul)
			out << "|";
		out << reach_hist_to_string(values[i]);
	}
	return values.empty() ? "-" : out.str();
}


template <typename Thresholds>
std::string
threshold_chain_to_string(const Thresholds& thresholds)
{
	std::ostringstream out;
	bool first = true;
	for (const auto& item: thresholds) {
		if (!first)
			out << ",";
		first = false;
		out << item.first;
	}
	return first ? "-" : out.str();
}


std::string
atlas_ancestor_gate(const std::string& signature)
{
	if (signature.empty() || signature == "-")
		return "-";
	const size_t stop = signature.find_first_of("=|");
	std::string token = stop == std::string::npos
	                    ? signature : signature.substr(0ul, stop);
	const char* suffixes[] = {"_broken", "_count", "_st"};
	for (const auto suffix: suffixes) {
		const std::string text(suffix);
		if (token.size() >= text.size() &&
		    0 == token.compare(token.size()-text.size(), text.size(), text))
			return token.substr(0ul, token.size()-text.size());
	}
	return token;
}


size_t
sum_reach_count(const ReachabilityCount& reachCount)
{
	size_t total = 0ul;
	for (const auto& item: reachCount)
		total += item.second;
	return total;
}


ImportanceValue
max_reached_value(const ReachabilityCount& reachCount)
{
	if (reachCount.empty())
		return 0u;
	return std::max_element(
		begin(reachCount),
		end(reachCount),
		[](const ReachabilityCount::value_type& lhs,
		   const ReachabilityCount::value_type& rhs)
			{ return lhs.first < rhs.first; })->first;
}


struct SstaParentGroup
{
	std::string signature;
	std::vector<size_t> parentIndices;
	std::vector<double> parentWeights;
	double mass;
	fig::SstaAdvanceStats history;
	fig::FbSstaOutcomeStats fbHistory;
	fig::FbSstaEstimates fbEstimates;
	double advanceProbability;
	double score;
	size_t allocatedChildren;

	SstaParentGroup() :
		signature(),
		parentIndices(),
		parentWeights(),
		mass(0.0),
		history(),
		fbHistory(),
		fbEstimates(),
		advanceProbability(0.5),
		score(0.0),
		allocatedChildren(0ul)
	{}
};


struct SstaChildSpec
{
	std::string signature;
	double mass;

	SstaChildSpec(const std::string& groupSignature, const double childMass) :
		signature(groupSignature),
		mass(childMass)
	{}
};


struct SstaGroupBatchOutcome
{
	size_t trials;
	size_t advanced;
	size_t successes;
	double advancedMass;
	double successMass;
	size_t eventTransitions;
	std::map<ImportanceValue,size_t> reachedCounts;
	std::map<ImportanceValue,double> reachedMasses;

	SstaGroupBatchOutcome() :
		trials(0ul),
		advanced(0ul),
		successes(0ul),
		advancedMass(0.0),
		successMass(0.0),
		eventTransitions(0ul),
		reachedCounts(),
		reachedMasses()
	{}
};


void
normalise_success_pool_to_next_level(TraialVec& traials,
                                     const ImportanceValue& nextLvl)
{
	for (auto& traialRef: traials) {
		Traial& traial(traialRef.get());
		traial.level = nextLvl;
		traial.depth = 0;
		traial.numLevelsCrossed = 0;
		traial.nextSplitLevel = static_cast<int>(nextLvl) + 1;
	}
}

} // namespace   // // // // // // // // // // // // // // // // // // // // //


namespace fig  // // // // // // // // // // // // // // // // // // // // // //
{

SimulationEngineSFE::SimulationEngineSFE(
    std::shared_ptr<const ModuleNetwork> model,
    const bool thresholds) :
        SimulationEngineFixedEffort("sfe", model, thresholds),
        adaptivePopulationPlan_(),
        rollingPopulationConfig_(),
        rollingPopulationPlan_(),
        rollingObservations_(),
        rollingPreviousPopulations_(),
        rollingRemainingBudget_(0.0),
        rollingBaselineBudget_(0.0),
        rollingStageIndex_(0ul),
		vcstpProblem_(),
		vcstpPlan_(),
		vcstpBatchObservations_(),
		vcstpBatchIndex_(0ul),
		vcstpCurrentBatchTrajectories_(0ul),
		vcstpFixedCurrentBatchTrajectories_(0ul),
		vcstpCumulativeTrajectories_(0ul),
		vcstpFixedCumulativeTrajectories_(0ul),
		vcstpLastSolveMs_(0.0),
		vcstpSolveMsTotal_(0.0),
		vcstpPlanCount_(0ul),
		vcstpOptimizedCount_(0ul),
		vcstpFallbackCount_(0ul),
		vcstpVarianceGuardCount_(0ul),
		vcstpCostReductionCount_(0ul),
		vcstpAbsoluteRiskTargetCount_(0ul),
		bsrtpConfig_(),
		bsrtpHistory_(),
		bsrtpBatchHistory_(),
		bsrtpPlan_(),
		bsrtpPreviousPopulations_(),
		bsrtpReferencePopulations_(),
		bsrtpRunIndex_(0ul),
		bsrtpReplanCount_(0ul),
		bsrtpFallbackCount_(0ul),
		bsrtpActualEventTransitions_(0ul),
		bsrtpActualTrajectories_(0ul),
		bsrtpPredictedExtinctionSum_(0.0),
		bsrtpExtinctionObservationCount_(0ul),
		bsrtpExtinctionCount_(0ul),
		bsrtpPoolExtinctionObservationCount_(0ul),
		bsrtpPoolExtinctionCount_(0ul),
		bsrtpRiskDirectionPairCount_(0ul),
		bsrtpRiskDirectionAgreementCount_(0ul),
		cslaConfig_(),
		cslaHistory_(),
		cslaPlan_(),
		cslaFixedPopulations_(),
		cslaPreviousPopulations_(),
		cslaBatchIndex_(0ul),
		cslaActualEventTransitions_(0ul),
		cslaRunBudget_(0.0),
		cslaPlanningBudget_(0.0),
		cslaRemainingBudget_(0.0),
		xiwangOnlineRuntime_(),
		adaptivePopulationCalibrationPhase_(false),
		sequentialStateHistory_(),
		fbSequentialStateHistory_(),
		sequentialStateRunIndex_(0ul)
{ /* Not much to do around here */ }


SimulationEngineSFE::~SimulationEngineSFE()
{
    //TraialPool::get_instance().return_traials(traials_);
    // ^^^ pointless, and besides the TraialPool might be dead already,
    //     so this would trigger a re-creation of the pool or something worse
}

void
SimulationEngineSFE::reset() const
{
	SimulationEngineFixedEffort::reset();
	adaptivePopulationPlan_.reset();
	rollingPopulationPlan_ = RollingLevelPopulationPlan();
	rollingObservations_.clear();
	rollingPreviousPopulations_.clear();
	rollingRemainingBudget_ = 0.0;
	rollingBaselineBudget_ = 0.0;
	rollingStageIndex_ = 0ul;
	vcstpProblem_ = VcstpProblem();
	vcstpPlan_ = VcstpPlan();
	vcstpBatchObservations_.clear();
	vcstpBatchIndex_ = 0ul;
	vcstpCurrentBatchTrajectories_ = 0ul;
	vcstpFixedCurrentBatchTrajectories_ = 0ul;
	vcstpCumulativeTrajectories_ = 0ul;
	vcstpFixedCumulativeTrajectories_ = 0ul;
	vcstpLastSolveMs_ = 0.0;
	vcstpSolveMsTotal_ = 0.0;
	vcstpPlanCount_ = 0ul;
	vcstpOptimizedCount_ = 0ul;
	vcstpFallbackCount_ = 0ul;
	vcstpVarianceGuardCount_ = 0ul;
	vcstpCostReductionCount_ = 0ul;
	vcstpAbsoluteRiskTargetCount_ = 0ul;
	bsrtpHistory_.reset();
	bsrtpBatchHistory_.reset();
	bsrtpPlan_ = BsrtpPlan();
	bsrtpPreviousPopulations_.clear();
	bsrtpReferencePopulations_.clear();
	bsrtpRunIndex_ = 0ul;
	bsrtpReplanCount_ = 0ul;
	bsrtpFallbackCount_ = 0ul;
	bsrtpActualEventTransitions_ = 0ul;
	bsrtpActualTrajectories_ = 0ul;
	bsrtpPredictedExtinctionSum_ = 0.0;
	bsrtpExtinctionObservationCount_ = 0ul;
	bsrtpExtinctionCount_ = 0ul;
	bsrtpPoolExtinctionObservationCount_ = 0ul;
	bsrtpPoolExtinctionCount_ = 0ul;
	bsrtpRiskDirectionPairCount_ = 0ul;
	bsrtpRiskDirectionAgreementCount_ = 0ul;
	cslaPlan_ = CslaPlan();
	cslaFixedPopulations_.clear();
	cslaPreviousPopulations_.clear();
	cslaBatchIndex_ = 0ul;
	cslaActualEventTransitions_ = 0ul;
	cslaRunBudget_ = 0.0;
	cslaPlanningBudget_ = 0.0;
	cslaRemainingBudget_ = 0.0;
	xiwangOnlineRuntime_.reset();
	cslaHistory_.reset(impFun_ ? impFun_->num_thresholds() + 1ul : 0ul);
	for (const auto& item: cslaHistory_.observations())
		assert(item.trials == 0ul && item.successes == 0ul &&
		       item.retained == 0ul && item.event_transitions == 0ul);
	adaptivePopulationCalibrationPhase_ = false;
	sequentialStateHistory_.reset();
	fbSequentialStateHistory_.reset();
	sequentialStateRunIndex_ = 0ul;
}

std::vector<double>
SimulationEngineSFE::transient_simulations(
	const PropertyTransient& property,
	const size_t& numRuns) const
{
	std::vector<double> results(numRuns);
	ThresholdsPathCandidates paths;

	if (toBuildThresholds_)
		throw_FigException("adaptive SFE transient simulations cannot build thresholds");

	if (nullptr == property_ || property_->get_id() != property.get_id())
		property_ = &property;

	const bool rolling = bats_rolling_level_optimization_enabled();
	const bool vcstp = bats_vcstp_enabled();
	const bool csla = bats_csla_enabled();
	const bool bsrtp = bats_bsrtp_enabled();
	const bool xiwang = !bats_xiwang_plan().empty();
	const bool xiwangOnline = xiwangOnlineRuntime_.enabled();
	const bool adaptive = bats_adaptive_level_population_enabled() || rolling || bsrtp;
	if (xiwangOnline &&
	    (xiwang || adaptive || vcstp || csla ||
	     bats_level_population() > 0ul ||
	     bats_kbranch_production_enabled() ||
	     oracle_env_flag("BATS_SSTA_STATE_ALLOCATION") ||
	     oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION")))
		throw_FigException(
			"BATS_XIWANG_ONLINE cannot be combined with another population controller");
	if (xiwangOnline && bats_sfe_pool_mode() != "modal")
		throw_FigException("BATS_XIWANG_ONLINE requires SFE_POOL_MODE=modal");
	if (xiwang && (adaptive || vcstp || csla ||
	              bats_level_population() > 0ul ||
	              oracle_env_flag("BATS_SSTA_STATE_ALLOCATION") ||
	              oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION")))
		throw_FigException(
			"BATS_XIWANG_PLAN cannot be combined with another population controller");
	if (bsrtp && (rolling || vcstp || csla ||
	              oracle_env_flag("BATS_SSTA_STATE_ALLOCATION") ||
	              oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION")))
		throw_FigException(
			"BSRTP cannot be combined with ROLL, VCSTP, CSLA, or SSTA");
	if (bsrtp && bats_level_population() > 0ul)
		throw_FigException(
			"BSRTP cannot be combined with a fixed population override");
	if (bsrtp && bats_sfe_pool_mode() != "modal")
		throw_FigException("BSRTP requires SFE_POOL_MODE=modal");
	const bool fbSstaRequestedAtSimulation =
		oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION");
	if (fbSstaRequestedAtSimulation &&
	    (adaptive || csla || bats_level_population() > 0ul))
		throw_FigException(
			"FB-SSTA requires the original Fixed level population without ALP, CSLA, or population overrides");
	if (csla && rolling)
		throw_FigException("BATS_CSLA cannot be combined with BATS_ROLLING_LEVEL_OPT");
	if (vcstp && (rolling || csla || bats_adaptive_level_population_enabled() ||
	              bats_level_population() > 0ul ||
	              oracle_env_flag("BATS_SSTA_STATE_ALLOCATION") ||
	              oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION")))
		throw_FigException(
			"BATS_VCSTP cannot be combined with ROLL, CSLA, adaptive/fixed population overrides, or SSTA");
	if (vcstp && bats_sfe_pool_mode() != "modal")
		throw_FigException("BATS_VCSTP requires SFE_POOL_MODE=modal");
	if (csla && bats_adaptive_level_population_enabled())
		throw_FigException("BATS_CSLA cannot be combined with adaptive level population");
	if (csla && oracle_env_flag("BATS_SSTA_STATE_ALLOCATION"))
		throw_FigException("BATS_CSLA cannot be combined with SSTA state allocation");
	if (csla && bats_sfe_pool_mode() != "modal")
		throw_FigException("BATS_CSLA requires SFE_POOL_MODE=modal");
	if (rolling && !bats_adaptive_level_population_enabled())
		throw_FigException(
			"BATS_ROLLING_LEVEL_OPT requires BATS_ADAPTIVE_LEVEL_POPULATION=1");
	if (rolling && oracle_env_flag("BATS_SSTA_STATE_ALLOCATION"))
		throw_FigException(
			"BATS_ROLLING_LEVEL_OPT cannot be combined with SSTA state allocation");
	if (adaptive && bats_level_population() > 0ul)
		throw_FigException(
			"BATS_ADAPTIVE_LEVEL_POPULATION and BATS_LEVEL_POPULATION are mutually exclusive");
	if (adaptive && bats_sfe_pool_mode() != "modal")
		throw_FigException(
			"BATS_ADAPTIVE_LEVEL_POPULATION requires SFE_POOL_MODE=modal");

	if (csla) {
		const size_t levelCount = impFun_->num_thresholds() + 1ul;
		if (cslaFixedPopulations_.empty()) {
			cslaFixedPopulations_.assign(levelCount, 0ul);
			for (size_t level = 0ul; level < levelCount; ++level) {
				const size_t effort = std::max<size_t>(
					lvl_effort_min(), impFun_->effort_of(level));
				const size_t nominal = effort * base_nsims();
				cslaFixedPopulations_[level] = nominal;
			}
			cslaPreviousPopulations_ = cslaFixedPopulations_;
			cslaConfig_.min_population = bats_csla_min_population();
			cslaConfig_.max_population = bats_csla_max_population();
				cslaConfig_.min_samples = bats_csla_min_samples();
				cslaConfig_.max_batch_change = bats_csla_max_batch_change();
				cslaConfig_.max_extinction_risk = bats_csla_max_extinction_risk();
			const double eta = bats_csla_budget_eta();
			const double fixedEventBudget = bats_csla_fixed_event_budget();
			if (cslaConfig_.min_population == 0ul ||
			    cslaConfig_.max_population < cslaConfig_.min_population ||
			    cslaConfig_.min_samples == 0ul ||
			    cslaConfig_.max_batch_change < 0.0 ||
			    cslaConfig_.max_batch_change > 1.0 ||
			    eta <= 0.0 || eta > 1.0 || fixedEventBudget <= 0.0)
				throw_FigException("invalid CSLA configuration");
			cslaRunBudget_ = fixedEventBudget;
			cslaPlanningBudget_ = eta * fixedEventBudget;
			cslaRemainingBudget_ = cslaRunBudget_;
		}
	}

	if (vcstp && vcstpProblem_.fixed_populations.empty()) {
		const size_t levelCount = impFun_->num_thresholds() + 1ul;
		vcstpProblem_.observations.assign(levelCount, VcstpObservation());
		vcstpProblem_.fixed_populations.assign(levelCount, 0ul);
		for (size_t level = 0ul; level < levelCount; ++level) {
			const size_t effort = std::max<size_t>(
				lvl_effort_min(), impFun_->effort_of(level));
			vcstpProblem_.fixed_populations[level] = effort * base_nsims();
		}
		vcstpProblem_.previous_populations = vcstpProblem_.fixed_populations;
		vcstpProblem_.config.epsilon = bats_vcstp_epsilon();
		vcstpProblem_.config.kappa = bats_vcstp_kappa();
		vcstpProblem_.config.delta_path = bats_vcstp_delta_path();
		vcstpProblem_.config.min_samples = bats_vcstp_min_samples();
		vcstpProblem_.config.min_population = bats_vcstp_min_population();
		vcstpProblem_.config.max_population = bats_vcstp_max_population();
		vcstpProblem_.config.protect_fixed_variance =
			bats_vcstp_protect_fixed_variance_enabled();
		vcstpProblem_.config.require_strict_cost_reduction =
			bats_vcstp_require_cost_reduction_enabled();
		vcstpProblem_.config.minimize_event_cost_under_fixed_variance =
			bats_vcstp_v3_enabled();
		vcstpProblem_.config.variance_tolerance =
			bats_vcstp_variance_tolerance();
		vcstpProblem_.config.replan_interval =
			bats_vcstp_replan_interval();
		if (!vcstp_config_valid(vcstpProblem_.config))
			throw_FigException("invalid VCSTP configuration");
	}

	if (rolling && !rollingPopulationPlan_.populations.empty() &&
	    rollingStageIndex_ > 0ul) {
		std::vector<size_t> fallback = rollingPreviousPopulations_;
		if (fallback.empty())
			fallback.assign(impFun_->thresholds().size(),
								adaptivePopulationPlan_.config().reference_population);
		const size_t plannedLevels = impFun_->thresholds().size();
		std::vector<RollingLevelObservation> planObservations(
			rollingObservations_.begin(),
			rollingObservations_.begin() +
				std::min(rollingObservations_.size(), plannedLevels));
		const double runs = static_cast<double>(std::max<size_t>(1ul, numRuns));
		const double perRunCap = rollingBaselineBudget_ > 0.0
			? rollingBaselineBudget_ * bats_rolling_budget_multiplier()
			: (rollingRemainingBudget_ / runs);
		const double stageBudget = std::min(rollingRemainingBudget_ / runs,
		                                    perRunCap);
		const RollingLevelPopulationPlan replanned = rolling_plan_levels(
			planObservations, fallback, stageBudget, rollingPopulationConfig_,
			&rollingPreviousPopulations_);
		if (replanned.valid || replanned.fallback) {
			rollingPopulationPlan_ = replanned;
			if (!rollingPopulationPlan_.populations.empty())
				rollingPreviousPopulations_ = rollingPopulationPlan_.populations;
		}
		++rollingStageIndex_;
		std::ostringstream rollingLog;
		rollingLog << "ROLLING_PLAN stage=" << rollingStageIndex_
		           << " budget_per_run=" << stageBudget
		           << " remaining_budget=" << rollingRemainingBudget_
		           << " predicted_cost=" << rollingPopulationPlan_.predicted_cost
		           << " proxy=" << rollingPopulationPlan_.proxy_value
		           << " valid=" << (rollingPopulationPlan_.valid ? 1 : 0)
		           << " fallback=" << (rollingPopulationPlan_.fallback ? 1 : 0)
		           << " reason=" << rollingPopulationPlan_.reason
		           << " populations=";
		for (size_t level = 0ul;
		     level < rollingPopulationPlan_.populations.size(); ++level) {
			if (level > 0ul)
				rollingLog << ";";
			rollingLog << level << ":"
			           << rollingPopulationPlan_.populations[level];
		}
		rollingLog << " obs=";
		for (size_t level = 0ul; level < planObservations.size(); ++level) {
			if (level > 0ul)
				rollingLog << ";";
			const RollingLevelObservation& o(planObservations[level]);
			rollingLog << level << ":" << o.trials << "," << o.successes
			           << "," << o.retained << "," << o.event_transitions;
		}
		ModelSuite::tech_log(rollingLog.str() + "\n");
	}

	if (adaptive && !adaptivePopulationPlan_.finalized() && !rolling) {
		adaptivePopulationCalibrationPhase_ = true;
		try {
			for (size_t sweep = 0ul;
			     sweep < bats_adaptive_calibration_sweeps();
			     ++sweep) {
				ThresholdsPathCandidates calibration;
				fixed_effort(calibration, get_event_watcher(property));
			}
		} catch (...) {
			adaptivePopulationCalibrationPhase_ = false;
			throw;
		}
		adaptivePopulationCalibrationPhase_ = false;
		const std::string alpConfig = bats_alp_config();
		if (bsrtp) {
			bsrtpConfig_.min_population = bats_bsrtp_min_population();
			bsrtpConfig_.max_population = bats_bsrtp_max_population();
			bsrtpConfig_.reference_population =
				bats_bsrtp_reference_population();
			bsrtpConfig_.budget_multiplier =
				bats_bsrtp_budget_multiplier();
			bsrtpConfig_.risk_relaxation = bats_bsrtp_risk_relaxation();
			bsrtpConfig_.max_change_share = bats_bsrtp_max_change();
			bsrtpConfig_.history_decay = bats_bsrtp_history_decay();
			bsrtpConfig_.replan_interval = bats_bsrtp_replan_interval();
			validate_bsrtp_config(bsrtpConfig_);
			const size_t levelCount = impFun_->thresholds().size();
			bsrtpReferencePopulations_.assign(levelCount, 0ul);
			for (size_t level = 0ul; level < levelCount; ++level) {
				const size_t effort = std::max<size_t>(
					lvl_effort_min(), impFun_->effort_of(level));
				bsrtpReferencePopulations_[level] = effort * base_nsims();
			}
			bsrtpHistory_.reset(levelCount);
			for (size_t level = 0ul; level < levelCount; ++level)
				bsrtpHistory_.observe(
					level, adaptivePopulationPlan_.trials(level),
					adaptivePopulationPlan_.successes(level),
					adaptivePopulationPlan_.retained(level),
					adaptivePopulationPlan_.rare_only_successes(level),
					adaptivePopulationPlan_.event_transitions(level));
			bsrtpBatchHistory_.reset(levelCount);
			bsrtpPlan_ = bsrtp_plan(
				bsrtpHistory_.observations(), {},
				bsrtpReferencePopulations_, bsrtpConfig_);
			if (!bsrtpPlan_.valid)
				throw_FigException(
					"BSRTP initial planning failed: " + bsrtpPlan_.reason);
			bsrtpPreviousPopulations_ = bsrtpPlan_.populations;
			++bsrtpReplanCount_;
			std::ostringstream initLog;
			initLog << std::setprecision(17)
			        << "BSRTP_INIT levels=" << levelCount
			        << " budget=" << bsrtpPlan_.budget
			        << " optimal_worst_risk="
			        << bsrtpPlan_.optimal_worst_risk
			        << " final_worst_risk=" << bsrtpPlan_.final_worst_risk
			        << " predicted_cost=" << bsrtpPlan_.predicted_cost
			        << " populations=";
			for (size_t level = 0ul; level < levelCount; ++level) {
				if (level > 0ul) initLog << ",";
				initLog << bsrtpPlan_.populations[level];
			}
			initLog << " risks=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.final_risk.size(); ++level) {
				if (level > 0ul) initLog << ",";
				initLog << bsrtpPlan_.final_risk[level];
			}
			initLog << " pool_risks=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.final_pool_risk.size(); ++level) {
				if (level > 0ul) initLog << ",";
				initLog << bsrtpPlan_.final_pool_risk[level];
			}
			initLog << " risk_active=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.risk_active.size(); ++level) {
				if (level > 0ul) initLog << ",";
				initLog << (bsrtpPlan_.risk_active[level] ? 1 : 0);
			}
			ModelSuite::tech_log(initLog.str() + "\n");
			// Mark calibration complete without using its TSCP population formula.
			adaptivePopulationPlan_.finalize_reference(
				levelCount, "bsrtp_calibration_only");
		} else if ("ALP0" == alpConfig)
			adaptivePopulationPlan_.finalize_extinction(
				impFun_->thresholds().size());
		else if ("ALP2" == alpConfig)
			adaptivePopulationPlan_.finalize_success_extinction(
				impFun_->thresholds().size());
		else
			adaptivePopulationPlan_.finalize_budgeted(
				impFun_->thresholds().size(), numRuns);
		reachCount_.clear();

		std::ostringstream planLog;
		planLog << "ALP_PLAN"
		        << " alp_config=" << alpConfig
		        << " planner_mode=" << adaptivePopulationPlan_.planner_mode()
		        << " unobserved_fallback_levels="
		        << adaptivePopulationPlan_.unobserved_fallback_levels()
		        << " calibration_sweeps=" << bats_adaptive_calibration_sweeps()
		        << " calibration_population=" << bats_adaptive_calibration_population()
		        << " min_population=" << adaptivePopulationPlan_.config().min_population
		        << " max_population=" << adaptivePopulationPlan_.config().max_population
		        << " reference_population="
		        << adaptivePopulationPlan_.config().reference_population
		        << " max_extinction_risk="
		        << adaptivePopulationPlan_.config().max_extinction_risk
		        << " total_reference_budget="
		        << adaptivePopulationPlan_.total_reference_budget()
		        << " calibration_event_transitions="
		        << adaptivePopulationPlan_.calibration_event_transitions()
		        << " production_budget_per_run="
		        << adaptivePopulationPlan_.production_budget_per_run()
		        << " predicted_production_cost="
		        << adaptivePopulationPlan_.predicted_production_cost()
		        << " fallback_reason="
		        << (adaptivePopulationPlan_.fallback_reason().empty()
		            ? "none" : adaptivePopulationPlan_.fallback_reason())
		        << " levels=";
		for (size_t level = 0ul;
		     level < adaptivePopulationPlan_.level_count();
		     ++level) {
			if (level > 0ul)
				planLog << ";";
			planLog << level
			        << ":" << adaptivePopulationPlan_.trials(level)
			        << ":" << adaptivePopulationPlan_.retained(level)
			        << ":" << adaptivePopulationPlan_.successes(level)
			        << ":" << adaptivePopulationPlan_.rare_only_successes(level)
			        << ":" << adaptivePopulationPlan_.probability(level)
			        << ":" << adaptivePopulationPlan_.probability_success(level)
			        << ":" << adaptivePopulationPlan_.event_transitions(level)
			        << ":" << adaptivePopulationPlan_.population(level);
		}
		ModelSuite::tech_log(planLog.str() + "\n");
	}

	if (rolling && !rollingPopulationPlan_.valid && rollingStageIndex_ == 0ul) {
		adaptivePopulationCalibrationPhase_ = true;
		try {
			for (size_t sweep = 0ul;
			     sweep < bats_adaptive_calibration_sweeps();
			     ++sweep) {
				ThresholdsPathCandidates calibration;
				fixed_effort(calibration, get_event_watcher(property));
			}
		} catch (...) {
			adaptivePopulationCalibrationPhase_ = false;
			throw;
		}
		adaptivePopulationCalibrationPhase_ = false;
		const size_t levelCount = impFun_->thresholds().size();
		if (rollingObservations_.size() < levelCount)
			rollingObservations_.resize(levelCount);
		adaptivePopulationPlan_.finalize_success_extinction(levelCount);
		std::vector<size_t> fallback(levelCount,
			adaptivePopulationPlan_.config().reference_population);
		for (size_t level = 0ul; level < levelCount; ++level)
			fallback[level] = adaptivePopulationPlan_.population(level);
		rollingPreviousPopulations_ = fallback;
		rollingPopulationConfig_.epsilon = bats_rolling_epsilon();
		rollingPopulationConfig_.max_stage_change = bats_rolling_stage_change();
		// Budget reference: the actual fallback/ASES plan cost.  The rolling
		// optimizer may redistribute samples across levels, but by default it
		// should not receive a larger per-run event-transition budget than the
		// comparison plan it replaces.
		std::vector<double> levelCost(levelCount, 0.0);
		std::vector<double> observedCosts;
		for (size_t level = 0ul; level < levelCount; ++level) {
			const double c = rolling_cost(rollingObservations_[level],
			                              rollingPopulationConfig_);
			levelCost[level] = c;
			if (c > 0.0)
				observedCosts.push_back(c);
		}
		if (!observedCosts.empty()) {
			std::sort(observedCosts.begin(), observedCosts.end());
			const double defaultCost =
				observedCosts[observedCosts.size() / 2ul];
			for (size_t level = 0ul; level < levelCount; ++level)
				if (levelCost[level] <= 0.0)
					levelCost[level] = defaultCost;
		}
		double baseline = 0.0;
		double asesEquivalentCost = 0.0;
		for (size_t level = 0ul; level < levelCount; ++level) {
			const size_t asesPopulation =
				adaptive_level_population_from_counts(
					adaptivePopulationPlan_.retained(level),
					adaptivePopulationPlan_.trials(level),
					adaptivePopulationPlan_.config());
			baseline += levelCost[level] *
			            static_cast<double>(fallback[level]);
			asesEquivalentCost += levelCost[level] *
			                      static_cast<double>(asesPopulation);
		}
		if (!std::isfinite(baseline) || baseline <= 0.0)
			baseline = static_cast<double>(levelCount) *
			           static_cast<double>(rollingPopulationConfig_.min_population);
		double budgetReference = baseline;
		if (!std::isfinite(budgetReference) || budgetReference <= 0.0)
			budgetReference = (std::isfinite(asesEquivalentCost) &&
			                   asesEquivalentCost > 0.0)
				? asesEquivalentCost : baseline;
		rollingBaselineBudget_ = budgetReference;
		// The per-run cap (budgetReference * multiplier) is what bounds the
		// spend; the pool below is bookkeeping only and is deliberately
		// large so planning never freezes from pool exhaustion.
		rollingRemainingBudget_ = budgetReference *
			static_cast<double>(std::max<size_t>(1ul, numRuns)) * 8192.0;
		const double runs = static_cast<double>(std::max<size_t>(1ul, numRuns));
		const double stageBudget = std::min(
			rollingRemainingBudget_ / runs,
			budgetReference * bats_rolling_budget_multiplier());
		std::vector<RollingLevelObservation> planObservations(
			rollingObservations_.begin(),
			rollingObservations_.begin() +
				std::min(rollingObservations_.size(), levelCount));
		rollingPopulationPlan_ = rolling_plan_levels(
			planObservations, fallback, stageBudget,
			rollingPopulationConfig_, nullptr);
		if (rollingPopulationPlan_.populations.empty())
			rollingPopulationPlan_.populations = fallback;
		rollingPreviousPopulations_ = rollingPopulationPlan_.populations;
		++rollingStageIndex_;
		std::ostringstream rollingLog;
		rollingLog << "ROLLING_PLAN stage=" << rollingStageIndex_
		           << " budget_per_run=" << stageBudget
		           << " budget_ref=" << budgetReference
		           << " fallback_cost=" << baseline
		           << " remaining_budget=" << rollingRemainingBudget_
		           << " predicted_cost=" << rollingPopulationPlan_.predicted_cost
		           << " proxy=" << rollingPopulationPlan_.proxy_value
		           << " valid=" << (rollingPopulationPlan_.valid ? 1 : 0)
		           << " fallback=" << (rollingPopulationPlan_.fallback ? 1 : 0)
		           << " reason=" << rollingPopulationPlan_.reason
		           << " populations=";
		for (size_t level = 0ul;
		     level < rollingPopulationPlan_.populations.size(); ++level) {
			if (level > 0ul)
				rollingLog << ";";
			rollingLog << level << ":"
			           << rollingPopulationPlan_.populations[level];
		}
		rollingLog << " obs=";
		for (size_t level = 0ul; level < planObservations.size(); ++level) {
			if (level > 0ul)
				rollingLog << ";";
			const RollingLevelObservation& o(planObservations[level]);
			rollingLog << level << ":" << o.trials << "," << o.successes
			           << "," << o.retained << "," << o.event_transitions;
		}
		ModelSuite::tech_log(rollingLog.str() + "\n");
		reachCount_.clear();
	}

	if (vcstp) {
		const bool compactVcstpLog = bats_vcstp_compact_log_enabled();
		if (vcstpBatchIndex_ == 0ul) {
				vcstpPlan_ = VcstpPlan();
				vcstpPlan_.populations = vcstpProblem_.fixed_populations;
				vcstpPlan_.reason = "fixed_observation";
				++vcstpPlanCount_;
				if (!compactVcstpLog) {
					std::ostringstream log;
					log << "VCSTP_PLAN run_id="
					    << oracle_safe_text(oracle_env("BATS_CONFIG_ID")) << ":"
					    << oracle_safe_text(oracle_env("RNG_SEED"))
					    << " batch=1 phase=fixed_observation populations=";
					for (size_t level = 0ul;
					     level < vcstpPlan_.populations.size(); ++level) {
						if (level > 0ul) log << ";";
						log << level << ":" << vcstpPlan_.populations[level];
					}
					ModelSuite::tech_log(log.str() + "\n");
				}
		} else {
				const std::vector<size_t> previousPopulations(
					vcstpProblem_.previous_populations);
				const bool v3 =
					vcstpProblem_.config.minimize_event_cost_under_fixed_variance;
				const bool replanNow = !v3 || vcstpPlan_.populations.empty() ||
					((vcstpBatchIndex_ - 1ul) %
					 vcstpProblem_.config.replan_interval == 0ul);
				if (!compactVcstpLog) {
					const std::vector<size_t> activeLevels =
						vcstp_active_levels(vcstpProblem_.paths);
					std::ostringstream stateLog;
					stateLog << "VCSTP_STATE_DIAG phase=plan_input"
					         << " batch=" << (vcstpBatchIndex_ + 1ul)
					         << " paths=" << vcstpProblem_.paths.size()
					         << " active_levels=" << activeLevels.size()
					         << " observations=" << vcstpProblem_.observations.size()
					         << " fixed_populations="
					         << vcstpProblem_.fixed_populations.size()
					         << " previous_populations="
					         << vcstpProblem_.previous_populations.size();
					ModelSuite::tech_log(stateLog.str() + "\n");
				}
				double elapsedMs = 0.0;
				if (replanNow) {
					const auto start = std::chrono::steady_clock::now();
					vcstpPlan_ = v3 ? vcstp_plan_v3(vcstpProblem_)
					                    : vcstp_plan(vcstpProblem_);
					elapsedMs = std::chrono::duration<double,std::milli>(
						std::chrono::steady_clock::now() - start).count();
					vcstpLastSolveMs_ = elapsedMs;
					vcstpSolveMsTotal_ += elapsedMs;
					++vcstpPlanCount_;
					if (vcstpPlan_.valid)
						++vcstpOptimizedCount_;
					if (vcstpPlan_.fallback)
						++vcstpFallbackCount_;
					if (vcstpPlan_.variance_guard_applied)
						++vcstpVarianceGuardCount_;
					if (v3 ? vcstpPlan_.event_cost_reduced
					       : vcstpPlan_.cost_reduced)
						++vcstpCostReductionCount_;
					if (vcstpPlan_.absolute_risk_target_met)
						++vcstpAbsoluteRiskTargetCount_;
				}
				if (vcstpPlan_.populations.empty())
					vcstpPlan_.populations = vcstp_fallback_populations(
						vcstpProblem_, vcstp_active_levels(vcstpProblem_.paths));
				vcstpProblem_.previous_populations = vcstpPlan_.populations;
				if (!compactVcstpLog) {
					std::ostringstream log;
					log << std::setprecision(17)
					    << "VCSTP_PLAN run_id="
					    << oracle_safe_text(oracle_env("BATS_CONFIG_ID")) << ":"
					    << oracle_safe_text(oracle_env("RNG_SEED"))
					    << " batch=" << (vcstpBatchIndex_ + 1ul)
				    << " status=" << (vcstpPlan_.valid ? "optimal" : "fallback")
				    << " mode=" << (v3 ? "event_cost_v3" : "legacy")
				    << " replanned=" << (replanNow ? 1 : 0)
					    << " fallback=" << (vcstpPlan_.fallback ? 1 : 0)
					    << " reason=" << vcstpPlan_.reason
					    << " stage1_variance=" << vcstpPlan_.stage1_variance
					    << " final_variance=" << vcstpPlan_.final_variance
					    << " exact_single_path_variance="
					    << vcstpPlan_.exact_single_path_variance
					    << " fixed_cost=" << vcstpPlan_.fixed_cost
					    << " stage1_cost=" << vcstpPlan_.stage1_cost
				    << " final_cost=" << vcstpPlan_.final_cost
				    << " fixed_predicted_event_cost="
				    << vcstpPlan_.fixed_predicted_event_cost
				    << " final_predicted_event_cost="
				    << vcstpPlan_.final_predicted_event_cost
				    << " variance_ratio="
				    << (vcstpPlan_.fixed_variance > 0.0
				        ? vcstpPlan_.final_variance / vcstpPlan_.fixed_variance
				        : 1.0)
				    << " event_cost_reduced="
				    << (vcstpPlan_.event_cost_reduced ? 1 : 0)
					    << " fixed_extinction_risk="
					    << vcstpPlan_.fixed_extinction_risk
					    << " extinction_risk=" << vcstpPlan_.extinction_risk
					    << " effective_delta=" << vcstpPlan_.effective_delta
					    << " absolute_risk_target_met="
					    << (vcstpPlan_.absolute_risk_target_met ? 1 : 0)
					    << " frontier_states=" << vcstpPlan_.frontier_states
					    << " solve_ms=" << elapsedMs << " path=";
					for (size_t path = 0ul; path < vcstpProblem_.paths.size(); ++path) {
						if (path > 0ul) log << "|";
						for (size_t pos = 0ul;
						     pos < vcstpProblem_.paths[path].size(); ++pos) {
							if (pos > 0ul) log << ",";
							log << vcstpProblem_.paths[path][pos];
						}
					}
					log << " levels=";
					for (size_t level = 0ul;
					     level < vcstpProblem_.observations.size(); ++level) {
						if (level > 0ul) log << ";";
						const VcstpObservation& item(
							vcstpProblem_.observations[level]);
						log << level << ":" << item.trials << ","
						    << item.successes << "," << item.retained << ","
						    << item.event_transitions << ","
						    << (level < vcstpPlan_.p.size() ? vcstpPlan_.p[level] : 0.0)
						    << "," << (level < vcstpPlan_.rho.size()
						                  ? vcstpPlan_.rho[level] : 0.0)
						    << "," << (level < vcstpPlan_.gradient.size()
						                  ? vcstpPlan_.gradient[level] : 0.0)
						    << "," << vcstpProblem_.fixed_populations[level]
						    << "," << previousPopulations[level]
						    << "," << vcstpPlan_.populations[level];
					}
					ModelSuite::tech_log(log.str() + "\n");
				}
		}
	vcstpBatchObservations_.assign(
			vcstpProblem_.observations.size(), VcstpObservation());
		vcstpCurrentBatchTrajectories_ = 0ul;
		vcstpFixedCurrentBatchTrajectories_ = 0ul;
	}
	if (xiwangOnline)
		xiwangOnlineRuntime_.begin_formal_batch();
	std::map<std::vector<size_t>,size_t> vcstpBatchPathCounts;
	double cslaCumulativePredictedCost = 0.0;
	for (size_t run = 0ul; run < numRuns; ++run) {
		const size_t diagnosticBatchId = run;
		activeParentDiagBatchId = diagnosticBatchId;
		if (bsrtp && bsrtpRunIndex_ > 0ul &&
		    bsrtpRunIndex_ % bsrtpConfig_.replan_interval == 0ul) {
			bsrtpHistory_.decay(bsrtpConfig_.history_decay);
			const BsrtpPlan previousPlan(bsrtpPlan_);
			bsrtpPlan_ = bsrtp_plan(
				bsrtpHistory_.observations(),
				bsrtpPreviousPopulations_,
				bsrtpReferencePopulations_, bsrtpConfig_);
			if (!bsrtpPlan_.valid) {
				++bsrtpFallbackCount_;
				bsrtpPlan_ = previousPlan;
				bsrtpPlan_.reason = "keep_previous_after_failed_replan";
			} else {
				bsrtpPreviousPopulations_ = bsrtpPlan_.populations;
				++bsrtpReplanCount_;
			}
			std::ostringstream planLog;
			planLog << std::setprecision(17)
			        << "BSRTP_PLAN run=" << (bsrtpRunIndex_ + 1ul)
			        << " valid=" << (bsrtpPlan_.valid ? 1 : 0)
			        << " reason=" << bsrtpPlan_.reason
			        << " budget=" << bsrtpPlan_.budget
			        << " optimal_worst_risk="
			        << bsrtpPlan_.optimal_worst_risk
			        << " final_worst_risk=" << bsrtpPlan_.final_worst_risk
			        << " predicted_cost=" << bsrtpPlan_.predicted_cost
			        << " populations=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.populations.size(); ++level) {
				if (level > 0ul) planLog << ",";
				planLog << bsrtpPlan_.populations[level];
			}
			planLog << " risks=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.final_risk.size(); ++level) {
				if (level > 0ul) planLog << ",";
				planLog << bsrtpPlan_.final_risk[level];
			}
			planLog << " pool_risks=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.final_pool_risk.size(); ++level) {
				if (level > 0ul) planLog << ",";
				planLog << bsrtpPlan_.final_pool_risk[level];
			}
			planLog << " risk_active=";
			for (size_t level = 0ul;
			     level < bsrtpPlan_.risk_active.size(); ++level) {
				if (level > 0ul) planLog << ",";
				planLog << (bsrtpPlan_.risk_active[level] ? 1 : 0);
			}
			ModelSuite::tech_log(planLog.str() + "\n");
		}
		bsrtpBatchHistory_.reset(bsrtpHistory_.observations().size());
		const size_t bsrtpEventsBefore = bsrtpActualEventTransitions_;
		const size_t bsrtpTrajectoriesBefore = bsrtpActualTrajectories_;
		double cslaBatchPredictedCost = 0.0;
		const size_t cslaActualCostBefore = cslaActualEventTransitions_;
		if (csla && cslaBatchIndex_ > 0ul) {
			const size_t levelCount = cslaFixedPopulations_.size();
			std::vector<double> fixedCosts(levelCount, cslaConfig_.cost_floor);
			double fixedPredictedCost = 0.0;
			double requiredLowerCost = 0.0;
			double conservativeUnobservedCost = cslaConfig_.cost_floor;
			for (size_t level = 0ul; level < levelCount; ++level)
				if (cslaHistory_.observations()[level].trials > 0ul)
					conservativeUnobservedCost = std::max(
						conservativeUnobservedCost,
						csla_unit_cost(cslaHistory_.observations()[level],
						               cslaConfig_.cost_floor));
			for (size_t level = 0ul; level < levelCount; ++level) {
				fixedCosts[level] = cslaHistory_.observations()[level].trials > 0ul
					? csla_unit_cost(cslaHistory_.observations()[level],
					                 cslaConfig_.cost_floor)
					: conservativeUnobservedCost;
				fixedPredictedCost += fixedCosts[level] *
					static_cast<double>(cslaFixedPopulations_[level]);
				const size_t previous = std::max(cslaConfig_.min_population,
					std::min(cslaConfig_.max_population,
					         cslaPreviousPopulations_[level]));
				const size_t changeLower = static_cast<size_t>(std::ceil(
					static_cast<double>(previous) *
					(1.0 - cslaConfig_.max_batch_change)));
				const size_t hardLower =
					cslaHistory_.observations()[level].trials < cslaConfig_.min_samples
					? previous
					: std::max(cslaConfig_.min_population, changeLower);
				requiredLowerCost += fixedCosts[level] *
					static_cast<double>(hardLower);
			}
			const double remainingHardBudget = std::max(0.0,
				cslaRunBudget_ - static_cast<double>(cslaActualEventTransitions_));
			const double planBudget = std::min(
				std::max(bats_csla_budget_eta() * fixedPredictedCost,
				         requiredLowerCost),
				remainingHardBudget);
			cslaPlan_ = csla_plan_levels(
				cslaHistory_.observations(), cslaFixedPopulations_,
				cslaPreviousPopulations_, planBudget, cslaConfig_);
			if (!cslaPlan_.valid) {
				std::ostringstream error;
				error << "CSLA planning failed: reason=" << cslaPlan_.reason
				      << " batch=" << (cslaBatchIndex_ + 1ul)
				      << " budget=" << planBudget;
				ModelSuite::tech_log(error.str() + "\n");
				throw_FigException(error.str());
			}
			if (cslaPlan_.predicted_cost > cslaPlan_.budget + 1.0e-9)
				throw_FigException("CSLA budget conservation assertion failed");
			cslaBatchPredictedCost = cslaPlan_.predicted_cost;
			cslaPreviousPopulations_ = cslaPlan_.populations;
			std::ostringstream log;
			log << std::setprecision(17)
			    << "CSLA_PLAN batch=" << (cslaBatchIndex_ + 1ul)
			    << " eta=" << bats_csla_budget_eta()
			    << " run_budget=" << cslaRunBudget_
			    << " planning_budget=" << cslaPlanningBudget_
			    << " cumulative_actual_cost=" << cslaActualEventTransitions_
			    << " fixed_predicted_cost=" << fixedPredictedCost
			    << " required_lower_cost=" << requiredLowerCost
			    << " budget=" << cslaPlan_.budget
				    << " predicted_cost=" << cslaPlan_.predicted_cost
				    << " risk_target=" << cslaPlan_.risk_target
				    << " risk_constraint_satisfied="
				    << (cslaPlan_.risk_constraint_satisfied ? 1 : 0)
			    << " remaining_budget=" << remainingHardBudget
			    << " variance_term=" << cslaPlan_.variance_term
			    << " extinction_term=" << cslaPlan_.extinction_term
			    << " objective=" << cslaPlan_.objective
			    << " lambda=" << cslaPlan_.lambda
			    << " frozen_levels=" << cslaPlan_.frozen_levels
			    << " conservative=" << (cslaPlan_.conservative ? 1 : 0)
			    << " exact_checked=" << (cslaPlan_.exact_checked ? 1 : 0)
			    << " exact_match=" << (cslaPlan_.exact_match ? 1 : 0)
			    << " reason=" << cslaPlan_.reason << " levels=";
			for (size_t level = 0ul; level < levelCount; ++level) {
				if (level > 0ul) log << ";";
				const CslaObservation& item(cslaHistory_.observations()[level]);
				log << level << ":" << item.trials << "," << item.successes
				    << "," << item.retained << "," << item.event_transitions
				    << "," << cslaPlan_.q[level] << "," << cslaPlan_.rho[level]
				    << "," << cslaPlan_.cost[level]
				    << "," << cslaFixedPopulations_[level]
				    << "," << cslaPlan_.populations[level]
				    << "," << cslaPlan_.lower[level]
				    << "," << cslaPlan_.upper[level];
			}
			ModelSuite::tech_log(log.str() + "\n");
		} else if (csla) {
			for (const size_t population: cslaFixedPopulations_)
				cslaBatchPredictedCost += cslaConfig_.cost_floor *
					static_cast<double>(population);
			std::ostringstream log;
			log << "CSLA_PLAN batch=1 phase=fixed_observation"
			    << " run_budget=" << cslaRunBudget_
			    << " planning_budget=" << cslaPlanningBudget_
			    << " history_generation=" << cslaHistory_.generation()
			    << " fixed_populations=";
			for (size_t level = 0ul; level < cslaFixedPopulations_.size(); ++level) {
				if (level > 0ul) log << ",";
				log << cslaFixedPopulations_[level];
			}
			ModelSuite::tech_log(log.str() + "\n");
		}
		completedFixedEffortInvocationId.clear();
		fixed_effort(paths, get_event_watcher(property));
		const std::string pathInvocationId = completedFixedEffortInvocationId;
		if (vcstp && !bats_vcstp_compact_log_enabled()) {
			std::ostringstream stateLog;
			stateLog << "VCSTP_STATE_DIAG phase=after_fixed_effort"
			         << " batch=" << (vcstpBatchIndex_ + 1ul)
			         << " run=" << run
			         << " returned_paths=" << paths.size()
			         << " nonempty_paths=";
			size_t nonemptyPaths = 0ul;
			for (const auto& path: paths)
				if (!path.empty())
					++nonemptyPaths;
			stateLog << nonemptyPaths
			         << " observed_paths_before_count="
			         << vcstpBatchPathCounts.size();
			ModelSuite::tech_log(stateLog.str() + "\n");
		}
		if (csla) {
			const size_t actualEventCost =
				cslaActualEventTransitions_ - cslaActualCostBefore;
			cslaCumulativePredictedCost += cslaBatchPredictedCost;
			const double predictionErrorRatio = cslaBatchPredictedCost > 0.0
				? (static_cast<double>(actualEventCost) - cslaBatchPredictedCost) /
				  cslaBatchPredictedCost
				: 0.0;
			const double remainingBudget = cslaRunBudget_ -
				static_cast<double>(cslaActualEventTransitions_);
			const double budgetUtilization = cslaRunBudget_ > 0.0
				? static_cast<double>(cslaActualEventTransitions_) / cslaRunBudget_
				: 0.0;
			std::ostringstream costLog;
			costLog << std::setprecision(17)
			        << "CSLA_BATCH_COST batch=" << (cslaBatchIndex_ + 1ul)
			        << " predicted_event_cost=" << cslaBatchPredictedCost
			        << " actual_event_cost=" << actualEventCost
			        << " cumulative_predicted_cost=" << cslaCumulativePredictedCost
			        << " cumulative_actual_cost=" << cslaActualEventTransitions_
			        << " run_budget=" << cslaRunBudget_
			        << " remaining_budget=" << remainingBudget
			        << " prediction_error_ratio=" << predictionErrorRatio
			        << " budget_utilization=" << budgetUtilization;
			ModelSuite::tech_log(costLog.str() + "\n");
		}
		if (vcstp) {
			for (const auto& path: paths) {
				std::vector<size_t> actualPath;
				for (const auto& conditional: path)
					actualPath.push_back(static_cast<size_t>(conditional.first));
				if (!actualPath.empty()) {
					vcstpBatchPathCounts[actualPath]++;
				}
			}
			if (!bats_vcstp_compact_log_enabled()) {
				std::ostringstream stateLog;
				stateLog << "VCSTP_STATE_DIAG phase=path_counted"
				         << " batch=" << (vcstpBatchIndex_ + 1ul)
				         << " run=" << run
				         << " observed_paths=" << vcstpBatchPathCounts.size();
				ModelSuite::tech_log(stateLog.str() + "\n");
			}
		}
		if (csla && static_cast<double>(cslaActualEventTransitions_) >
		            cslaRunBudget_ + 1.0e-9)
			throw_FigException("CSLA actual event budget exceeded");
		if (csla)
			++cslaBatchIndex_;
		results[run] = 0.0;
		size_t diagnosticPathIndex = 0ul;
		for (const auto& path: paths) {
			if (path.empty()) {
				if (bats_sfe_parent_diag_enabled()) {
					std::ostringstream diag;
					diag << std::setprecision(17)
					     << "SFE_PARENT_PATH"
					     << " fixed_effort_invocation_id="
					     << oracle_safe_text(pathInvocationId)
					     << " diagnostic_run_id=" << oracle_safe_text(activeParentDiagRunId)
					     << " batch_id=" << activeParentDiagBatchId
					     << " level=terminal"
					     << " batch_step=terminal"
					     << " run=" << run
				     << " path_id=" << diagnosticPathIndex++
				     << " path_contribution=0"
				     << " path_signature=-"
				     << " conditional_factor_sequence=-";
					bats_sfe_parent_diag_log(diag.str());
				}
				continue;
			}
			double estimate = 1.0;
			for (const auto& conditional: path)
				estimate *= conditional.second;
			results[run] += estimate;
			if (bats_sfe_parent_diag_enabled()) {
				std::ostringstream diag;
				diag << std::setprecision(17)
				     << "SFE_PARENT_PATH"
				     << " fixed_effort_invocation_id="
				     << oracle_safe_text(pathInvocationId)
				     << " diagnostic_run_id=" << oracle_safe_text(activeParentDiagRunId)
				     << " batch_id=" << activeParentDiagBatchId
				     << " level=terminal"
				     << " batch_step=terminal"
				     << " run=" << run
				     << " path_id=" << diagnosticPathIndex++
				     << " path_contribution=" << estimate
				     << " path_signature=";
				bool firstLevel = true;
				for (const auto& conditional: path) {
					if (!firstLevel)
						diag << ",";
					firstLevel = false;
					diag << conditional.first;
				}
				diag
				     << " conditional_factor_sequence=";
				bool firstFactor = true;
				for (const auto& conditional: path) {
					if (!firstFactor)
						diag << ";";
					firstFactor = false;
					diag << conditional.first << ":" << conditional.second;
				}
				bats_sfe_parent_diag_log(diag.str());
			}
		}
		if (bsrtp) {
			++bsrtpRunIndex_;
			const auto& batchObs = bsrtpBatchHistory_.observations();
			const size_t comparableLevels = std::min(
				batchObs.size(), bsrtpPlan_.final_risk.size());
			for (size_t level = 0ul; level < comparableLevels; ++level) {
				if (batchObs[level].trials <= 0.0)
					continue;
				bsrtpPredictedExtinctionSum_ += bsrtpPlan_.final_risk[level];
				++bsrtpExtinctionObservationCount_;
				bsrtpExtinctionCount_ += batchObs[level].successes <= 0.0 ? 1ul : 0ul;
				++bsrtpPoolExtinctionObservationCount_;
				bsrtpPoolExtinctionCount_ += batchObs[level].retained <= 0.0 ? 1ul : 0ul;
			}
			for (size_t level = 0ul; level < batchObs.size(); ++level) {
				if (level >= comparableLevels || batchObs[level].trials <= 0.0)
					continue;
				for (size_t other = level + 1ul;
				     other < comparableLevels; ++other) {
					if (batchObs[other].trials <= 0.0)
						continue;
					const double predictedDiff =
						bsrtpPlan_.final_risk[level] - bsrtpPlan_.final_risk[other];
					const int observedDiff =
						(batchObs[level].successes <= 0.0 ? 1 : 0) -
						(batchObs[other].successes <= 0.0 ? 1 : 0);
					if (std::fabs(predictedDiff) <= 1.0e-15 || observedDiff == 0)
						continue;
					++bsrtpRiskDirectionPairCount_;
					if (predictedDiff * static_cast<double>(observedDiff) > 0.0)
						++bsrtpRiskDirectionAgreementCount_;
				}
			}
		}
	}
	if (bsrtp) {
		std::ostringstream summary;
		summary << std::setprecision(17)
		        << "BSRTP_SUMMARY runs=" << bsrtpRunIndex_
		        << " replans=" << bsrtpReplanCount_
		        << " fallbacks=" << bsrtpFallbackCount_
		        << " actual_event_cost=" << bsrtpActualEventTransitions_
		        << " actual_trajectories=" << bsrtpActualTrajectories_
		        << " last_worst_risk=" << bsrtpPlan_.final_worst_risk
		        << " last_predicted_cost=" << bsrtpPlan_.predicted_cost
		        << " risk_predictions=" << bsrtpPlan_.final_risk.size()
		        << " extinction_observations="
		        << bsrtpExtinctionObservationCount_
		        << " predicted_extinction_sum="
		        << bsrtpPredictedExtinctionSum_
		        << " observed_extinctions=" << bsrtpExtinctionCount_
		        << " pool_extinction_observations="
		        << bsrtpPoolExtinctionObservationCount_
		        << " observed_pool_extinctions="
		        << bsrtpPoolExtinctionCount_
		        << " direction_pairs=" << bsrtpRiskDirectionPairCount_
		        << " direction_agreements="
		        << bsrtpRiskDirectionAgreementCount_;
		ModelSuite::tech_log(summary.str() + "\n");
	}
	if (vcstp) {
		if (!bats_vcstp_compact_log_enabled()) {
			std::ostringstream stateLog;
			stateLog << "VCSTP_STATE_DIAG phase=batch_write_before"
			         << " batch=" << (vcstpBatchIndex_ + 1ul)
			         << " observed_paths=" << vcstpBatchPathCounts.size()
			         << " existing_paths=" << vcstpProblem_.paths.size();
			ModelSuite::tech_log(stateLog.str() + "\n");
		}
		std::map<std::vector<size_t>,size_t> storedPathCounts;
		if (vcstpProblem_.config.minimize_event_cost_under_fixed_variance &&
		    vcstpProblem_.path_counts.size() == vcstpProblem_.paths.size())
			for (size_t path = 0ul; path < vcstpProblem_.paths.size(); ++path)
				storedPathCounts[vcstpProblem_.paths[path]] +=
					vcstpProblem_.path_counts[path];
		for (const auto& item: vcstpBatchPathCounts)
			storedPathCounts[item.first] += item.second;
		vcstpProblem_.paths.clear();
		vcstpProblem_.path_counts.clear();
		for (const auto& item: storedPathCounts) {
			vcstpProblem_.paths.push_back(item.first);
			vcstpProblem_.path_counts.push_back(item.second);
		}
		if (vcstpProblem_.paths.empty())
			vcstpProblem_.previous_populations =
				vcstpProblem_.fixed_populations;
		if (!bats_vcstp_compact_log_enabled()) {
			const std::vector<size_t> activeLevels =
				vcstp_active_levels(vcstpProblem_.paths);
			std::ostringstream stateLog;
			stateLog << "VCSTP_STATE_DIAG phase=batch_write_after"
			         << " batch=" << (vcstpBatchIndex_ + 1ul)
			         << " stored_paths=" << vcstpProblem_.paths.size()
			         << " stored_active_levels=" << activeLevels.size()
			         << " previous_reset="
			         << (vcstpProblem_.paths.empty() ? 1 : 0);
			ModelSuite::tech_log(stateLog.str() + "\n");
		}
		vcstpCumulativeTrajectories_ += vcstpCurrentBatchTrajectories_;
		vcstpFixedCumulativeTrajectories_ +=
			vcstpFixedCurrentBatchTrajectories_;
		++vcstpBatchIndex_;
		const bool compactVcstpLog = bats_vcstp_compact_log_enabled();
		std::ostringstream log;
		log << (compactVcstpLog ? "VCSTP_SUMMARY run_id="
		                         : "VCSTP_BATCH_END run_id=")
		    << oracle_safe_text(oracle_env("BATS_CONFIG_ID")) << ":"
		    << oracle_safe_text(oracle_env("RNG_SEED"))
		    << " batch=" << vcstpBatchIndex_
		    << " phase="
		    << (vcstpBatchIndex_ == 1ul ? "fixed_observation" : "production")
		    << " plan_rows=" << vcstpPlanCount_
		    << " optimized_rows=" << vcstpOptimizedCount_
		    << " fallback_rows=" << vcstpFallbackCount_
		    << " variance_guard_rows=" << vcstpVarianceGuardCount_
		    << " cost_reduction_rows=" << vcstpCostReductionCount_
		    << " absolute_risk_target_rows=" << vcstpAbsoluteRiskTargetCount_
		    << " status=" << (vcstpPlan_.valid ? "optimal" : "fallback")
		    << " fallback=" << (vcstpPlan_.fallback ? 1 : 0)
		    << " reason=" << vcstpPlan_.reason
		    << " stage1_variance=" << vcstpPlan_.stage1_variance
		    << " final_variance=" << vcstpPlan_.final_variance
		    << " fixed_cost=" << vcstpPlan_.fixed_cost
		    << " stage1_cost=" << vcstpPlan_.stage1_cost
		    << " final_cost=" << vcstpPlan_.final_cost
		    << " fixed_predicted_event_cost="
		    << vcstpPlan_.fixed_predicted_event_cost
		    << " final_predicted_event_cost="
		    << vcstpPlan_.final_predicted_event_cost
		    << " variance_ratio="
		    << (vcstpPlan_.fixed_variance > 0.0
		        ? vcstpPlan_.final_variance / vcstpPlan_.fixed_variance
		        : 1.0)
		    << " event_cost_reduced="
		    << (vcstpPlan_.event_cost_reduced ? 1 : 0)
		    << " extinction_risk=" << vcstpPlan_.extinction_risk
		    << " absolute_risk_target_met="
		    << (vcstpPlan_.absolute_risk_target_met ? 1 : 0)
		    << " variance_guard="
		    << (vcstpPlan_.variance_guard_applied ? 1 : 0)
		    << " cost_reduced=" << (vcstpPlan_.cost_reduced ? 1 : 0)
		    << " solve_ms=" << vcstpLastSolveMs_
		    << " solve_ms_total=" << vcstpSolveMsTotal_
		    << " actual_trajectory_count=" << vcstpCurrentBatchTrajectories_
		    << " cumulative_trajectory_count=" << vcstpCumulativeTrajectories_
		    << " fixed_batch_trajectory_count="
		    << vcstpFixedCurrentBatchTrajectories_
		    << " fixed_cumulative_trajectory_count="
		    << vcstpFixedCumulativeTrajectories_
		    << " actual_trajectory_ratio="
		    << (vcstpFixedCurrentBatchTrajectories_ > 0ul
		        ? static_cast<double>(vcstpCurrentBatchTrajectories_) /
		          static_cast<double>(vcstpFixedCurrentBatchTrajectories_)
		        : 0.0)
		    << " distinct_paths=" << vcstpBatchPathCounts.size()
		    << " budget_exceeded="
		    << (vcstpProblem_.config.minimize_event_cost_under_fixed_variance
		        ? (vcstpPlan_.final_predicted_event_cost >
		           vcstpPlan_.fixed_predicted_event_cost + 1.0e-9 ? 1 : 0)
		        : (vcstpPlan_.final_cost > vcstpPlan_.fixed_cost ? 1 : 0));
		if (!compactVcstpLog)
			log << " selected_path=";
		if (!compactVcstpLog && !vcstpProblem_.paths.empty())
			for (size_t pos = 0ul; pos < vcstpProblem_.paths.front().size(); ++pos) {
				if (pos > 0ul) log << ",";
				log << vcstpProblem_.paths.front()[pos];
			}
		if (!compactVcstpLog) {
			log << " levels=";
			for (size_t level = 0ul;
			     level < vcstpBatchObservations_.size(); ++level) {
				if (level > 0ul) log << ";";
				const VcstpObservation& item(vcstpBatchObservations_[level]);
				log << level << ":" << item.trials << ","
				    << item.successes << "," << item.retained << ","
				    << item.event_transitions;
			}
		}
		ModelSuite::tech_log(log.str() + "\n");
		if (vcstpProblem_.config.minimize_event_cost_under_fixed_variance) {
			if (vcstpPlan_.final_predicted_event_cost >
			    vcstpPlan_.fixed_predicted_event_cost + 1.0e-9)
				throw_FigException("VCSTP predicted event-cost budget exceeded");
		} else if (vcstpPlan_.final_cost > vcstpPlan_.fixed_cost) {
			throw_FigException("VCSTP planning trajectory budget exceeded");
		}
	}
	if (csla) {
		cslaRemainingBudget_ = cslaRunBudget_ -
			static_cast<double>(cslaActualEventTransitions_);
		if (cslaRemainingBudget_ < -1.0e-9)
			throw_FigException("CSLA actual event budget exceeded");
		std::ostringstream log;
		log << "CSLA_BATCH_END batch=" << cslaBatchIndex_
		    << " cumulative_actual_event_transitions="
		    << cslaActualEventTransitions_
		    << " run_budget=" << cslaRunBudget_
		    << " remaining_budget=" << cslaRemainingBudget_;
		ModelSuite::tech_log(log.str() + "\n");
	}
	if (xiwangOnline) {
		xiwangOnlineRuntime_.end_formal_batch();
		ModelSuite::tech_log(
			"XIWANG_ONLINE_BATCH " + xiwangOnlineRuntime_.last_summary());
	}
	return results;
}


const SimulationEngineFixedEffort::EventWatcher&
SimulationEngineSFE::get_event_watcher(const Property& property) const
{
	using namespace std::placeholders;  // _1, _2, ...
	// shared_ptr avoids memory leaks and also deletion when exiting scope
	static std::shared_ptr<const EventWatcher> event_watcher(nullptr);
	switch (property.type) {
	case PropertyType::TRANSIENT:
		event_watcher = std::make_shared<const EventWatcher>(
				std::bind(&SimulationEngineSFE::transient_event, this, _1, _2, _3));
		break;
	case PropertyType::RATE:
		event_watcher = std::make_shared<const EventWatcher>(
				std::bind(&SimulationEngineSFE::rate_event, this, _1, _2, _3));
		break;
	default:
		throw_FigException("unsupported property type: "+std::to_string(property.type));
	}
	return *event_watcher;
}


void
SimulationEngineSFE::fixed_effort_kbranch(
	ThresholdsPathCandidates& result,
	const EventWatcher& watch_events) const
{
	if (toBuildThresholds_)
		throw_FigException("KBRANCH cannot be used while building thresholds");
	if (bats_branch_fk_observe_enabled() ||
	    bats_branch_fk_shadow_plan_enabled() ||
	    bats_adaptive_level_population_enabled() ||
	    bats_rolling_level_optimization_enabled() ||
	    bats_vcstp_enabled() || bats_csla_enabled() || bats_bsrtp_enabled() ||
	    bats_ssta_stage0_weighted_enabled() ||
	    oracle_env_flag("BATS_SSTA_STATE_ALLOCATION") ||
	    oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION"))
		throw_FigException(
			"KBRANCH production is incompatible with observation and legacy adaptive allocation modes");

	const size_t requestedK = bats_branch_fk_keep_k();
	if (requestedK != 2ul)
		throw_FigException("KBRANCH production currently requires BATS_BRANCH_FK_K=2");
	const ImportanceValue initialLevel = impFun_->initial_value(false);
	const ImportanceValue maximumLevel = impFun_->max_value();
	const bool tvcpEnabled = bats_tvcp_enabled();
	const bool tvcpNestedDiagnostic = bats_tvcp_nested_diagnostic_enabled();
	if (tvcpNestedDiagnostic && !tvcpEnabled)
		throw_FigException("BATS_TVCP_NESTED_DIAGNOSTIC requires BATS_TVCP");
	const bool tvcpDiagnosticOnly = bats_tvcp_diagnostic_only_enabled();
	if (tvcpNestedDiagnostic != tvcpDiagnosticOnly)
		throw_FigException(
			"TVCP nested diagnostics and BATS_TVCP_DIAGNOSTIC_ONLY must be enabled together");
	const size_t tvcpNestedOuter = tvcpNestedDiagnostic
		? bats_tvcp_nested_outer_replicates() : 0ul;
	const size_t tvcpNestedInner = tvcpNestedDiagnostic
		? bats_tvcp_nested_inner_replicates() : 0ul;
	const std::map<unsigned,size_t> tvcpPopulations = tvcpEnabled
		? bats_tvcp_population_map() : std::map<unsigned,size_t>();
	if(tvcpEnabled){
		const auto frozenChain=bats_tvcp_threshold_chain();
		const auto& actualThresholds=impFun_->thresholds();
		std::vector<unsigned long long> actualChain;
		for(size_t i=1ul;i+1ul<actualThresholds.size();++i)
			actualChain.push_back(static_cast<unsigned long long>(actualThresholds[i].first));
		if(frozenChain!=actualChain) throw_FigException("TVCP frozen threshold chain does not match active FIG thresholds");
		try { tvcp_validate_level_coverage(tvcpPopulations,static_cast<unsigned>(initialLevel),static_cast<unsigned>(maximumLevel)); }
		catch(const std::exception& error){ throw_FigException(error.what()); }
	}
	auto levelEffort = [&](ImportanceValue level) -> size_t {
		if (tvcpEnabled) {
			try{return tvcp_population_for_level(tvcpPopulations,static_cast<unsigned>(level));}
			catch(const std::exception& error){throw_FigException(error.what());}
		}
		const size_t configured = bats_level_population();
		if (configured > 0ul) return configured;
		const size_t effort = std::max<size_t>(
			lvl_effort_min(), impFun_->effort_of(level));
		return effort * base_nsims();
	};
	std::ostringstream effectivePlan;
	for (ImportanceValue level = initialLevel; level <= maximumLevel; ++level) {
		if (level > initialLevel) effectivePlan << ",";
		effectivePlan << level << ":" << levelEffort(level);
	}
	std::ostringstream actualThresholdChain;
	const auto& actualIdentityThresholds = impFun_->thresholds();
	for (size_t index = 1ul; index + 1ul < actualIdentityThresholds.size(); ++index) {
		if (index > 1ul) actualThresholdChain << ",";
		actualThresholdChain << actualIdentityThresholds[index].first;
	}
	std::ostringstream identity;
	identity << "TVCP_RUN_IDENTITY"
	         << " model_id=" << oracle_safe_text(oracle_env("TVCP_MODEL_ID", "-"))
	         << " seed=" << oracle_safe_text(oracle_env("RNG_SEED", "-"))
	         << " plan=" << effectivePlan.str()
	         << " threshold_chain=" << actualThresholdChain.str()
	         << " binary_sha256=" << oracle_safe_text(oracle_env("TVCP_BINARY_SHA256", "-"))
	         << " model_sha256=" << oracle_safe_text(oracle_env("TVCP_MODEL_SHA256", "-"))
	         << " manifest_sha256=" << oracle_safe_text(oracle_env("TVCP_MANIFEST_SHA256", "-"))
	         << " diagnostic_only=" << (tvcpDiagnosticOnly ? 1 : 0);
	ModelSuite::tech_log(identity.str() + "\n");

	struct LiveParticle {
		Reference<Traial> traial;
		double mass;
		std::string parentId;
		std::string candidateId;
		size_t offspringOrdinal;
		std::string factorSequence;
		std::vector<unsigned> lineageLevels;
		size_t eventCount;
		LiveParticle(Traial& value, double valueMass,
		             const std::string& parent,
		             const std::string& candidate,
		             size_t ordinal,
		             const std::string& factors,
		             const std::vector<unsigned>& lineage,
		             size_t events) :
			traial(value), mass(valueMass), parentId(parent),
			candidateId(candidate), offspringOrdinal(ordinal),
			factorSequence(factors), lineageLevels(lineage), eventCount(events) {}
	};
	typedef std::map<ImportanceValue, std::vector<LiveParticle> > LiveFrontier;

	static thread_local unsigned long long kbranchRunSerial = 0ull;
	const unsigned long long runSerial = kbranchRunSerial++;
	static thread_local unsigned long long tvcpNestedMasterSeed = 0ull;
	if (tvcpNestedDiagnostic && tvcpNestedMasterSeed == 0ull)
		tvcpNestedMasterSeed = static_cast<unsigned long long>(Clock::rng_seed());
	const unsigned long long baseSeed = tvcpNestedDiagnostic
		? tvcpNestedMasterSeed : static_cast<unsigned long long>(Clock::rng_seed());
	const auto tvcpNestedModelClocks(model_->clocks());
	if (tvcpNestedDiagnostic) {
		unsigned long rootSeed = static_cast<unsigned long>(
			(baseSeed ^ (0xd1b54a32d192ed03ull * (runSerial + 1ull))) & 0xffffffffull);
		if (rootSeed == 0ul) rootSeed = 1ul;
		for (const auto& clockRef: tvcpNestedModelClocks)
			clockRef.get().reset_distribution_rng_state();
		Clock::reseed_for_diagnostics(rootSeed);
	}

	auto tpool(TraialPool::get_instance());
	LiveFrontier frontier;
	Traial& seed(tpool.get_traial());
	seed.initialise(*model_, *impFun_);
	seed.weight = 1.0;
	frontier[initialLevel].emplace_back(
		seed, 1.0, "root", activeFixedEffortInvocationId + ":seed",
		0ul, "-", std::vector<unsigned>{static_cast<unsigned>(initialLevel)}, 0ul);

	std::mt19937_64 plannerRng(
		baseSeed ^ (0x9e3779b97f4a7c15ull +
		runSerial * 0xbf58476d1ce4e5b9ull));

	double terminalTopMass = 0.0;
	double terminalDeadMass = 0.0;
	size_t totalEventTransitions = 0ul;
	size_t totalContinuations = 0ul;
	size_t totalFrames = 0ul;
	size_t totalRetained = 0ul;
	size_t totalDirectTop = 0ul;
	size_t totalJumps = 0ul;
	size_t maximumFrontierPaths = 1ul;
	size_t frameSerial = 0ul;
	std::map<unsigned,size_t> tvcpStageTrials, tvcpStageSuccesses,
		tvcpStageEvents, tvcpStageAccesses;
	std::map<unsigned, std::vector<LiveParticle> > tvcpNestedSnapshots;

	while (!frontier.empty() && !interrupted) {
		auto levelIt = frontier.begin();
		const ImportanceValue sourceLevel = levelIt->first;
		std::vector<LiveParticle> parents(std::move(levelIt->second));
		frontier.erase(levelIt);
		if (sourceLevel > maximumLevel)
			throw_FigException("KBRANCH frontier level exceeds the maximum threshold");
		std::vector<double> parentMasses;
		parentMasses.reserve(parents.size());
		for (const auto& parent: parents) {
			if (!std::isfinite(parent.mass) || parent.mass <= 0.0)
				throw_FigException("KBRANCH frontier contains a non-positive parent mass");
			if (parent.traial.get().level != sourceLevel)
				throw_FigException("KBRANCH frontier state level is misaligned");
			parentMasses.push_back(parent.mass);
		}
		if (tvcpNestedDiagnostic) {
			auto& snapshot = tvcpNestedSnapshots[static_cast<unsigned>(sourceLevel)];
			if (!snapshot.empty())
				throw_FigException("TVCP nested diagnostic saw a repeated source-level activation");
			for (const auto& parent: parents) {
				Traial& clone(tpool.get_traial());
				clone = parent.traial.get();
				snapshot.emplace_back(clone, parent.mass, parent.parentId,
					parent.candidateId, parent.offspringOrdinal,
					parent.factorSequence, parent.lineageLevels, parent.eventCount);
			}
		}
		const size_t continuations = levelEffort(sourceLevel);
		const auto allocation = kbranch_allocate_continuations(
			parentMasses, continuations);

		std::vector<KBranchFrameCandidate> frame;
		std::vector<Traial*> liveCandidates;
		frame.reserve(continuations);
		liveCandidates.reserve(continuations);
		size_t levelEventTransitions = 0ul;
		size_t levelSuccesses = 0ul;
		const EventWatcher countedWatch = [&](const Property& property,
		                                       Traial& child,
		                                       Event& event) {
			const bool stop = watch_events(property, child, event);
			if (!stop) levelEventTransitions++;
			return stop;
		};

		for (size_t parentIndex = 0ul; parentIndex < parents.size(); ++parentIndex) {
			const LiveParticle& parent(parents[parentIndex]);
			const size_t offspringCount = allocation[parentIndex];
			if (offspringCount == 0ul)
				throw_FigException("KBRANCH deleted a positive-mass parent");
			const double childMass = parent.mass /
				static_cast<double>(offspringCount);
			for (size_t ordinal = 0ul; ordinal < offspringCount; ++ordinal) {
				Traial& child(tpool.get_traial());
				if (resampleOnSplit_)
					child.copyResampling(parent.traial.get());
				else
					child = parent.traial.get();
				child.level = sourceLevel;
				child.depth = 0;
				child.weight = childMass;
				const size_t before = levelEventTransitions;
				model_->simulation_step(child, *property_, countedWatch);
				const size_t eventCount = levelEventTransitions - before;
				const bool directTop = property_->is_rare(child.state);
				const bool terminalDead = !directTop &&
					(property_->is_stop(child.state) || child.level <= sourceLevel);
				if (!directTop && !terminalDead && child.level <= sourceLevel)
					throw_FigException("KBRANCH continuation did not advance or terminate");
				const size_t candidateIndex = frame.size();
				std::vector<unsigned> lineage = parent.lineageLevels;
				if (lineage.empty() || lineage.back() != static_cast<unsigned>(sourceLevel))
					lineage.push_back(static_cast<unsigned>(sourceLevel));
				std::ostringstream candidateId;
				candidateId << activeFixedEffortInvocationId << ":"
				            << frameSerial << ":" << candidateIndex;
				std::ostringstream factors;
				if (parent.factorSequence != "-" && !parent.factorSequence.empty())
					factors << parent.factorSequence << ";";
				factors << sourceLevel << "->" << child.level;
				frame.emplace_back(candidateIndex,
					static_cast<unsigned>(child.level), parent.mass,
					1.0 / static_cast<double>(offspringCount), childMass,
					directTop, terminalDead, parent.candidateId,
					candidateId.str(), factors.str(), lineage, eventCount);
				liveCandidates.push_back((directTop || terminalDead) ? nullptr : &child);
			if (directTop) {
				levelSuccesses++;
				totalDirectTop++;
					tpool.return_traial(std::move(child));
				} else if (terminalDead) {
					terminalDeadMass += childMass;
					tpool.return_traial(std::move(child));
				} else {
					levelSuccesses++;
					if (child.level > sourceLevel + 1u)
						totalJumps++;
				}
				totalContinuations++;
			}
		}
		for (auto& parent: parents)
			tpool.return_traial(parent.traial);

		const KBranchFrameResult compressed =
			kbranch_compress_frame(frame, requestedK, plannerRng);
		terminalTopMass += compressed.direct_top_mass;
		std::vector<bool> selected(frame.size(), false);
		for (const auto& retained: compressed.retained) {
			if (retained.candidate_index >= liveCandidates.size() ||
			    nullptr == liveCandidates[retained.candidate_index])
				throw_FigException("KBRANCH retained an invalid candidate snapshot");
			selected[retained.candidate_index] = true;
			Traial& child(*liveCandidates[retained.candidate_index]);
			child.weight = retained.propagated_mass;
			frontier[static_cast<ImportanceValue>(retained.reached_level)].emplace_back(
				child, retained.propagated_mass, retained.parent_id,
				retained.candidate_id, 0ul, retained.factor_sequence,
				retained.lineage_levels, retained.event_count);
		}
		for (size_t i = 0ul; i < liveCandidates.size(); ++i)
			if (nullptr != liveCandidates[i] && !selected[i])
				tpool.return_traial(std::move(*liveCandidates[i]));

		totalEventTransitions += levelEventTransitions;
		tvcpStageTrials[static_cast<unsigned>(sourceLevel)] += continuations;
		tvcpStageSuccesses[static_cast<unsigned>(sourceLevel)] += levelSuccesses;
		tvcpStageEvents[static_cast<unsigned>(sourceLevel)] += levelEventTransitions;
		tvcpStageAccesses[static_cast<unsigned>(sourceLevel)] += 1ul;
		totalFrames++;
		totalRetained += compressed.retained.size();
		frameSerial++;
		size_t frontierPaths = 0ul;
		for (const auto& item: frontier) frontierPaths += item.second.size();
		maximumFrontierPaths = std::max(maximumFrontierPaths, frontierPaths);
		std::ostringstream levelLog;
		levelLog << std::setprecision(17)
		         << "KBRANCH_LEVEL"
		         << " invocation_id=" << oracle_safe_text(activeFixedEffortInvocationId)
		         << " frame=" << (frameSerial-1ul)
		         << " source_level=" << sourceLevel
		         << " parent_count=" << parents.size()
		         << " continuation_count=" << continuations
		         << " candidate_count=" << frame.size()
		         << " active_candidate_count=" << compressed.positive_active_count
		         << " effective_k=" << compressed.effective_k
		         << " retained_count=" << compressed.retained.size()
		         << " direct_top_mass=" << compressed.direct_top_mass
		         << " active_input_mass=" << compressed.active_input_mass
		         << " retained_output_mass=" << compressed.retained_output_mass
		         << " terminal_top_mass=" << terminalTopMass
		         << " terminal_dead_mass=" << terminalDeadMass
		         << " level_success_count=" << levelSuccesses
		         << " event_transition_count=" << levelEventTransitions
		         << " frontier_path_count=" << frontierPaths;
		ModelSuite::tech_log(levelLog.str() + "\n");
	}

	for (auto& item: frontier)
		for (auto& particle: item.second)
			tpool.return_traial(particle.traial);
	frontier.clear();

	if (tvcpNestedDiagnostic) {
		auto cloneParticles = [&](const std::vector<LiveParticle>& source)
				-> std::vector<LiveParticle> {
			std::vector<LiveParticle> result;
			result.reserve(source.size());
			for (const auto& particle: source) {
				Traial& clone(tpool.get_traial());
				clone = particle.traial.get();
				result.emplace_back(clone, particle.mass, particle.parentId,
					particle.candidateId, particle.offspringOrdinal,
					particle.factorSequence, particle.lineageLevels,
					particle.eventCount);
			}
			return result;
		};
		auto cloneFrontier = [&](const LiveFrontier& source) -> LiveFrontier {
			LiveFrontier result;
			for (const auto& item: source)
				result[item.first] = cloneParticles(item.second);
			return result;
		};
		auto releaseFrontier = [&](LiveFrontier& source) {
			for (auto& item: source)
				for (auto& particle: item.second)
					tpool.return_traial(particle.traial);
			source.clear();
		};
		auto releaseSnapshots = [&]() {
			for (auto& activation: tvcpNestedSnapshots) {
				for (auto& particle: activation.second)
					tpool.return_traial(particle.traial);
				activation.second.clear();
			}
		};
		auto diagnosticSeed = [&](unsigned level, size_t outer, size_t inner,
		                          unsigned phase) -> unsigned long {
			unsigned long long value = baseSeed;
			value ^= 0x9e3779b97f4a7c15ull * (runSerial + 1ull);
			value ^= 0xbf58476d1ce4e5b9ull * (static_cast<unsigned long long>(level) + 1ull);
			value ^= 0x94d049bb133111ebull * (static_cast<unsigned long long>(outer) + 1ull);
			value ^= 0xd6e8feb86659fd93ull * (static_cast<unsigned long long>(inner) + 1ull);
			value ^= 0xa0761d6478bd642full * static_cast<unsigned long long>(phase + 1u);
			unsigned long result = static_cast<unsigned long>(value & 0xffffffffull);
			return result == 0ul ? 1ul : result;
		};

		// Execute exactly one formal KBRANCH activation.  The caller controls
		// the clock RNG and pivotal RNG, so outer and inner streams are disjoint.
		auto diagnosticAdvance = [&](LiveFrontier& diagFrontier,
		                             std::mt19937_64& diagPlanner) -> double {
			if (diagFrontier.empty()) return 0.0;
			auto levelIt = diagFrontier.begin();
			const ImportanceValue sourceLevel = levelIt->first;
			std::vector<LiveParticle> parents(std::move(levelIt->second));
			diagFrontier.erase(levelIt);
			std::vector<double> parentMasses;
			for (const auto& parent: parents) {
				if (!std::isfinite(parent.mass) || parent.mass <= 0.0 ||
				    parent.traial.get().level != sourceLevel)
					throw_FigException("invalid TVCP nested frontier parent");
				parentMasses.push_back(parent.mass);
			}
			const size_t continuations = levelEffort(sourceLevel);
			const auto allocation = kbranch_allocate_continuations(parentMasses, continuations);
			std::vector<KBranchFrameCandidate> frame;
			std::vector<Traial*> liveCandidates;
			frame.reserve(continuations);
			liveCandidates.reserve(continuations);
			for (size_t parentIndex = 0ul; parentIndex < parents.size(); ++parentIndex) {
				const LiveParticle& parent(parents[parentIndex]);
				const size_t offspringCount = allocation[parentIndex];
				if (offspringCount == 0ul)
					throw_FigException("TVCP nested diagnostic deleted a positive parent");
				const double childMass = parent.mass / static_cast<double>(offspringCount);
				for (size_t ordinal = 0ul; ordinal < offspringCount; ++ordinal) {
					Traial& child(tpool.get_traial());
					if (resampleOnSplit_) child.copyResampling(parent.traial.get());
					else child = parent.traial.get();
					child.level = sourceLevel;
					child.depth = 0;
					child.weight = childMass;
					model_->simulation_step(child, *property_, watch_events);
					const bool directTop = property_->is_rare(child.state);
					const bool terminalDead = !directTop &&
						(property_->is_stop(child.state) || child.level <= sourceLevel);
					if (!directTop && !terminalDead && child.level <= sourceLevel)
						throw_FigException("TVCP nested continuation did not advance");
					const size_t candidateIndex = frame.size();
					std::vector<unsigned> lineage = parent.lineageLevels;
					if (lineage.empty() || lineage.back() != static_cast<unsigned>(sourceLevel))
						lineage.push_back(static_cast<unsigned>(sourceLevel));
					frame.emplace_back(candidateIndex, static_cast<unsigned>(child.level),
						parent.mass, 1.0 / static_cast<double>(offspringCount), childMass,
						directTop, terminalDead, parent.candidateId, "tvcp_nested",
						"tvcp_nested", lineage, 0ul);
					liveCandidates.push_back((directTop || terminalDead) ? nullptr : &child);
					if (directTop || terminalDead)
						tpool.return_traial(std::move(child));
				}
			}
			for (auto& parent: parents) tpool.return_traial(parent.traial);
			const KBranchFrameResult compressed =
				kbranch_compress_frame(frame, requestedK, diagPlanner);
			std::vector<bool> selected(frame.size(), false);
			for (const auto& retained: compressed.retained) {
				if (retained.candidate_index >= liveCandidates.size() ||
				    nullptr == liveCandidates[retained.candidate_index])
					throw_FigException("TVCP nested pivotal retained invalid state");
				selected[retained.candidate_index] = true;
				Traial& child(*liveCandidates[retained.candidate_index]);
				child.weight = retained.propagated_mass;
				diagFrontier[static_cast<ImportanceValue>(retained.reached_level)].emplace_back(
					child, retained.propagated_mass, retained.parent_id,
					retained.candidate_id, 0ul, retained.factor_sequence,
					retained.lineage_levels, retained.event_count);
			}
			for (size_t i = 0ul; i < liveCandidates.size(); ++i)
				if (nullptr != liveCandidates[i] && !selected[i])
					tpool.return_traial(std::move(*liveCandidates[i]));
			return compressed.direct_top_mass;
		};

		for (auto& activation: tvcpNestedSnapshots) {
			const unsigned level = activation.first;
			const size_t population = levelEffort(static_cast<ImportanceValue>(level));
			for (size_t outer = 0ul; outer < tvcpNestedOuter; ++outer) {
				LiveFrontier post;
				post[static_cast<ImportanceValue>(level)] = cloneParticles(activation.second);
				const unsigned long outerSeed = diagnosticSeed(level, outer, 0ul, 0u);
				for (const auto& clockRef: tvcpNestedModelClocks)
					clockRef.get().reset_distribution_rng_state();
				Clock::reseed_for_diagnostics(outerSeed);
				std::mt19937_64 outerPlanner(static_cast<unsigned long long>(outerSeed) ^
					0x243f6a8885a308d3ull);
				const double immediateTop = diagnosticAdvance(post, outerPlanner);
				for (size_t inner = 0ul; inner < tvcpNestedInner; ++inner) {
					LiveFrontier downstream = cloneFrontier(post);
					const unsigned long innerSeed = diagnosticSeed(level, outer, inner, 1u);
					for (const auto& clockRef: tvcpNestedModelClocks)
						clockRef.get().reset_distribution_rng_state();
					Clock::reseed_for_diagnostics(innerSeed);
					std::mt19937_64 innerPlanner(static_cast<unsigned long long>(innerSeed) ^
						0x13198a2e03707344ull);
					double returned = immediateTop;
					while (!downstream.empty() && !interrupted)
						returned += diagnosticAdvance(downstream, innerPlanner);
					if (interrupted) {
						releaseFrontier(downstream);
						releaseFrontier(post);
						releaseSnapshots();
						ThresholdsPathCandidates(1ul).swap(result);
						result.front().emplace_back(initialLevel, terminalTopMass);
						ModelSuite::tech_log(
							"TVCP_NESTED_INCOMPLETE diagnostic_probability_discarded=1\n");
						return;
					}
					std::ostringstream row;
					row << std::setprecision(17)
					    << "TVCP_NESTED_RETURN"
					    << " root_id=" << oracle_safe_text(activeFixedEffortInvocationId)
					    << " level=" << level
					    << " activation_id=" << oracle_safe_text(activeFixedEffortInvocationId)
					    << ":" << level
					    << " population=" << population
					    << " outer_id=" << outer
					    << " inner_id=" << inner
					    << " return_value=" << returned
					    << " diagnostic_probability_discarded=1";
					ModelSuite::tech_log(row.str() + "\n");
				}
				releaseFrontier(post);
			}
			for (auto& particle: activation.second)
				tpool.return_traial(particle.traial);
			activation.second.clear();
		}
		std::ostringstream nestedSummary;
		nestedSummary << "TVCP_NESTED_SUMMARY"
		              << " root_id=" << oracle_safe_text(activeFixedEffortInvocationId)
		              << " tvcp_nested_outer_replicates=" << tvcpNestedOuter
		              << " tvcp_nested_inner_replicates=" << tvcpNestedInner
		              << " diagnostic_probability_discarded=1";
		ModelSuite::tech_log(nestedSummary.str() + "\n");
	}
	if (!std::isfinite(terminalTopMass) || terminalTopMass < 0.0)
		throw_FigException("KBRANCH terminal top mass is invalid");
	ThresholdsPathCandidates(1ul).swap(result);
	result.front().emplace_back(initialLevel, terminalTopMass);
	if (tvcpDiagnosticOnly)
		ModelSuite::tech_log(
			"TVCP_DIAGNOSTIC_RESULT estimate_discarded=1 probability_eligible=0\n");
	std::ostringstream summary;
	summary << std::setprecision(17)
	        << "KBRANCH_RUN"
	        << " invocation_id=" << oracle_safe_text(activeFixedEffortInvocationId)
	        << " k=" << requestedK
	        << " estimate=" << terminalTopMass
	        << " terminal_top_mass=" << terminalTopMass
	        << " terminal_dead_mass=" << terminalDeadMass
	        << " frame_count=" << totalFrames
	        << " continuation_count=" << totalContinuations
	        << " retained_count=" << totalRetained
	        << " direct_top_count=" << totalDirectTop
	        << " jump_count=" << totalJumps
	        << " max_frontier_paths=" << maximumFrontierPaths
	        << " event_transition_count=" << totalEventTransitions;
	if (tvcpEnabled) summary << " tvcp_enabled=1 tvcp_level_count=" << tvcpPopulations.size();
	bool firstTvcp=true;
	summary << " tvcp_stage_trials=";
	for(const auto& item:tvcpStageTrials){if(!firstTvcp)summary<<",";firstTvcp=false;summary<<item.first<<":"<<item.second;}
	summary << " tvcp_stage_successes="; firstTvcp=true;
	for(const auto& item:tvcpStageSuccesses){if(!firstTvcp)summary<<",";firstTvcp=false;summary<<item.first<<":"<<item.second;}
	summary << " tvcp_stage_events="; firstTvcp=true;
	for(const auto& item:tvcpStageEvents){if(!firstTvcp)summary<<",";firstTvcp=false;summary<<item.first<<":"<<item.second;}
	summary << " tvcp_stage_accesses="; firstTvcp=true;
	for(const auto& item:tvcpStageAccesses){if(!firstTvcp)summary<<",";firstTvcp=false;summary<<item.first<<":"<<item.second;}
	ModelSuite::tech_log(summary.str() + "\n");
	if (bats_sfe_count_only_enabled()) {
		std::ostringstream counts;
		counts << "SFE_COUNT_SUMMARY"
		       << " mode=kbranch_production"
		       << " adaptive_population_phase=disabled"
		       << " level_count=" << totalFrames
		       << " production_trajectory_count=" << totalContinuations
		       << " simulation_step_count=" << totalContinuations
		       << " event_transition_count=" << totalEventTransitions
		       << " threshold_build_trajectory_count=0"
		       << " threshold_build_simulation_step_count=0"
		       << " threshold_build_event_transition_count=0"
		       << " threshold_build_wall_time_ms=0"
		       << " level_wall_time_ms=0";
		ModelSuite::tech_log(counts.str() + "\n");
	}
	if (tvcpNestedDiagnostic &&
	    runSerial + 1ull >= bats_tvcp_nested_root_replicates())
		interrupted = true;
}


void
SimulationEngineSFE::fixed_effort(ThresholdsPathCandidates& result,
                                  const EventWatcher& watch_events) const
{
	assert(nullptr != impFun_);
	const std::string previousInvocationId = activeFixedEffortInvocationId;
	activeFixedEffortInvocationId = next_fixed_effort_invocation_id();
	struct InvocationGuard {
		std::string previous;
		~InvocationGuard()
		{
			completedFixedEffortInvocationId = activeFixedEffortInvocationId;
			activeFixedEffortInvocationId = previous;
		}
	} invocationGuard{previousInvocationId};
	if (!bats_kbranch_production_enabled() && oracle_env_flag("TVCP_RUN_AUDIT")) {
		std::ostringstream actualThresholdChain;
		const auto& thresholds = impFun_->thresholds();
		for (size_t index = 1ul; index + 1ul < thresholds.size(); ++index) {
			if (index > 1ul) actualThresholdChain << ",";
			actualThresholdChain << thresholds[index].first;
		}
		std::ostringstream identity;
		identity << "TVCP_RUN_IDENTITY"
		         << " model_id=" << oracle_safe_text(oracle_env("TVCP_MODEL_ID", "-"))
		         << " seed=" << Clock::rng_seed()
		         << " plan=" << oracle_safe_text(oracle_env("TVCP_EFFECTIVE_PLAN", "-"))
		         << " threshold_chain=" << actualThresholdChain.str()
		         << " binary_sha256=" << oracle_safe_text(oracle_env("TVCP_BINARY_SHA256", "-"))
		         << " model_sha256=" << oracle_safe_text(oracle_env("TVCP_MODEL_SHA256", "-"))
		         << " manifest_sha256=" << oracle_safe_text(oracle_env("TVCP_MANIFEST_SHA256", "-"))
		         << " diagnostic_only=0";
		ModelSuite::tech_log(identity.str() + "\n");
	}
	const bool tvcpDegenerateFixed = !toBuildThresholds_ &&
		bats_tvcp_degenerate_fixed_enabled();
	if (tvcpDegenerateFixed) {
		ModelSuite::tech_log(
			"TVCP_DEGENERATE tvcp_enabled=1 decision_dimension=0 "
			"allocation_status=FIXED_DEGENERATE "
			"reason=NO_INTERMEDIATE_SPLITTING_LEVELS\n");
	}
	if (!tvcpDegenerateFixed && bats_kbranch_production_enabled() &&
	    !toBuildThresholds_) {
		fixed_effort_kbranch(result, watch_events);
		return;
	}
	if (!tvcpDegenerateFixed && bats_tvcp_enabled() && !toBuildThresholds_)
		throw_FigException("BATS_TVCP requires BATS_KBRANCH production mode");

	auto lvl_effort = [&](const unsigned& n) -> unsigned
		{ return toBuildThresholds_ ? arbitrary_effort(n) : n*base_nsims(); };
	auto max_lvl_effort = [&]() { return std::max(lvl_effort_min(),
												  lvl_effort(impFun_->max_thresholds_effort())); };
	const size_t LVL_MAX = toBuildThresholds_ ? arbitraryMaxLevel : impFun_->max_value(),
				 EFF_MAX = toBuildThresholds_ ? ThresholdsBuilderAdaptive::max_n() : max_lvl_effort(),
				 LVL_INI = impFun_->initial_value(toBuildThresholds_);
	const std::string sfePoolMode = toBuildThresholds_ ? "modal" : bats_sfe_pool_mode();
	decltype(reachCount_) reachCountLocal(reachCount_);
	auto tpool(TraialPool::get_instance());
	std::vector< Reference< Traial > > traialsNow, traialsNext;
	const bool oracleRequested = oracle_env_flag("N4Q_ORACLE_DIAG");
	const std::string oracleModel = oracle_env("N4Q_ORACLE_MODEL_ID");
	const std::string oracleSource = oracle_env("N4Q_ORACLE_SOURCE");
	const std::string oracleOutputDir = oracle_env("N4Q_ORACLE_OUTPUT_DIR");
	const std::string oracleSeedFile = oracle_env("N4Q_ORACLE_SEED_FILE");
	const std::string oracleRunKey = oracleOutputDir + "|" + oracleModel + "|" + oracleSource;
	const bool oracleActive = oracleRequested && !toBuildThresholds_ &&
		!oracleModel.empty() && !oracleSource.empty() &&
		!oracleOutputDir.empty() && !oracleSeedFile.empty() &&
		oracle_claim_run(oracleRunKey);
	const bool clfOracleRequested = oracle_env_flag("CLF_ORACLE_DIAG");
	const std::string clfOracleModel = oracle_env("CLF_ORACLE_MODEL_ID");
	const std::string clfOracleSource = oracle_env("CLF_ORACLE_SOURCE", "C6H");
	const std::string clfOracleOutputDir = oracle_env("CLF_ORACLE_OUTPUT_DIR");
	const std::string clfOracleSeedFile = oracle_env("CLF_ORACLE_SEED_FILE");
	const std::string clfOracleRunKey = clfOracleOutputDir + "|" +
		clfOracleModel + "|" + clfOracleSource;
	const bool clfOracleActive = clfOracleRequested && !toBuildThresholds_ &&
		!clfOracleModel.empty() && !clfOracleSource.empty() &&
		!clfOracleOutputDir.empty() && !clfOracleSeedFile.empty() &&
		oracle_claim_run("CLF|" + clfOracleRunKey);
	const bool saclfOracleRequested = oracle_env_flag("SACLF_ORACLE_DIAG");
	const std::string saclfOracleModel = oracle_env("SACLF_ORACLE_MODEL_ID");
	const std::string saclfOracleSource = oracle_env("SACLF_ORACLE_SOURCE", "C3");
	const std::string saclfOracleSourceSeed = oracle_env("SACLF_ORACLE_SOURCE_SEED",
		oracle_env("RNG_SEED"));
	const std::string saclfOracleOutputDir = oracle_env("SACLF_ORACLE_OUTPUT_DIR");
	const std::string saclfOracleSeedFile = oracle_env("SACLF_ORACLE_SEED_FILE");
	const std::string saclfOracleRunKey = saclfOracleOutputDir + "|" +
		saclfOracleModel + "|" + saclfOracleSource + "|" + saclfOracleSourceSeed;
	const bool saclfOracleActive = saclfOracleRequested && !toBuildThresholds_ &&
		!saclfOracleModel.empty() && !saclfOracleSource.empty() &&
		!saclfOracleOutputDir.empty() && !saclfOracleSeedFile.empty() &&
		oracle_claim_run("SACLF|" + saclfOracleRunKey);
	const bool clfAtlasRequested = oracle_env_flag("CLF_ATLAS_DIAG");
	const std::string clfAtlasModel = oracle_env("CLF_ATLAS_MODEL_ID");
	const std::string clfAtlasSource = oracle_env("CLF_ATLAS_SOURCE", "C3");
	const std::string clfAtlasSourceSeed = oracle_env("CLF_ATLAS_SOURCE_SEED",
		oracle_env("RNG_SEED"));
	const std::string clfAtlasOutputDir = oracle_env("CLF_ATLAS_OUTPUT_DIR");
	const std::string clfAtlasSeedFile = oracle_env("CLF_ATLAS_SEED_FILE");
	const std::string clfAtlasRunKey = clfAtlasOutputDir + "|" +
		clfAtlasModel + "|" + clfAtlasSource + "|" + clfAtlasSourceSeed;
	const bool clfAtlasActive = clfAtlasRequested && !toBuildThresholds_ &&
		!clfAtlasModel.empty() && !clfAtlasSource.empty() &&
		!clfAtlasOutputDir.empty() && !clfAtlasSeedFile.empty() &&
		oracle_claim_run("CLF_ATLAS|" + clfAtlasRunKey);
	if ((oracleActive ? 1 : 0) + (clfOracleActive ? 1 : 0) +
	    (saclfOracleActive ? 1 : 0) + (clfAtlasActive ? 1 : 0) > 1)
		throw_FigException("only one Oracle diagnostic mode can run at a time");
	const bool oracleDiagnosticRequested =
		oracleRequested || clfOracleRequested ||
		saclfOracleRequested || clfAtlasRequested;
	const bool branchFkObserve =
		bats_branch_fk_observe_enabled() && !toBuildThresholds_;
	const size_t branchFkKeepK = branchFkObserve
		? bats_branch_fk_keep_k() : 0ul;
	const bool branchFkShadowPlan = branchFkObserve &&
		bats_branch_fk_shadow_plan_enabled();
	if (branchFkObserve && oracleDiagnosticRequested)
		throw_FigException(
			"BATS_BRANCH_FK_OBSERVE cannot be combined with Oracle diagnostics");
	const bool sstaStage1Requested =
		oracle_env_flag("BATS_SSTA_STATE_ALLOCATION");
	const bool fbSstaRequested =
		oracle_env_flag("BATS_FB_SSTA_STATE_ALLOCATION");
	if (sstaStage1Requested && fbSstaRequested)
		throw_FigException(
			"legacy SSTA and FB-SSTA state allocation are mutually exclusive");
	const bool weightedStage0Requested =
		bats_ssta_stage0_weighted_enabled() || sstaStage1Requested ||
		fbSstaRequested;
	const bool weightedStage0Eligible = weighted_sfe_stage0_eligible(
		toBuildThresholds_, sfePoolMode, oracleDiagnosticRequested);
	if (weightedStage0Requested && !toBuildThresholds_ &&
	    !adaptivePopulationCalibrationPhase_ && !weightedStage0Eligible)
		throw_FigException(
			"BATS_SSTA_STAGE0_WEIGHTED requires formal transient modal SFE without Oracle or CLF diagnostic modes");
	const bool weightedStage0Active =
		weightedStage0Requested && weightedStage0Eligible &&
		!adaptivePopulationCalibrationPhase_;
	const bool sstaStage1Active =
		sstaStage1Requested && weightedStage0Eligible &&
		!adaptivePopulationCalibrationPhase_;
	const bool fbSstaActive =
		fbSstaRequested && weightedStage0Eligible &&
		!adaptivePopulationCalibrationPhase_;
	const bool stateAllocationActive = sstaStage1Active || fbSstaActive;
	const bool sstaStage1Diagnostics =
		stateAllocationActive &&
		(oracle_env_flag("BATS_SSTA_STAGE1_DIAG") ||
		 oracle_env_flag("BATS_FB_SSTA_DIAG"));
	const size_t sstaStage1DiagStride =
		std::max<size_t>(1ul, oracle_env_size("BATS_FB_SSTA_DIAG_STRIDE", 1000ul));
	const size_t sstaStage1DiagInitial =
		oracle_env_size("BATS_FB_SSTA_DIAG_INITIAL", 5ul);
	const size_t sstaRunIndex = stateAllocationActive
		? sequentialStateRunIndex_++ : 0ul;
	const size_t oracleLevelEffort = std::max<size_t>(
		10ul, oracle_env_size("N4Q_ORACLE_LEVEL_EFFORT", 200ul));
	const size_t clfOracleLevelEffort = std::max<size_t>(
		10ul, oracle_env_size("CLF_ORACLE_LEVEL_EFFORT", 200ul));
	const size_t saclfOracleLevelEffort = std::max<size_t>(
		10ul, oracle_env_size("SACLF_ORACLE_LEVEL_EFFORT", 200ul));
	const size_t clfAtlasLevelEffort = std::max<size_t>(
		10ul, oracle_env_size("CLF_ATLAS_LEVEL_EFFORT", 200ul));
	const double oracleHorizon = std::max(
		0.0, oracle_env_double("N4Q_ORACLE_HORIZON", 0.0));
	const double clfOracleHorizon = std::max(
		0.0, oracle_env_double("CLF_ORACLE_HORIZON", 0.0));
	const double saclfOracleHorizon = std::max(
		0.0, oracle_env_double("SACLF_ORACLE_HORIZON", 0.0));
	const double clfAtlasHorizon = std::max(
		0.0, oracle_env_double("CLF_ATLAS_HORIZON", 0.0));
	const auto oracleStateNames(model_->global_state().varnames());
	const std::string diagnosticRunId = next_parent_diag_run_id();
	activeParentDiagRunId = diagnosticRunId;
	std::array<TraialVec,3> oracleSelected;
	std::array<TraialVec,3> oracleFallback;
	std::array<std::unordered_set<std::string>,3> oracleSeenPaths;
	TraialVec clfOracleSamples;
	std::vector<ClfOracleSampleMeta> clfOracleMetas;
	TraialVec saclfOracleSamples;
	std::vector<SaclfOracleCandidateMeta> saclfOracleMetas;
	std::vector<SaclfOracleCandidateMeta> saclfOracleDecisionMetas;
	TraialVec clfAtlasCandidates;
	std::vector<ClfAtlasCandidateMeta> clfAtlasMetas;
	std::vector<ClfAtlasPathMeta> clfAtlasPathMetas;
	std::array<size_t,2> clfOracleSideCounts = {{0ul, 0ul}};
	std::array<std::array<size_t,3>,2> clfOracleStratumCounts =
		{{{{0ul, 0ul, 0ul}}, {{0ul, 0ul, 0ul}}}};
	std::unordered_set<std::string> clfOracleSeenSampleKeys;
	auto oracleClone = [&](TraialVec& destination, const Traial& source) {
		Traial& snapshot(tpool.get_traial());
		snapshot = source;
		destination.push_back(snapshot);
	};
	auto oracleCapture = [&](const Traial& candidate, ImportanceValue currentLvl) {
		if (!oracleActive)
			return;
		const double fraction = oracleHorizon > 0.0
			? static_cast<double>(candidate.lifeTime) / oracleHorizon
			: (LVL_MAX > 0ul
			   ? static_cast<double>(currentLvl) / static_cast<double>(LVL_MAX)
			   : 0.0);
		const size_t stratum = std::min<size_t>(2ul,
			static_cast<size_t>(std::floor(3.0 * std::max(0.0, fraction))));
		const std::string signature(oracle_path_signature(candidate, oracleStateNames));
		if (oracleSelected[stratum].size() < 10ul &&
		    oracleSeenPaths[stratum].insert(signature).second) {
			oracleClone(oracleSelected[stratum], candidate);
		} else if (oracleFallback[stratum].size() < 10ul) {
			oracleClone(oracleFallback[stratum], candidate);
		}
	};
	const EventWatcher oracleWatchEvents = [&](const Property& property,
	                                            Traial& candidate,
	                                            Event& event) {
		oracleCapture(candidate, candidate.level);
		return watch_events(property, candidate, event);
	};

	// Init result & internal ADTs
	ThresholdsPathCandidates(1).swap(result);
	auto& pathToRare(result.front());
	assert(pathToRare.empty());
	pathToRare.reserve(LVL_MAX);
	traialsNow.reserve(EFF_MAX);
	traialsNext.reserve(EFF_MAX);

	// Bootstrap the Fixed Effort run
	Traial& seedTraial(tpool.get_traial());
	seedTraial.initialise(*model_, *impFun_);
	traialsNext.push_back(seedTraial);
	std::vector<std::string> branchFkParentLineages;
	if (branchFkObserve)
		branchFkParentLineages.push_back(
			activeFixedEffortInvocationId + ":seed");
	size_t numSuccesses;
	size_t numRareSuccesses;
	size_t numRareOnlySuccesses;
	ImportanceValue lvl = LVL_INI;
	size_t diagStep = 0ul;
	size_t countSummaryProductionTrajectories = 0ul;
	size_t countSummarySimulationSteps = 0ul;
	size_t countSummaryEventTransitions = 0ul;
	double countSummaryWallTimeMs = 0.0;
	const std::map<size_t,size_t> xiwangPlan(bats_xiwang_plan());

	// Run Fixed Effort: For each threshold level 'lvl' ...
	do {
		const auto levelWallStart = std::chrono::steady_clock::now();
        // ... prepare the Traials to run the simulations ...
        numSuccesses = 0ul;
		numRareSuccesses = 0ul;
		numRareOnlySuccesses = 0ul;
		const auto LE = toBuildThresholds_ ? lvl : std::max<size_t>(lvl_effort_min(), impFun_->effort_of(lvl));
		const size_t nominalLevelEffort = lvl_effort(static_cast<unsigned>(LE));
		const bool adaptivePopulationActive =
			!toBuildThresholds_ &&
			(bats_adaptive_level_population_enabled() || bats_bsrtp_enabled());
		const bool vcstpActive = !toBuildThresholds_ && bats_vcstp_enabled();
		const bool cslaActive = !toBuildThresholds_ && bats_csla_enabled();
		const size_t fixedLevelPopulation = bats_level_population();
		if (adaptivePopulationActive && fixedLevelPopulation > 0ul)
			throw_FigException(
				"BATS_ADAPTIVE_LEVEL_POPULATION and BATS_LEVEL_POPULATION are mutually exclusive");
		size_t requestedLevelPopulation = fixedLevelPopulation;
		std::string adaptivePopulationPhase = "disabled";
		if (xiwangOnlineRuntime_.enabled()) {
			requestedLevelPopulation = xiwangOnlineRuntime_.population(
				static_cast<size_t>(lvl), nominalLevelEffort);
			adaptivePopulationPhase = "xiwang_online_formal";
		} else if (!xiwangPlan.empty()) {
			requestedLevelPopulation = bats_xiwang_population(
				xiwangPlan, static_cast<size_t>(lvl), nominalLevelEffort);
			adaptivePopulationPhase = "xiwang_production";
		}
		if (vcstpActive) {
			const std::vector<size_t>& populations = vcstpBatchIndex_ == 0ul
				? vcstpProblem_.fixed_populations : vcstpPlan_.populations;
			requestedLevelPopulation = static_cast<size_t>(lvl) < populations.size()
				? populations[static_cast<size_t>(lvl)] : nominalLevelEffort;
			adaptivePopulationPhase = "vcstp_production";
		}
		if (cslaActive && cslaBatchIndex_ > 0ul && cslaPlan_.valid) {
			requestedLevelPopulation = static_cast<size_t>(lvl) <
				cslaPlan_.populations.size()
				? cslaPlan_.populations[static_cast<size_t>(lvl)]
				: nominalLevelEffort;
			adaptivePopulationPhase = "csla_production";
		}
		if (adaptivePopulationActive) {
			if (adaptivePopulationCalibrationPhase_) {
				requestedLevelPopulation =
					bats_adaptive_calibration_population();
				adaptivePopulationPhase = "calibration";
			} else if (bats_rolling_level_optimization_enabled() &&
			           !rollingPopulationPlan_.populations.empty()) {
				requestedLevelPopulation =
					static_cast<size_t>(lvl) < rollingPopulationPlan_.populations.size()
					? rollingPopulationPlan_.populations[static_cast<size_t>(lvl)]
					: rollingPopulationConfig_.max_population;
				adaptivePopulationPhase = rollingPopulationPlan_.fallback
					? "rolling_fallback" : "rolling_production";
			} else if (bats_bsrtp_enabled() && bsrtpPlan_.valid) {
				requestedLevelPopulation = static_cast<size_t>(lvl) <
					bsrtpPlan_.populations.size()
					? bsrtpPlan_.populations[static_cast<size_t>(lvl)]
					: nominalLevelEffort;
				adaptivePopulationPhase = "bsrtp_production";
			} else if (adaptivePopulationPlan_.finalized()) {
				requestedLevelPopulation =
					adaptivePopulationPlan_.population(lvl);
				adaptivePopulationPhase = "production";
			} else {
				throw_FigException(
					"adaptive level population requested before calibration");
			}
		}
		const bool populationOverrideActive =
			!toBuildThresholds_ && requestedLevelPopulation > 0ul;
		const size_t productionLevelEffort = populationOverrideActive
			? requestedLevelPopulation : nominalLevelEffort;
		const size_t diagnosticLevelEffort = oracleActive
			? oracleLevelEffort
			: (clfOracleActive ? clfOracleLevelEffort
			   : (saclfOracleActive ? saclfOracleLevelEffort
			      : (clfAtlasActive ? clfAtlasLevelEffort : 0ul)));
		const bool oracleDiagnosticActive =
			oracleActive || clfOracleActive || saclfOracleActive || clfAtlasActive;
		const size_t LVL_EFFORT = oracleDiagnosticActive
			? std::max(diagnosticLevelEffort, productionLevelEffort)
			: productionLevelEffort;
		std::string populationSource = vcstpActive ? "vcstp"
			: (cslaActive
			? (cslaBatchIndex_ == 0ul ? "csla_fixed_observation" : "csla")
			: (adaptivePopulationActive
			? (bats_rolling_level_optimization_enabled()
			   ? "rolling_level_optimization" : "adaptive_level_population")
			: (populationOverrideActive ? "bats_level_population" : "legacy")));
		if (bats_bsrtp_enabled())
			populationSource = "bsrtp";
		if (!xiwangPlan.empty())
			populationSource = "xiwang";
		if (oracleDiagnosticActive && diagnosticLevelEffort > productionLevelEffort)
			populationSource = "oracle_diag";
		if (LVL_EFFORT == 0ul && toBuildThresholds_) {
			// we deviate from a designated path: abort simulation
			pathToRare.clear();
			result.clear();
			break;
		}
		assert(LVL_EFFORT > 0ul);
		const double weightedChildMass =
			weightedStage0Active && !stateAllocationActive
			? uniform_local_child_mass(LVL_EFFORT) : 0.0;
		WeightedSfeMassAccumulator weightedMasses;
		assert(traialsNow.empty());
		assert(!traialsNext.empty());
		const size_t availableParentCount = traialsNext.size();
		std::vector<std::string> parentDiagSignatures;
		std::vector<double> parentDiagWeights;
		struct ParentDiagAccum {
			size_t childCount = 0ul;
			size_t successCount = 0ul;
			size_t advancedCount = 0ul;
			size_t rareOnlyCount = 0ul;
			size_t eventCount = 0ul;
			double weightSum = 0.0;
			double contributionSum = 0.0;
			double contributionSqSum = 0.0;
			double eventCountSqSum = 0.0;
		};
		std::map<std::string, ParentDiagAccum> parentDiagAccum;
		if (bats_sfe_parent_diag_enabled()) {
			parentDiagSignatures.reserve(availableParentCount);
			parentDiagWeights.reserve(availableParentCount);
			for (size_t parentIndex = 0ul;
			     parentIndex < availableParentCount; ++parentIndex) {
				const auto& parentRef = traialsNext[parentIndex];
				const Traial& parent(parentRef.get());
				parentDiagSignatures.push_back(
					oracle_path_signature(parent, oracleStateNames));
				parentDiagWeights.push_back(parent.weight);
				std::ostringstream diag;
				 diag << std::setprecision(17)
				     << "SFE_PARENT_FREEZE"
				     << " fixed_effort_invocation_id="
				     << oracle_safe_text(activeFixedEffortInvocationId)
				     << " diagnostic_run_id=" << oracle_safe_text(activeParentDiagRunId)
				     << " batch_id=" << activeParentDiagBatchId
				     << " level=" << lvl
				     << " batch_step=" << diagStep
				     << " parent_index=" << parentIndex
				     << " parent_signature="
				     << oracle_safe_text(parentDiagSignatures.back())
				     << " parent_weight=" << parent.weight
				     << " parent_level=" << parent.level
				     << " parent_lifetime=" << parent.lifeTime
				     << " parent_state="
				     << oracle_safe_text(oracle_state_values(parent, oracleStateNames))
				     << " child_factor_scope=local_continuation_only"
				     << " frozen_conditional_factor_sequence=";
				bool firstFrozenFactor = true;
				for (const auto& conditional: pathToRare) {
					if (!firstFrozenFactor)
						diag << ";";
					firstFrozenFactor = false;
					diag << conditional.first << ":" << conditional.second;
				}
				 diag << "";
				bats_sfe_parent_diag_log(diag.str());
			}
		}
		std::vector<SstaParentGroup> sstaGroups;
		std::vector<std::vector<SstaChildSpec>> sstaChildSpecs;
		std::vector<std::string> sstaChildGroupKeys;
		std::vector<std::string> parentDiagChildSignatures;
		if (bats_sfe_parent_diag_enabled())
			parentDiagChildSignatures.reserve(LVL_EFFORT);
		std::map<std::string,SstaGroupBatchOutcome> sstaBatchOutcomes;
		std::vector<size_t> fixedParentChildren;
		std::string sstaAllocationFallback("none");
		if (stateAllocationActive) {
			std::map<std::string,SstaParentGroup> grouped;
			bool fbGroupsMerged = false;
			for (size_t parent = 0ul; parent < availableParentCount; ++parent) {
				const Traial& candidate(traialsNext[parent].get());
				if (!std::isfinite(candidate.weight) || candidate.weight < 0.0)
					throw_FigException(
						"SSTA parent mass must be finite and non-negative");
				if (candidate.weight == 0.0)
					continue;
				const std::string signature(fbSstaActive
					? fb_ssta_state_signature(candidate, oracleStateNames, lvl)
					: ssta_pand_state_signature(oracleStateNames, candidate.state));
				SstaParentGroup& group(grouped[signature]);
				group.signature = signature;
				group.parentIndices.push_back(parent);
				group.parentWeights.push_back(candidate.weight);
				group.mass += candidate.weight;
			}
			const size_t fbMaxGroups = fbSstaActive
				? std::min(LVL_EFFORT, std::max<size_t>(1ul, oracle_env_size(
					"BATS_FB_SSTA_MAX_GROUPS", 8ul))) : grouped.size();
			if (fbSstaActive && grouped.size() > fbMaxGroups) {
				std::vector<std::string> order;
				for (const auto& item: grouped)
					order.push_back(item.first);
				std::stable_sort(order.begin(), order.end(),
					[&](const std::string& lhs, const std::string& rhs) {
						if (grouped[lhs].mass != grouped[rhs].mass)
							return grouped[lhs].mass > grouped[rhs].mass;
						return lhs < rhs;
					});
				const size_t keep = fbMaxGroups > 1ul ? fbMaxGroups - 1ul : 0ul;
				SstaParentGroup other;
				other.signature = "__other__";
				for (size_t i = keep; i < order.size(); ++i) {
					SstaParentGroup& source(grouped[order[i]]);
					other.mass += source.mass;
					other.parentIndices.insert(other.parentIndices.end(),
						source.parentIndices.begin(), source.parentIndices.end());
					other.parentWeights.insert(other.parentWeights.end(),
						source.parentWeights.begin(), source.parentWeights.end());
				}
				std::map<std::string,SstaParentGroup> compact;
				for (size_t i = 0ul; i < keep; ++i)
					compact.emplace(order[i], grouped[order[i]]);
				compact.emplace(other.signature, other);
				grouped.swap(compact);
				fbGroupsMerged = true;
			}
			if (fbSstaActive) {
				const double mergeThreshold = std::max(0.0,
					oracle_env_double("BATS_FB_SSTA_MERGE_MASS", 0.02));
				size_t rareGroups = 0ul;
				for (const auto& item: grouped)
					if (item.second.mass < mergeThreshold)
						++rareGroups;
				if (rareGroups >= 2ul) {
					std::map<std::string,SstaParentGroup> compact;
					SstaParentGroup other;
					other.signature = "__other__";
					for (const auto& item: grouped) {
						if (item.second.mass >= mergeThreshold) {
							compact.emplace(item.first, item.second);
							continue;
						}
						other.mass += item.second.mass;
						other.parentIndices.insert(other.parentIndices.end(),
							item.second.parentIndices.begin(),
							item.second.parentIndices.end());
						other.parentWeights.insert(other.parentWeights.end(),
							item.second.parentWeights.begin(),
							item.second.parentWeights.end());
					}
					compact.emplace(other.signature, other);
					grouped.swap(compact);
					fbGroupsMerged = true;
				}
			}
			std::vector<SstaGroupRequest> requests;
			std::vector<FbSstaGroupRequest> fbRequests;
			requests.reserve(grouped.size());
			fbRequests.reserve(grouped.size());
			fixedParentChildren.assign(availableParentCount, 0ul);
			for (size_t child = 0ul; child < LVL_EFFORT; ++child)
				fixedParentChildren[child % availableParentCount]++;
			for (auto& item: grouped) {
				if (fbSstaActive) {
					size_t fixedChildren = 0ul;
					for (const size_t parent: item.second.parentIndices)
						fixedChildren += fixedParentChildren[parent];
					item.second.fbHistory = fbSequentialStateHistory_.group(
						static_cast<size_t>(lvl), item.first);
					fbRequests.emplace_back(
						item.first, item.second.mass, item.second.fbHistory,
						fbSequentialStateHistory_.previous_allocation(
							static_cast<size_t>(lvl), item.first),
						fixedChildren);
				} else {
					item.second.history = sequentialStateHistory_.get(
						static_cast<size_t>(lvl), item.first);
					requests.emplace_back(
						item.first, item.second.mass, item.second.history);
				}
				sstaGroups.push_back(item.second);
			}
			SstaAllocationPlan allocation;
			FbSstaAllocationPlan fbAllocation;
			if (fbSstaActive) {
				FbSstaConfig config;
				config.max_groups = fbMaxGroups;
				config.merge_mass_threshold = 0.0;
				config.max_group_share = std::max(0.05, std::min(1.0,
					oracle_env_double("BATS_FB_SSTA_MAX_GROUP_SHARE",
					                  config.max_group_share)));
				config.max_batch_change_share = std::max(0.0, std::min(1.0,
					oracle_env_double("BATS_FB_SSTA_MAX_BATCH_CHANGE",
					                  config.max_batch_change_share)));
				fbAllocation = fb_ssta_allocate_groups(
					LVL_EFFORT, fbRequests, config);
			} else {
				allocation = ssta_allocate_groups(LVL_EFFORT, requests, 0.02);
			}
			sstaChildSpecs.resize(availableParentCount);
			if ((fbSstaActive && fbAllocation.valid) ||
			    (!fbSstaActive && allocation.valid)) {
				for (size_t groupIndex = 0ul;
				     groupIndex < sstaGroups.size(); ++groupIndex) {
					SstaParentGroup& group(sstaGroups[groupIndex]);
					size_t assignedChildren = 0ul;
					if (fbSstaActive) {
						const auto assignedIt = std::find_if(
							fbAllocation.groups.begin(), fbAllocation.groups.end(),
							[&](const FbSstaGroupAllocation& candidate) {
								return candidate.signature == group.signature;
							});
						if (assignedIt == fbAllocation.groups.end())
							throw_FigException(
								"FB-SSTA allocation lost a state group");
						const FbSstaGroupAllocation& assigned(*assignedIt);
						group.signature = assigned.signature;
						group.mass = assigned.mass;
						group.fbHistory = assigned.history;
						group.fbEstimates = assigned.estimates;
						group.advanceProbability = assigned.estimates.q;
						group.score = assigned.score;
						assignedChildren = assigned.children;
					} else {
						const SstaGroupAllocation& assigned(
							allocation.groups[groupIndex]);
						group.mass = assigned.mass;
						group.advanceProbability = assigned.advance_probability;
						group.score = assigned.score;
						assignedChildren = assigned.children;
					}
					group.allocatedChildren = assignedChildren;
					const double childMass =
						fb_ssta_child_mass(group.mass, assignedChildren);
					const double offset = ssta_resampling_offset(
						static_cast<size_t>(Clock::rng_seed()),
						sstaRunIndex, static_cast<size_t>(lvl), group.signature);
					const auto selected = ssta_systematic_resample(
						group.parentWeights, assignedChildren, offset);
					for (const size_t localParent: selected) {
						const size_t parent = group.parentIndices[localParent];
						sstaChildSpecs[parent].emplace_back(
							group.signature, childMass);
					}
				}
				if (fbSstaActive) {
					sstaAllocationFallback = fbAllocation.fallback_reason.empty()
						? "none" : fbAllocation.fallback_reason;
					if (fbAllocation.merged || fbGroupsMerged)
						sstaAllocationFallback += "+merged";
				}
			} else {
				sstaAllocationFallback = fbSstaActive
					? fbAllocation.fallback_reason : allocation.reason;
				std::vector<double> parentWeights;
				parentWeights.reserve(availableParentCount);
				for (const auto& parentRef: traialsNext)
					parentWeights.push_back(parentRef.get().weight);
				const double offset = ssta_resampling_offset(
					static_cast<size_t>(Clock::rng_seed()),
					sstaRunIndex, static_cast<size_t>(lvl),
					"__global_fallback__");
				const auto selected = ssta_systematic_resample(
					parentWeights, LVL_EFFORT, offset);
				for (const size_t parent: selected) {
					const Traial& selectedParent(traialsNext[parent].get());
					const std::string signature(fbSstaActive
						? fb_ssta_state_signature(selectedParent, oracleStateNames, lvl)
						: ssta_pand_state_signature(
							oracleStateNames, selectedParent.state));
					sstaChildSpecs[parent].emplace_back(
						signature, uniform_local_child_mass(LVL_EFFORT));
					for (auto& group: sstaGroups)
						if (group.signature == signature) {
							group.allocatedChildren++;
							break;
						}
				}
			}
			size_t plannedChildren = 0ul;
			for (const auto& parentSpecs: sstaChildSpecs)
				plannedChildren += parentSpecs.size();
			if (plannedChildren != LVL_EFFORT)
				throw_FigException(
					"SSTA state allocation did not preserve the level population");
			sstaChildGroupKeys.reserve(LVL_EFFORT);
		}
		const bool calibrationDiagnostics =
			bats_sfe_calibration_diag_enabled() &&
			!toBuildThresholds_ &&
			adaptivePopulationActive &&
			adaptivePopulationCalibrationPhase_;
		const bool resamplingDiagnostics =
			calibrationDiagnostics || bats_sfe_level_agg_diag_enabled() ||
			bats_sfe_parent_diag_enabled() || stateAllocationActive;
		std::vector<size_t> offspringCounts;
		size_t usedParentCount = 0ul;
		size_t offspringCountMax = 0ul;
		double offspringSquaredSum = 0.0;
		if (resamplingDiagnostics) {
			offspringCounts.assign(availableParentCount, 0ul);
			if (stateAllocationActive) {
				for (size_t parent = 0ul;
				     parent < availableParentCount; ++parent)
					offspringCounts[parent] = sstaChildSpecs[parent].size();
			} else {
				for (auto i = 0ul ; i < LVL_EFFORT ; i++)
					offspringCounts[i % availableParentCount]++;
			}
			for (const auto count: offspringCounts) {
				if (count > 0ul)
					usedParentCount++;
				offspringCountMax = std::max(offspringCountMax, count);
				offspringSquaredSum += static_cast<double>(count) *
				                       static_cast<double>(count);
			}
		}
		const size_t unusedParentCount =
			availableParentCount > usedParentCount
			? availableParentCount - usedParentCount : 0ul;
		const size_t newCopyCount = stateAllocationActive
			? (LVL_EFFORT > usedParentCount
			   ? LVL_EFFORT - usedParentCount : 0ul)
			: (LVL_EFFORT > availableParentCount
			   ? LVL_EFFORT - availableParentCount : 0ul);
		const double offspringCountMean = usedParentCount > 0ul
			? static_cast<double>(LVL_EFFORT) / usedParentCount : 0.0;
		const double resamplingEssAbs = offspringSquaredSum > 0.0
			? (static_cast<double>(LVL_EFFORT) * LVL_EFFORT) /
			  offspringSquaredSum : 0.0;
		const double resamplingEssFraction = LVL_EFFORT > 0ul
			? resamplingEssAbs / LVL_EFFORT : 0.0;
		const double maxOffspringShare = LVL_EFFORT > 0ul
			? static_cast<double>(offspringCountMax) / LVL_EFFORT : 0.0;
		const bool bootstrapLevel = availableParentCount == 1ul;
		TraialVec clfLevelCandidates;
		if (clfOracleActive || saclfOracleActive || clfAtlasActive)
			clfLevelCandidates.reserve(LVL_EFFORT);
		size_t levelSimulationStepCount = 0ul;
		size_t levelEventTransitionCount = 0ul;
		const bool levelCountDiagnostics =
			bats_diag_enabled() || bats_sfe_pool_diag_enabled() ||
			bats_sfe_count_only_enabled() ||
			bats_sfe_level_agg_diag_enabled() ||
			bats_sfe_parent_diag_enabled() ||
			branchFkObserve ||
			bats_rolling_level_opt_needs_event_counts() ||
			vcstpActive || cslaActive ||
			calibrationDiagnostics || stateAllocationActive;
		std::vector<size_t> parentSuccessCounts;
		std::vector<size_t> parentRareOnlyCounts;
		std::vector<ReachabilityCount> parentReachCounts;
		std::vector<BranchFkCandidate> branchFkCandidates;
		TraialVec branchFkCandidateSnapshots;
		std::vector<std::string> branchFkChildParentLineages;
		std::vector<size_t> branchFkChildParentIndices;
		std::vector<BranchFkLineagePoolEntry> branchFkNextLineages;
		if (branchFkObserve) {
			branchFkCandidates.reserve(LVL_EFFORT);
			branchFkCandidateSnapshots.reserve(LVL_EFFORT);
			branchFkChildParentLineages.reserve(LVL_EFFORT);
			branchFkChildParentIndices.reserve(LVL_EFFORT);
			branchFkNextLineages.reserve(LVL_EFFORT);
			std::string alignmentReason;
			std::vector<BranchFkLineagePoolEntry> parentLineageEntries;
			for (const auto& lineage: branchFkParentLineages)
				parentLineageEntries.emplace_back(lineage,
					static_cast<unsigned>(lvl));
			if (!branch_fk_lineage_pool_aligned(parentLineageEntries,
				availableParentCount, alignmentReason))
				throw_FigException("branch FK parent lineage pool is misaligned");
		}
		if (calibrationDiagnostics) {
			parentSuccessCounts.assign(availableParentCount, 0ul);
			parentRareOnlyCounts.assign(availableParentCount, 0ul);
			parentReachCounts.assign(availableParentCount, ReachabilityCount());
		}
		const EventWatcher countedWatchEvents =
			[&](const Property& property, Traial& candidate, Event& event) {
				const bool stop = oracleActive
					? oracleWatchEvents(property, candidate, event)
					: watch_events(property, candidate, event);
				if (!stop)
					levelEventTransitionCount++;
				return stop;
			};
		if (stateAllocationActive) {
			TraialVec unselectedParents;
			for (size_t parent = 0ul;
			     parent < availableParentCount; ++parent) {
				const auto& specs(sstaChildSpecs[parent]);
				if (specs.empty()) {
					unselectedParents.push_back(traialsNext[parent]);
					continue;
				}
				Traial& source(traialsNext[parent].get());
				for (size_t child = 0ul; child < specs.size(); ++child) {
					Traial& traial(child == 0ul ? source : tpool.get_traial());
					if (child > 0ul) {
						if (resampleOnSplit_)
							traial.copyResampling(source);
						else
							traial = source;
					}
					assert(traial.level == lvl);
					traial.weight = specs[child].mass;
					traial.depth = 0;
					traialsNow.push_back(traial);
					if (branchFkObserve)
						branchFkChildParentLineages.push_back(
							branchFkParentLineages[parent]);
					if (branchFkObserve)
						branchFkChildParentIndices.push_back(parent);
					sstaChildGroupKeys.push_back(specs[child].signature);
					if (bats_sfe_parent_diag_enabled())
						parentDiagChildSignatures.push_back(
							parent < parentDiagSignatures.size()
							? parentDiagSignatures[parent] : "-");
				}
			}
			traialsNext.clear();
			tpool.return_traials(unselectedParents);
		} else {
			for (auto i = 0ul ; i < LVL_EFFORT ; i++) {
				const bool getFresh(i >= traialsNext.size());
				Traial& traial(getFresh ? tpool.get_traial() : traialsNext[i].get());
				if (getFresh) {  // copy *contents* from a previous-run
					if (resampleOnSplit_)
						traial.copyResampling(traialsNext[i%traialsNext.size()].get());
					else
						traial = traialsNext[i%traialsNext.size()].get();
				}
				assert(traial.level == lvl);
				if (weightedStage0Active)
					traial.weight = weightedChildMass;
				traial.depth = 0;
				traialsNow.push_back(traial);
				if (branchFkObserve)
					branchFkChildParentLineages.push_back(
						branchFkParentLineages[i % availableParentCount]);
				if (branchFkObserve)
					branchFkChildParentIndices.push_back(
						i % availableParentCount);
				if (bats_sfe_parent_diag_enabled())
					parentDiagChildSignatures.push_back(
						(i % availableParentCount) < parentDiagSignatures.size()
						? parentDiagSignatures[i % availableParentCount] : "-");
			}
			traialsNext.erase(begin(traialsNext),
							  begin(traialsNext)+std::min(traialsNext.size(),LVL_EFFORT));
			tpool.return_traials(traialsNext);
		}
		assert(traialsNow.size() == LVL_EFFORT);
		if (stateAllocationActive)
			assert(sstaChildGroupKeys.size() == LVL_EFFORT);
		assert(traialsNext.empty());
		reachCountLocal.clear();
		// ... run Fixed Effort until any level > 'l' ...
		for (auto i = 0ul ; i < LVL_EFFORT ; i++) {
			Traial& traial(traialsNow.back());
			traialsNow.pop_back();
			std::string sstaGroupKey;
			if (stateAllocationActive) {
				sstaGroupKey = sstaChildGroupKeys.back();
				 sstaChildGroupKeys.pop_back();
			}
			std::string parentDiagChildSignature;
			if (bats_sfe_parent_diag_enabled()) {
				parentDiagChildSignature = parentDiagChildSignatures.back();
				parentDiagChildSignatures.pop_back();
			}
			const size_t parentIndex = availableParentCount > 0ul
				? ((LVL_EFFORT - 1ul - i) % availableParentCount)
				: 0ul;
			std::string branchFkParentLineage;
			size_t branchFkParentIndex = parentIndex;
			if (branchFkObserve) {
				branchFkParentLineage = branchFkChildParentLineages.back();
				branchFkChildParentLineages.pop_back();
				branchFkParentIndex = branchFkChildParentIndices.back();
				branchFkChildParentIndices.pop_back();
			}
			const size_t trajectoryEventTransitionsBefore =
				levelEventTransitionCount;
			assert(traial.level < LVL_MAX+(toBuildThresholds_?0:1));
			oracleCapture(traial, lvl);
			if (levelCountDiagnostics) {
				levelSimulationStepCount++;
				model_->simulation_step(
					traial, *property_,
					countedWatchEvents);
			} else {
				model_->simulation_step(
					traial, *property_,
					oracleActive ? oracleWatchEvents : watch_events);
			}
			oracleCapture(traial, traial.level);
			const bool reachedRare = property_->is_rare(traial.state);
			const bool reachedUpper = traial.level > lvl;
			const size_t trajectoryEventCount = levelEventTransitionCount -
				trajectoryEventTransitionsBefore;
			if (branchFkObserve && (reachedUpper || reachedRare)) {
				std::ostringstream factors;
				for (const auto& conditional: pathToRare) {
					if (factors.tellp() > 0)
						factors << ";";
					factors << conditional.first << ":" << conditional.second;
				}
				if (factors.tellp() > 0)
					factors << ";";
				factors << lvl << ":pending";
				branchFkCandidates.emplace_back(
					activeFixedEffortInvocationId, diagStep,
					branchFkParentLineage,
					branchFkParentIndex, branchFkCandidates.size(),
					static_cast<unsigned>(traial.level), traial.weight,
					reachedRare, reachedUpper && traial.level > lvl + 1u,
					trajectoryEventCount, factors.str(),
					oracle_path_signature(traial, oracleStateNames));
				oracleClone(branchFkCandidateSnapshots, traial);
			}
			if (bats_sfe_parent_diag_enabled()) {
				const size_t eventCount = trajectoryEventCount;
				const double childLocalContribution =
					(reachedUpper || reachedRare) ? traial.weight : 0.0;
				const unsigned long long childIndex =
					static_cast<unsigned long long>(diagStep) *
					static_cast<unsigned long long>(LVL_EFFORT) +
					static_cast<unsigned long long>(i);
				const std::string unitSignature = parentDiagChildSignature.empty()
					? (stateAllocationActive ? sstaGroupKey : "-")
					: parentDiagChildSignature;
				const double parentWeight = parentIndex < parentDiagWeights.size()
					? parentDiagWeights[parentIndex] : traial.weight;
				std::ostringstream diag;
				diag << std::setprecision(17)
				     << "SFE_PARENT_CHILD"
				     << " fixed_effort_invocation_id="
				     << oracle_safe_text(activeFixedEffortInvocationId)
				     << " diagnostic_run_id=" << oracle_safe_text(activeParentDiagRunId)
				     << " batch_id=" << activeParentDiagBatchId
				     << " level=" << lvl
				     << " batch_step=" << diagStep
				     << " parent_index=" << parentIndex
				     << " parent_signature=" << oracle_safe_text(unitSignature)
				     << " child_id=" << oracle_safe_text(activeFixedEffortInvocationId)
				     << ":" << diagStep << ":" << i
				     << " child_index=" << childIndex
				     << " path_id=-1"
				     << " child_factor_scope=local_continuation_only"
				     << " parent_weight=" << parentWeight
				     << " child_weight=" << traial.weight
				     << " child_level=" << traial.level
				     << " success=" << ((reachedUpper || reachedRare) ? 1 : 0)
				     << " rare_only=" << ((reachedRare && !reachedUpper) ? 1 : 0)
				     << " advanced=" << (reachedUpper ? 1 : 0)
				     << " reached_level=" << traial.level
				     << " event_count=" << eventCount
				     << " event_count_sq="
				     << static_cast<double>(eventCount) * eventCount
				     << " child_local_factor_sequence=" << lvl << ":"
				     << (childLocalContribution > 0.0 ? 1 : 0)
				     << " child_path_contribution=" << childLocalContribution
				     << " child_path_contribution_square="
				     << childLocalContribution * childLocalContribution;
				bats_sfe_parent_diag_log(diag.str());
				ParentDiagAccum& accum = parentDiagAccum[unitSignature];
				++accum.childCount;
				accum.successCount += (reachedUpper || reachedRare) ? 1ul : 0ul;
				accum.advancedCount += reachedUpper ? 1ul : 0ul;
				accum.rareOnlyCount += (reachedRare && !reachedUpper) ? 1ul : 0ul;
				accum.eventCount += eventCount;
				accum.weightSum += traial.weight;
				accum.contributionSum += childLocalContribution;
				accum.contributionSqSum += childLocalContribution * childLocalContribution;
				accum.eventCountSqSum += static_cast<double>(eventCount) * eventCount;
			}
			if (weightedStage0Active)
				weightedMasses.observe(
					traial.weight, reachedUpper, reachedRare,
					static_cast<WeightedSfeLevel>(traial.level));
			if (stateAllocationActive) {
				SstaGroupBatchOutcome& outcome(
					sstaBatchOutcomes[sstaGroupKey]);
				outcome.trials++;
				outcome.eventTransitions += levelEventTransitionCount -
					trajectoryEventTransitionsBefore;
				if (reachedUpper) {
					outcome.advanced++;
					outcome.advancedMass += traial.weight;
					outcome.reachedCounts[traial.level]++;
					outcome.reachedMasses[traial.level] += traial.weight;
				}
				if (reachedUpper || reachedRare) {
					outcome.successes++;
					outcome.successMass += traial.weight;
				}
			}
			if (reachedRare)
				numRareSuccesses++;
			if (reachedRare && !reachedUpper) {
				numRareOnlySuccesses++;
				if (calibrationDiagnostics)
					parentRareOnlyCounts[parentIndex]++;
			}
			if (reachedUpper || reachedRare) {
				numSuccesses++;
				if (calibrationDiagnostics)
					parentSuccessCounts[parentIndex]++;
				if (clfOracleActive || saclfOracleActive || clfAtlasActive)
					oracleClone(clfLevelCandidates, traial);
			}
			if (reachedUpper ||
			    (("all_success" == sfePoolMode ||
			      "adaptive_clf" == sfePoolMode ||
			      "structural_clf" == sfePoolMode)
			     && reachedRare && lvl < LVL_MAX)) {
				traialsNext.push_back(traial);
				if (branchFkObserve)
					branchFkNextLineages.emplace_back(
						branchFkCandidates.back().candidate_id(),
						static_cast<unsigned>(traial.level));
				if (reachedUpper) {
					reachCount_[traial.level]++;
					reachCountLocal[traial.level]++;
					if (calibrationDiagnostics)
						parentReachCounts[parentIndex][traial.level]++;
				}
			} else {
				tpool.return_traial(traial);
			}
		}
		assert(traialsNow.empty());
		if (bats_sfe_parent_diag_enabled()) {
			for (const auto& item: parentDiagAccum) {
				const ParentDiagAccum& accum = item.second;
				std::ostringstream diag;
				 diag << std::setprecision(17)
				     << "SFE_PARENT_BATCH"
				     << " fixed_effort_invocation_id="
				     << oracle_safe_text(activeFixedEffortInvocationId)
				     << " diagnostic_run_id=" << oracle_safe_text(activeParentDiagRunId)
				     << " batch_id=" << activeParentDiagBatchId
				     << " level=" << lvl
				     << " batch_step=" << diagStep
				     << " parent_signature=" << oracle_safe_text(item.first)
				     << " child_count=" << accum.childCount
				     << " success_count=" << accum.successCount
				     << " advanced_count=" << accum.advancedCount
				     << " rare_only_count=" << accum.rareOnlyCount
				     << " event_count=" << accum.eventCount
				     << " event_count_sq_sum=" << accum.eventCountSqSum
				     << " child_weight_sum=" << accum.weightSum
				     << " contribution_sum=" << accum.contributionSum
				     << " contribution_sq_sum=" << accum.contributionSqSum;
				bats_sfe_parent_diag_log(diag.str());
			}
		}
		if (stateAllocationActive)
			assert(sstaChildGroupKeys.empty());
		if (branchFkObserve) {
			assert(branchFkChildParentLineages.empty());
			assert(branchFkChildParentIndices.empty());
			if (branchFkCandidateSnapshots.size() != branchFkCandidates.size())
				throw_FigException("branch FK candidate snapshot pool is misaligned");
			if (branchFkNextLineages.size() != traialsNext.size())
				throw_FigException("branch FK next lineage pool is misaligned");
		}
		// ... and interpret the results
		const double countSuccessFactor =
			static_cast<double>(numSuccesses) / LVL_EFFORT;
		if (weightedStage0Active &&
		    std::fabs(weightedMasses.total_mass() - 1.0) >
		    weighted_sfe_stage0_modal_tolerance())
			throw_FigException(
				"weighted SFE stage-0 child mass does not sum to one");
		const double conditionalSuccessFactor = weightedStage0Active
			? weightedMasses.success_mass() : countSuccessFactor;
		pathToRare.emplace_back(lvl, conditionalSuccessFactor);
		if (branchFkObserve) {
			std::ostringstream factors;
			for (const auto& conditional: pathToRare) {
				if (factors.tellp() > 0)
					factors << ";";
				factors << conditional.first << ":" << conditional.second;
			}
			for (auto& candidate: branchFkCandidates)
				candidate.factor_sequence = factors.str();
		}
		const size_t reachedSuccesses = sum_reach_count(reachCountLocal);
		ImportanceValue nextLvlForDiag = 0u;
		size_t modalCount = 0ul;
		WeightedModalDecision weightedModalDecision;
		if (weightedStage0Active) {
			weightedModalDecision = weightedMasses.modal(
				weighted_sfe_stage0_modal_tolerance());
			if (weightedModalDecision.valid) {
				nextLvlForDiag = static_cast<ImportanceValue>(
					weightedModalDecision.level);
				const auto countIt = reachCountLocal.find(nextLvlForDiag);
				if (countIt != reachCountLocal.end())
					modalCount = countIt->second;
			}
		} else if (!reachCountLocal.empty()) {
			auto modalIt = std::max_element(
				begin(reachCountLocal),
				end(reachCountLocal),
				[](const ReachabilityCount::value_type& lhs,
				   const ReachabilityCount::value_type& rhs)
					{ return lhs.second < rhs.second; });
			nextLvlForDiag = modalIt->first;
			modalCount = modalIt->second;
		}
		if (weightedStage0Active && !stateAllocationActive) {
			if (std::fabs(conditionalSuccessFactor - countSuccessFactor) >
			    weighted_sfe_stage0_modal_tolerance())
				throw_FigException(
					"weighted SFE stage-0 success mass differs from the legacy count factor");
			if (weightedModalDecision.valid != !reachCountLocal.empty())
				throw_FigException(
					"weighted SFE stage-0 modal availability differs from the legacy reach histogram");
			if (weightedModalDecision.valid) {
				auto countModalIt = std::max_element(
					begin(reachCountLocal), end(reachCountLocal),
					[](const ReachabilityCount::value_type& lhs,
					   const ReachabilityCount::value_type& rhs)
						{ return lhs.second < rhs.second; });
				if (nextLvlForDiag != countModalIt->first)
					throw_FigException(
						"weighted SFE stage-0 modal level differs from the legacy count modal");
				const double countModalMass =
					static_cast<double>(modalCount) * weightedChildMass;
				if (std::fabs(weightedModalDecision.modal_mass - countModalMass) >
				    weighted_sfe_stage0_modal_tolerance())
					throw_FigException(
						"weighted SFE stage-0 modal mass differs from the legacy modal count factor");
			}
		}
		const ImportanceValue nextSequentialLvl = static_cast<ImportanceValue>(lvl + 1u);
		const size_t rareCount = numRareSuccesses;
		const size_t discardedSuccessCount = numSuccesses > modalCount
		                                     ? numSuccesses - modalCount
		                                     : 0ul;
		size_t overshootCount = 0ul;
		for (const auto& item: reachCountLocal)
			if (item.first > nextSequentialLvl)
				overshootCount += item.second;
		const ImportanceValue maxReachedValue = max_reached_value(reachCountLocal);
		const auto clfDecision = bats_adaptive_clf_decision(
			numSuccesses,
			modalCount,
			overshootCount,
			numRareOnlySuccesses,
			bats_clf_risk_threshold());
		const bool adaptiveClfConfigured = "adaptive_clf" == sfePoolMode;
		const bool clfActivated = adaptiveClfConfigured && clfDecision.activated;
		std::unordered_set<std::string> structuralAllSignatures;
		std::unordered_set<std::string> structuralModalSignatures;
		std::unordered_set<std::string> structuralDiscardedSignatures;
		if ("structural_clf" == sfePoolMode || bats_sfe_pool_diag_enabled()) {
			for (const auto& candidateRef: traialsNext) {
				const Traial& candidate(candidateRef.get());
				const std::string signature(
					oracle_path_signature(candidate, oracleStateNames));
				structuralAllSignatures.insert(signature);
				if (candidate.level == nextLvlForDiag)
					structuralModalSignatures.insert(signature);
				else
					structuralDiscardedSignatures.insert(signature);
			}
		}
		const auto structuralDecision = bats_structural_clf_decision(
			structuralAllSignatures,
			structuralModalSignatures,
			structuralDiscardedSignatures);
		const bool structuralClfConfigured = "structural_clf" == sfePoolMode;
		const bool structuralClfActivated =
			structuralClfConfigured && structuralDecision.activated;
		const bool useAllSuccess = bats_use_all_success(
			"all_success" == sfePoolMode,
			adaptiveClfConfigured,
			clfDecision) || structuralClfActivated;
		const std::string effectivePoolMode =
			useAllSuccess ? "all_success" : "modal";
		double reachEntropy = 0.0;
		if (reachedSuccesses > 0ul) {
			for (const auto& item: reachCountLocal) {
				const double p = static_cast<double>(item.second) / reachedSuccesses;
				if (p > 0.0)
					reachEntropy -= p * std::log(p);
			}
		}
		const size_t candidateCount = traialsNext.size();
		const size_t retainedCount = "all_success" == effectivePoolMode
		                           ? candidateCount : modalCount;
		if (branchFkObserve) {
			const BranchFkLevelSummary branchSummary =
				branch_fk_summarise(branchFkCandidates,
					static_cast<unsigned>(nextLvlForDiag));
			if (branchSummary.candidate_count != numSuccesses)
				throw_FigException(
					"branch FK observation did not capture every successful candidate");
			if (branchSummary.weight_closure_error > 1.0e-10)
				throw_FigException("branch FK candidate weight closure failed");
			const auto samples = branch_fk_systematic_pps(
				branchFkCandidates, branchFkKeepK,
				branch_fk_diagnostic_offset(activeFixedEffortInvocationId, diagStep));
			const auto preserved = branch_fk_preserved_records(
				branchFkCandidates, samples);
			std::ostringstream summary;
			summary << std::setprecision(17)
			        << "SUMMARY fixed_effort_invocation_id="
			        << oracle_safe_text(activeFixedEffortInvocationId)
			        << " batch_step=" << diagStep
			        << " level=" << lvl
			        << " k=" << branchFkKeepK
			        << " candidate_count=" << branchSummary.candidate_count
			        << " modal_count=" << branchSummary.modal_count
			        << " rare_count=" << branchSummary.rare_count
			        << " event_count=" << branchSummary.event_count
			        << " candidate_weight=" << branchSummary.candidate_weight
			        << " modal_weight=" << branchSummary.modal_weight
			        << " discarded_weight=" << branchSummary.discarded_weight
			        << " weight_closure_error="
			        << branchSummary.weight_closure_error
			        << " sample_count=" << samples.size()
			        << " preserved_snapshot_count="
			        << preserved.size()
			        << " estimator_unchanged=1";
			bats_branch_fk_log(summary.str());
			for (const auto& candidate: branchFkCandidates) {
				std::ostringstream row;
				row << std::setprecision(17)
				    << "CANDIDATE fixed_effort_invocation_id="
				    << oracle_safe_text(activeFixedEffortInvocationId)
				    << " batch_step=" << diagStep
				    << " level=" << lvl
				    << " parent_id=" << candidate.parent_index
				    << " parent_index=" << candidate.parent_index
				    << " parent_lineage=" << oracle_safe_text(candidate.parent_id)
				    << " candidate_id=" << oracle_safe_text(candidate.candidate_id())
				    << " child_index=" << candidate.candidate_index
				    << " path_id=" << candidate.candidate_index
				    << " reached_level=" << candidate.reached_level
				    << " rare=" << (candidate.rare ? 1 : 0)
				    << " jump=" << (candidate.jump ? 1 : 0)
				    << " weight=" << candidate.weight
				    << " event_count=" << candidate.event_count
				    << " factor_sequence="
				    << oracle_safe_text(candidate.factor_sequence)
				    << " path_signature="
				    << oracle_safe_text(candidate.path_signature);
				bats_branch_fk_log(row.str());
			}
			for (size_t sampleIndex = 0ul;
			     sampleIndex < samples.size(); ++sampleIndex) {
				const auto& sample(samples[sampleIndex]);
				const BranchFkCandidate* candidate =
					sample.candidate_index < branchFkCandidates.size()
					? &branchFkCandidates[sample.candidate_index] : nullptr;
				std::ostringstream row;
				row << std::setprecision(17)
				    << "SAMPLE fixed_effort_invocation_id="
				    << oracle_safe_text(activeFixedEffortInvocationId)
				    << " batch_step=" << diagStep
				    << " level=" << lvl
				    << " parent_id=" << sample.parent_index
				    << " path_id=" << sample.candidate_index
				    << " candidate_id="
				    << oracle_safe_text(candidate ? candidate->candidate_id() : "-")
				    << " lineage_signature="
				    << oracle_safe_text(candidate
					? candidate->lineage_signature() : "-")
				    << " snapshot_preserved="
				    << (candidate && sample.candidate_index <
					branchFkCandidateSnapshots.size() ? 1 : 0)
				    << " snapshot_state_signature="
				    << oracle_safe_text(candidate && sample.candidate_index <
					branchFkCandidateSnapshots.size()
					? oracle_state_values(
						branchFkCandidateSnapshots[sample.candidate_index].get(),
						oracleStateNames) : "-")
				    << " snapshot_path_signature="
				    << oracle_safe_text(candidate && sample.candidate_index <
					branchFkCandidateSnapshots.size()
					? oracle_path_signature(
						branchFkCandidateSnapshots[sample.candidate_index].get(),
						oracleStateNames) : "-")
				    << " snapshot_clock_signature="
				    << oracle_safe_text(candidate && sample.candidate_index <
					branchFkCandidateSnapshots.size()
					? oracle_clock_values(
						branchFkCandidateSnapshots[sample.candidate_index].get()) : "-")
				    << " snapshot_lifetime="
				    << (candidate && sample.candidate_index <
					branchFkCandidateSnapshots.size()
					? branchFkCandidateSnapshots[sample.candidate_index].get().lifeTime
					: 0.0)
				    << " inclusion_probability="
				    << sample.inclusion_probability
				    << " ht_weight=" << sample.horvitz_thompson_weight;
				bats_branch_fk_log(row.str());
			}
			if (branchFkShadowPlan) {
				BranchFkLevelObservation observation;
				observation.level = static_cast<size_t>(lvl);
				observation.trials = LVL_EFFORT;
				observation.successes = numSuccesses;
				observation.event_transitions = levelEventTransitionCount;
				observation.unit_event_cost = LVL_EFFORT > 0ul
					? static_cast<double>(levelEventTransitionCount) / LVL_EFFORT
					: 0.0;
				observation.baseline_population = LVL_EFFORT;
				observation.influence_validated = false;
				observation.influence = 0.0;
				BranchFkOstreConfig config;
				config.min_population = 1ul;
				config.max_population = std::max<size_t>(1ul, LVL_EFFORT);
				config.min_successes = 1ul;
				config.budget = std::max(1.0,
					static_cast<double>(levelEventTransitionCount));
				const BranchFkPlan plan = branch_fk_ostre_plan(
					std::vector<BranchFkLevelObservation>(1ul, observation), config);
				std::ostringstream row;
				row << "SHADOW_PLAN fixed_effort_invocation_id="
				    << oracle_safe_text(activeFixedEffortInvocationId)
				    << " batch_step=" << diagStep
				    << " level=" << lvl
				    << " valid=" << (plan.valid ? 1 : 0)
				    << " reason=" << oracle_safe_text(plan.reason)
				    << " influence_source_required=validated_branch_lineage"
				    << " allocation_applied=0"
				    << " production_population=" << LVL_EFFORT;
				bats_branch_fk_log(row.str());
			}
		}
		if (branchFkObserve)
			tpool.return_traials(branchFkCandidateSnapshots);
		std::vector<size_t> parentRetainedCounts;
		if (calibrationDiagnostics) {
			parentRetainedCounts.assign(availableParentCount, 0ul);
			for (size_t parent = 0ul; parent < availableParentCount; ++parent) {
				if ("all_success" == effectivePoolMode) {
					parentRetainedCounts[parent] = parentSuccessCounts[parent];
				} else {
					const auto found =
						parentReachCounts[parent].find(nextLvlForDiag);
					parentRetainedCounts[parent] =
						found == parentReachCounts[parent].end()
						? 0ul : found->second;
				}
			}
		}
		const double levelWallTimeMs = std::chrono::duration<double,std::milli>(
			std::chrono::steady_clock::now() - levelWallStart).count();
		if (weightedStage0Active && !stateAllocationActive &&
		    bats_ssta_stage0_diag_enabled()) {
			std::ostringstream msg;
			msg << std::setprecision(17)
			    << "SSTA_STAGE0_LEVEL"
			    << " step=" << diagStep
			    << " level_index=" << lvl
			    << " trajectory_count=" << LVL_EFFORT
			    << " child_local_mass=" << weightedChildMass
			    << " total_mass=" << weightedMasses.total_mass()
			    << " mass_error=" << std::fabs(weightedMasses.total_mass() - 1.0)
			    << " advanced_mass=" << weightedMasses.advanced_mass()
			    << " rare_only_mass=" << weightedMasses.rare_only_mass()
			    << " success_mass=" << weightedMasses.success_mass()
			    << " legacy_success_factor=" << countSuccessFactor
			    << " success_factor_error="
			    << std::fabs(weightedMasses.success_mass() - countSuccessFactor)
			    << " modal_valid=" << (weightedModalDecision.valid ? 1 : 0)
			    << " modal_level=" << nextLvlForDiag
			    << " modal_count=" << modalCount
			    << " modal_keep_mass="
			    << (weightedModalDecision.valid
			        ? weightedModalDecision.modal_mass : 0.0)
			    << " modal_largest_mass="
			    << weightedModalDecision.largest_mass
			    << " modal_second_largest_mass="
			    << weightedModalDecision.second_largest_mass
			    << " modal_mass_gap=" << weightedModalDecision.mass_gap
			    << " modal_tolerance="
			    << weighted_sfe_stage0_modal_tolerance()
			    << " modal_tolerance_tie="
			    << (weightedModalDecision.tolerance_tie ? 1 : 0)
			    << " path_conditional_factor=" << conditionalSuccessFactor;
			ModelSuite::tech_log(msg.str() + "\n");
		}
		if (sstaStage1Diagnostics &&
		    (diagStep < sstaStage1DiagInitial ||
		     diagStep % sstaStage1DiagStride == 0ul)) {
			for (const auto& group: sstaGroups) {
				const auto found = sstaBatchOutcomes.find(group.signature);
				const SstaGroupBatchOutcome emptyOutcome;
				const SstaGroupBatchOutcome& outcome =
					found == sstaBatchOutcomes.end()
					? emptyOutcome : found->second;
				const auto keepCountIt =
					outcome.reachedCounts.find(nextLvlForDiag);
				const auto keepMassIt =
					outcome.reachedMasses.find(nextLvlForDiag);
				const size_t modalKeepCount =
					weightedModalDecision.valid &&
					keepCountIt != outcome.reachedCounts.end()
					? keepCountIt->second : 0ul;
				const double modalKeepMass =
					weightedModalDecision.valid &&
					keepMassIt != outcome.reachedMasses.end()
					? keepMassIt->second : 0.0;
				std::ostringstream msg;
				msg << std::setprecision(17)
				    << (fbSstaActive ? "FB_SSTA_GROUP" : "SSTA_STAGE1_GROUP")
				    << " run_index=" << sstaRunIndex
				    << " step=" << diagStep
				    << " level_index=" << lvl
				    << " pand_signature=" << oracle_safe_text(group.signature)
				    << " parent_count=" << group.parentIndices.size()
				    << " group_mass=" << group.mass
				    << " history_trials=" << (fbSstaActive
						? group.fbHistory.trials : group.history.trials)
				    << " history_advanced=" << (fbSstaActive
						? group.fbHistory.successes : group.history.advanced)
				    << " q_adv=" << group.advanceProbability
				    << " q_success=" << (fbSstaActive
						? group.fbEstimates.q : group.advanceProbability)
				    << " rho_keep=" << (fbSstaActive
						? group.fbEstimates.rho : 0.0)
				    << " cost_per_child=" << (fbSstaActive
						? group.fbEstimates.cost : 0.0)
				    << " effective_sample_size=" << (fbSstaActive
						? group.fbEstimates.effective_sample_size : 0.0)
				    << " allocation_score=" << group.score
				    << " allocated_children=" << group.allocatedChildren
				    << " fixed_children=" << [&]() {
						size_t value = 0ul;
						for (const size_t parent: group.parentIndices)
							value += fixedParentChildren[parent];
						return value;
					}()
				    << " batch_trials=" << outcome.trials
				    << " batch_advanced=" << outcome.advanced
				    << " batch_success=" << outcome.successes
				    << " batch_advanced_mass=" << outcome.advancedMass
				    << " batch_success_mass=" << outcome.successMass
				    << " batch_event_transitions=" << outcome.eventTransitions
				    << " modal_keep_count=" << modalKeepCount
				    << " modal_keep_mass=" << modalKeepMass
				    << " allocation_fallback="
				    << oracle_safe_text(sstaAllocationFallback)
				    << " total_population=" << LVL_EFFORT;
				ModelSuite::tech_log(msg.str() + "\n");
			}
		}
		if (stateAllocationActive) {
			for (const auto& item: sstaBatchOutcomes) {
				if (item.second.trials == 0ul)
					continue;
				if (fbSstaActive) {
					const auto keepIt = item.second.reachedCounts.find(nextLvlForDiag);
					const size_t retained = weightedModalDecision.valid &&
						keepIt != item.second.reachedCounts.end()
						? keepIt->second : 0ul;
					size_t allocated = item.second.trials;
					for (const auto& group: sstaGroups)
						if (group.signature == item.first) {
							allocated = group.allocatedChildren;
							break;
						}
					fbSequentialStateHistory_.observe_batch(
						static_cast<size_t>(lvl), item.first,
						FbSstaOutcomeStats(item.second.trials,
							item.second.successes, retained,
							item.second.eventTransitions), allocated);
				} else {
					sequentialStateHistory_.observe_batch(
						static_cast<size_t>(lvl), item.first,
						item.second.trials, item.second.advanced);
				}
			}
		}
		if (adaptivePopulationActive &&
		    adaptivePopulationCalibrationPhase_)
			adaptivePopulationPlan_.observe_success(
				static_cast<size_t>(lvl),
				LVL_EFFORT,
				retainedCount,
				numSuccesses,
				numRareOnlySuccesses,
				levelEventTransitionCount,
				levelWallTimeMs / 1000.0);
		if (bats_rolling_level_optimization_enabled()) {
			if (rollingObservations_.size() <= static_cast<size_t>(lvl))
				rollingObservations_.resize(static_cast<size_t>(lvl) + 1ul);
			RollingLevelObservation& rollingObservation =
				rollingObservations_[static_cast<size_t>(lvl)];
			rollingObservation.trials += LVL_EFFORT;
			rollingObservation.successes += numSuccesses;
			rollingObservation.retained += retainedCount;
			rollingObservation.event_transitions += levelEventTransitionCount;
			if (!adaptivePopulationCalibrationPhase_ &&
			    rollingPopulationPlan_.valid &&
			    rollingRemainingBudget_ > 0.0) {
				rollingRemainingBudget_ = std::max(
					0.0, rollingRemainingBudget_ -
					static_cast<double>(levelEventTransitionCount));
			}
		}
		if (vcstpActive) {
			const size_t levelIndex = static_cast<size_t>(lvl);
			if (vcstpProblem_.observations.size() <= levelIndex)
				throw_FigException("VCSTP observed level is out of range");
			VcstpObservation& observation =
				vcstpProblem_.observations[levelIndex];
			observation.trials += LVL_EFFORT;
			observation.successes += numSuccesses;
			observation.retained += retainedCount;
			observation.event_transitions += levelEventTransitionCount;
			VcstpObservation& batchObservation =
				vcstpBatchObservations_[levelIndex];
			batchObservation.trials += LVL_EFFORT;
			batchObservation.successes += numSuccesses;
			batchObservation.retained += retainedCount;
			batchObservation.event_transitions += levelEventTransitionCount;
			vcstpCurrentBatchTrajectories_ += LVL_EFFORT;
			vcstpFixedCurrentBatchTrajectories_ +=
				vcstpProblem_.fixed_populations[levelIndex];
		}
		if (cslaActive) {
			cslaHistory_.observe(static_cast<size_t>(lvl), LVL_EFFORT,
			                     numSuccesses, retainedCount,
			                     levelEventTransitionCount);
			cslaActualEventTransitions_ += levelEventTransitionCount;
		}
		if (xiwangOnlineRuntime_.enabled() && !toBuildThresholds_)
			xiwangOnlineRuntime_.observe(
				activeFixedEffortInvocationId,
				static_cast<size_t>(lvl), LVL_EFFORT,
				numSuccesses, LVL_EFFORT);
		if (bats_bsrtp_enabled() && !adaptivePopulationCalibrationPhase_) {
			const size_t levelIndex = static_cast<size_t>(lvl);
			bsrtpHistory_.observe(levelIndex, LVL_EFFORT,
			                      numSuccesses, retainedCount,
			                      numRareOnlySuccesses,
			                      levelEventTransitionCount);
			bsrtpBatchHistory_.observe(levelIndex, LVL_EFFORT,
			                           numSuccesses, retainedCount,
			                           numRareOnlySuccesses,
			                           levelEventTransitionCount);
			bsrtpActualEventTransitions_ += levelEventTransitionCount;
			bsrtpActualTrajectories_ += LVL_EFFORT;
		}
		if (calibrationDiagnostics && !toBuildThresholds_ &&
		    adaptivePopulationActive && adaptivePopulationCalibrationPhase_) {
			std::ostringstream msg;
			msg << "SFE_CALIBRATION_LEVEL"
			    << " calibration_sweep=" << diagStep
			    << " level_index=" << lvl
			    << " trials=" << LVL_EFFORT
			    << " success_count=" << numSuccesses
			    << " retained_count=" << retainedCount
			    << " modal_count=" << modalCount
			    << " rare_count=" << rareCount
			    << " rare_only_count=" << numRareOnlySuccesses
			    << " candidate_count=" << candidateCount
			    << " available_parent_count=" << availableParentCount
			    << " used_parent_count=" << usedParentCount
			    << " parent_offspring_counts=" << size_vector_to_string(offspringCounts)
			    << " parent_success_counts=" << size_vector_to_string(parentSuccessCounts)
			    << " parent_retained_counts=" << size_vector_to_string(parentRetainedCounts)
			    << " parent_rare_only_counts=" << size_vector_to_string(parentRareOnlyCounts)
			    << " parent_reached_hist=" << parent_reach_hist_to_string(parentReachCounts)
			    << " resampling_ess_abs=" << resamplingEssAbs
			    << " resampling_ess_fraction=" << resamplingEssFraction
			    << " max_offspring_share=" << maxOffspringShare
			    << " simulation_step_count=" << levelSimulationStepCount
			    << " event_transition_count=" << levelEventTransitionCount
			    << " wall_seconds=" << (levelWallTimeMs / 1000.0);
			bats_sfe_calibration_diag_log(msg.str());
		}
		countSummaryProductionTrajectories += LVL_EFFORT;
		countSummarySimulationSteps += levelSimulationStepCount;
		countSummaryEventTransitions += levelEventTransitionCount;
		countSummaryWallTimeMs += levelWallTimeMs;
		if (bats_sfe_level_agg_diag_enabled() && !toBuildThresholds_ &&
		    !adaptivePopulationCalibrationPhase_) {
			const bool hasCalibration =
				adaptivePopulationActive && adaptivePopulationPlan_.finalized();
			const size_t levelIndex = static_cast<size_t>(lvl);
			std::ostringstream msg;
			msg << "SFE_LEVEL_AGG"
			    << " level_index=" << levelIndex
			    << " calibration_trials=" << (hasCalibration ? adaptivePopulationPlan_.trials(levelIndex) : 0ul)
			    << " calibration_retained=" << (hasCalibration ? adaptivePopulationPlan_.retained(levelIndex) : 0ul)
			    << " estimated_retention_probability=" << (hasCalibration ? adaptivePopulationPlan_.probability(levelIndex) : 0.0)
			    << " planned_population=" << LVL_EFFORT
			    << " production_arrivals=" << availableParentCount
			    << " production_retained=" << retainedCount
			    << " level_extinction=" << (retainedCount == 0ul ? 1 : 0)
			    << " calibration_event_transitions=" << (hasCalibration ? adaptivePopulationPlan_.event_transitions(levelIndex) : 0ul)
			    << " production_event_transitions=" << levelEventTransitionCount
			    << " continuation_count=" << LVL_EFFORT
			    << " calibration_wall_seconds=" << (hasCalibration ? adaptivePopulationPlan_.wall_seconds(levelIndex) : 0.0)
			    << " production_wall_seconds=" << (levelWallTimeMs / 1000.0);
			ModelSuite::tech_log(msg.str() + "\n");
		}
		if (bats_sfe_pool_diag_enabled() && toBuildThresholds_) {
			std::ostringstream msg;
			msg << "SFE_THRESHOLD_LEVEL"
			    << " sweep_step=" << diagStep
			    << " level_index=" << lvl
			    << " num_trials=" << LVL_EFFORT
			    << " target_population=" << LVL_EFFORT
			    << " threshold_build_trajectory_count=" << LVL_EFFORT
			    << " threshold_build_simulation_step_count=" << levelSimulationStepCount
			    << " threshold_build_event_transition_count=" << levelEventTransitionCount
			    << " threshold_build_wall_time_ms=" << levelWallTimeMs
			    << " level_wall_time_ms=" << levelWallTimeMs
			    << " success_count=" << numSuccesses
			    << " rare_count=" << rareCount
			    << " rare_only_success_count=" << numRareOnlySuccesses
			    << " reached_success_count=" << reachedSuccesses
			    << " modal_reached_value=" << nextLvlForDiag
			    << " modal_count=" << modalCount
			    << " num_distinct_reached_values=" << reachCountLocal.size()
			    << " overshoot_count=" << overshootCount
			    << " max_reached_value=" << maxReachedValue
			    << " reached_hist=" << reach_hist_to_string(reachCountLocal);
			bats_sfe_pool_diag_log(msg.str());
		}
		if (bats_diag_enabled() && !toBuildThresholds_) {
			const auto thresholds = impFun_->thresholds();
			const auto high = (lvl + 1u < thresholds.size())
			                  ? thresholds[lvl+1u].first
			                  : impFun_->max_value(true) + 1u;
			std::ostringstream msg;
			msg << "SFE_LEVEL"
			    << " sweep_step=" << diagStep
			    << " level=" << lvl
			    << " threshold_low=" << thresholds[lvl].first
			    << " threshold_high=" << high
			    << " effort=" << LVL_EFFORT
			    << " successes=" << numSuccesses
			    << " p_hat=" << (static_cast<double>(numSuccesses)/LVL_EFFORT)
			    << " reached_successes=" << reachedSuccesses
			    << " reached_hist=" << reach_hist_to_string(reachCountLocal)
			    << " overshoot_count=" << overshootCount
			    << " modal_level=" << nextLvlForDiag
			    << " discarded_success=" << discardedSuccessCount;
			bats_diag_log(msg.str());
		}
		if (bats_sfe_pool_diag_enabled() && !toBuildThresholds_) {
			const auto thresholds = impFun_->thresholds();
			const auto cur = (lvl < thresholds.size())
			                 ? thresholds[lvl].first
			                 : impFun_->max_value(true);
			const auto next = (lvl + 1u < thresholds.size())
			                  ? thresholds[lvl+1u].first
			                  : impFun_->max_value(true) + 1u;
			const double discardedRate = numSuccesses > 0ul
			                             ? static_cast<double>(discardedSuccessCount) / numSuccesses
			                             : 0.0;
			const double overshootRate = numSuccesses > 0ul
			                             ? static_cast<double>(overshootCount) / numSuccesses
			                             : 0.0;
			const double modalKeepRate = numSuccesses > 0ul
			                             ? static_cast<double>(modalCount) / numSuccesses
			                             : 0.0;
			std::ostringstream msg;
			msg << "SFE_POOL_LEVEL"
			    << " pool_mode=" << sfePoolMode
			    << " configured_pool_mode=" << sfePoolMode
			    << " effective_pool_mode=" << effectivePoolMode
			    << " clf_activated=" << (clfActivated ? 1 : 0)
			    << " saclf_activated=" << (structuralClfActivated ? 1 : 0)
			    << " clf_risk=" << clfDecision.risk
			    << " clf_risk_threshold=" << bats_clf_risk_threshold()
			    << " discard_rate=" << clfDecision.discard_rate
			    << " sweep_step=" << diagStep
			    << " level_index=" << lvl
			    << " threshold_cur=" << cur
			    << " threshold_next=" << next
			    << " threshold_count=" << thresholds.size();
			if (diagStep == 0ul)
				msg << " threshold_chain=" << threshold_chain_to_string(thresholds);
			msg
			    << " num_trials=" << LVL_EFFORT
			    << " target_population=" << LVL_EFFORT
			    << " nominal_population=" << nominalLevelEffort
			    << " bats_level_population_effective=" << LVL_EFFORT
			    << " population_override_active=" << (populationOverrideActive ? 1 : 0)
			    << " population_source=" << populationSource
			    << " adaptive_population_active=" << (adaptivePopulationActive ? 1 : 0)
			    << " adaptive_population_phase=" << adaptivePopulationPhase
			    << " adaptive_calibration_population="
			    << bats_adaptive_calibration_population()
			    << " adaptive_calibration_sweeps="
			    << bats_adaptive_calibration_sweeps()
			    << " available_parent_count=" << availableParentCount
			    << " used_parent_count=" << usedParentCount
			    << " unused_parent_count=" << unusedParentCount
			    << " new_copy_count=" << newCopyCount
			    << " offspring_count_mean=" << offspringCountMean
			    << " offspring_count_max=" << offspringCountMax
			    << " resampling_ess_abs=" << resamplingEssAbs
			    << " resampling_ess_fraction=" << resamplingEssFraction
			    << " max_offspring_share=" << maxOffspringShare
			    << " bootstrap_level=" << (bootstrapLevel ? 1 : 0)
			    << " production_trajectory_count=" << LVL_EFFORT
			    << " simulation_step_count=" << levelSimulationStepCount
			    << " event_transition_count=" << levelEventTransitionCount
			    << " level_wall_time_ms=" << levelWallTimeMs
			    << " threshold_build_trajectory_count=0"
			    << " threshold_build_simulation_step_count=0"
			    << " threshold_build_event_transition_count=0"
			    << " threshold_build_wall_time_ms=0"
			    << " success_count=" << numSuccesses
			    << " rare_count=" << rareCount
			    << " rare_only_success_count=" << numRareOnlySuccesses
			    << " reached_success_count=" << reachedSuccesses
			    << " modal_reached_value=" << nextLvlForDiag
			    << " modal_count=" << modalCount
			    << " num_distinct_reached_values=" << reachCountLocal.size()
			    << " overshoot_count=" << overshootCount
			    << " overshoot_rate=" << overshootRate
			    << " max_reached_value=" << maxReachedValue
			    << " discarded_success_count=" << discardedSuccessCount
			    << " discarded_success_rate=" << discardedRate
			    << " modal_keep_rate=" << modalKeepRate
			    << " reach_entropy=" << reachEntropy
			    << " candidate_count=" << candidateCount
			    << " retained_count=" << retainedCount
			    << " structural_signature_count=" << structuralDecision.structural_signature_count
			    << " modal_signature_count=" << structuralDecision.modal_signature_count
			    << " discarded_signature_count=" << structuralDecision.discarded_signature_count
			    << " novel_discarded_signature_count=" << structuralDecision.novel_discarded_signature_count
			    << " structural_opportunity=" << (structuralDecision.structural_opportunity ? 1 : 0)
			    << " reached_hist=" << reach_hist_to_string(reachCountLocal);
			bats_sfe_pool_diag_log(msg.str());
		}
		if ((clfOracleActive || saclfOracleActive || clfAtlasActive) && !clfLevelCandidates.empty()) {
			std::unordered_map<ImportanceValue,size_t> clfGroupCounts;
			std::unordered_map<ImportanceValue,double> clfGroupWeights;
			double clfAllWeight = 0.0;
			double clfKeepWeight = 0.0;
			double clfDropWeight = 0.0;
			for (const auto& candidateRef: clfLevelCandidates) {
				const Traial& candidate(candidateRef.get());
				clfGroupCounts[candidate.level]++;
				clfGroupWeights[candidate.level] += candidate.weight;
				clfAllWeight += candidate.weight;
				if (candidate.level == nextLvlForDiag)
					clfKeepWeight += candidate.weight;
				else
					clfDropWeight += candidate.weight;
			}
			const auto thresholds = impFun_->thresholds();
			std::ostringstream thresholdLow, thresholdHigh;
			thresholdLow << ((lvl < thresholds.size())
			                 ? thresholds[lvl].first : impFun_->max_value(true));
			thresholdHigh << ((lvl + 1u < thresholds.size())
			                  ? thresholds[lvl+1u].first
			                  : impFun_->max_value(true) + 1u);
			if (clfOracleActive) {
				size_t candidateIndex = 0ul;
				for (const auto& candidateRef: clfLevelCandidates) {
					const Traial& candidate(candidateRef.get());
					const size_t currentCandidateIndex = candidateIndex++;
					const bool retained = candidate.level == nextLvlForDiag;
					const size_t side = retained ? 0ul : 1ul;
					const size_t sideTarget = 40ul;
					if (clfOracleSideCounts[side] >= sideTarget)
						continue;
					const double fraction = saclfOracleHorizon > 0.0
						? static_cast<double>(candidate.lifeTime) / saclfOracleHorizon
						: (LVL_MAX > 0ul
						   ? static_cast<double>(lvl) / static_cast<double>(LVL_MAX)
						   : 0.0);
					const size_t stratum = std::min<size_t>(2ul,
						static_cast<size_t>(std::floor(
							3.0 * std::max(0.0, fraction))));
					std::ostringstream signature;
					signature << oracle_path_signature(candidate, oracleStateNames)
					          << "|rv=" << candidate.level;
					std::ostringstream sampleKey;
					sampleKey << side << "|" << stratum << "|"
					          << signature.str();
					if (!clfOracleSeenSampleKeys.insert(sampleKey.str()).second)
						continue;
					ClfOracleSampleMeta meta;
					meta.poolDecision = diagStep;
					meta.candidateIndex = currentCandidateIndex;
					meta.stratum = stratum;
					meta.level = lvl;
					meta.candidateReachedValue = candidate.level;
					meta.modalReachedValue = nextLvlForDiag;
					meta.retained = retained;
					meta.rare = property_->is_rare(candidate.state);
					meta.candidateCount = clfLevelCandidates.size();
					meta.modalCount = modalCount;
					meta.groupCount = clfGroupCounts[candidate.level];
					meta.groupCountTotal = clfGroupCounts.size();
					meta.overshootCount = overshootCount;
					meta.candidateWeight = candidate.weight;
					meta.groupWeight = clfGroupWeights[candidate.level];
					meta.allWeight = clfAllWeight;
					meta.keepWeight = clfKeepWeight;
					meta.dropWeight = clfDropWeight;
					meta.thresholdLow = thresholdLow.str();
					meta.thresholdHigh = thresholdHigh.str();
					meta.configuredPoolMode = sfePoolMode;
					meta.effectivePoolMode = effectivePoolMode;
					meta.pathSignature = signature.str();
					oracleClone(clfOracleSamples, candidate);
					clfOracleMetas.push_back(meta);
					clfOracleSideCounts[side]++;
					clfOracleStratumCounts[side][stratum]++;
				}
			}
			if (saclfOracleActive) {
				std::unordered_map<std::string,size_t> signatureCounts;
				std::unordered_set<std::string> modalSignatures;
				std::unordered_set<std::string> discardedSignatures;
				std::unordered_set<std::string> novelDiscardedSignatures;
				std::unordered_map<ImportanceValue,size_t> reachedGroupCounts;
				for (const auto& candidateRef: clfLevelCandidates) {
					const Traial& candidate(candidateRef.get());
					const std::string signature(
						oracle_path_signature(candidate, oracleStateNames));
					signatureCounts[signature]++;
					reachedGroupCounts[candidate.level]++;
					if (candidate.level == nextLvlForDiag)
						modalSignatures.insert(signature);
					else
						discardedSignatures.insert(signature);
				}
				for (const auto& signature: discardedSignatures)
					if (modalSignatures.find(signature) == modalSignatures.end())
						novelDiscardedSignatures.insert(signature);
				const bool structuralOpportunity = !novelDiscardedSignatures.empty();
				const std::string saclfEffectivePoolMode =
					structuralOpportunity ? "all_success" : "modal";
				SaclfOracleCandidateMeta decisionMeta;
				decisionMeta.poolDecision = diagStep;
				decisionMeta.candidateIndex = 0ul;
				decisionMeta.stratum = 0ul;
				decisionMeta.level = lvl;
				decisionMeta.candidateReachedValue = 0u;
				decisionMeta.modalReachedValue = nextLvlForDiag;
				decisionMeta.retained = false;
				decisionMeta.rare = false;
				decisionMeta.candidateCount = clfLevelCandidates.size();
				decisionMeta.modalCount = modalCount;
				decisionMeta.reachedGroupCount = 0ul;
				decisionMeta.reachedGroupTotal = reachedGroupCounts.size();
				decisionMeta.structuralSignatureCount = signatureCounts.size();
				decisionMeta.modalSignatureCount = modalSignatures.size();
				decisionMeta.discardedSignatureCount = discardedSignatures.size();
				decisionMeta.novelDiscardedSignatureCount =
					novelDiscardedSignatures.size();
				decisionMeta.structuralOpportunity = structuralOpportunity;
				decisionMeta.saclfActivated = structuralOpportunity;
				decisionMeta.overshootCount = overshootCount;
				decisionMeta.candidateWeight = 0.0;
				decisionMeta.candidateQuality = 0.0;
				decisionMeta.inclusionProbability = 1.0;
				decisionMeta.frameSize = clfLevelCandidates.size();
				decisionMeta.stratumSize = 0ul;
				decisionMeta.sampleSize = 0ul;
				decisionMeta.thresholdLow = thresholdLow.str();
				decisionMeta.thresholdHigh = thresholdHigh.str();
				decisionMeta.configuredPoolMode = "structural_clf";
				decisionMeta.effectivePoolMode = saclfEffectivePoolMode;
				decisionMeta.reachedHist = reach_hist_to_string(reachCountLocal);
				decisionMeta.structuralSignature = "";
				saclfOracleDecisionMetas.push_back(decisionMeta);

				size_t candidateIndex = 0ul;
				for (const auto& candidateRef: clfLevelCandidates) {
					const Traial& candidate(candidateRef.get());
					const size_t currentCandidateIndex = candidateIndex++;
					const bool retained = candidate.level == nextLvlForDiag;
					const double fraction = clfOracleHorizon > 0.0
						? static_cast<double>(candidate.lifeTime) / clfOracleHorizon
						: (LVL_MAX > 0ul
						   ? static_cast<double>(lvl) / static_cast<double>(LVL_MAX)
						   : 0.0);
					const size_t stratum = std::min<size_t>(2ul,
						static_cast<size_t>(std::floor(
							3.0 * std::max(0.0, fraction))));
					const std::string signature(
						oracle_path_signature(candidate, oracleStateNames));
					SaclfOracleCandidateMeta meta;
					meta.poolDecision = diagStep;
					meta.candidateIndex = currentCandidateIndex;
					meta.stratum = stratum;
					meta.level = lvl;
					meta.candidateReachedValue = candidate.level;
					meta.modalReachedValue = nextLvlForDiag;
					meta.retained = retained;
					meta.rare = property_->is_rare(candidate.state);
					meta.candidateCount = clfLevelCandidates.size();
					meta.modalCount = modalCount;
					meta.reachedGroupCount = reachedGroupCounts[candidate.level];
					meta.reachedGroupTotal = reachedGroupCounts.size();
					meta.structuralSignatureCount = signatureCounts.size();
					meta.modalSignatureCount = modalSignatures.size();
					meta.discardedSignatureCount = discardedSignatures.size();
					meta.novelDiscardedSignatureCount =
						novelDiscardedSignatures.size();
					meta.structuralOpportunity = structuralOpportunity;
					meta.saclfActivated = structuralOpportunity;
					meta.overshootCount = overshootCount;
					meta.candidateWeight = candidate.weight;
					meta.candidateQuality = 1.0;
					meta.inclusionProbability = 1.0;
					meta.frameSize = clfLevelCandidates.size();
					meta.stratumSize = signatureCounts[signature];
					meta.sampleSize = signatureCounts[signature];
					meta.thresholdLow = thresholdLow.str();
					meta.thresholdHigh = thresholdHigh.str();
					meta.configuredPoolMode = "structural_clf";
					meta.effectivePoolMode = saclfEffectivePoolMode;
					meta.reachedHist = reach_hist_to_string(reachCountLocal);
					meta.structuralSignature = signature;
					oracleClone(saclfOracleSamples, candidate);
					saclfOracleMetas.push_back(meta);
				}
			}
			if (clfAtlasActive) {
				std::unordered_map<std::string,size_t> signatureCounts;
				std::unordered_map<std::string,size_t> signatureModalCounts;
				std::unordered_map<std::string,size_t> signatureDiscardedCounts;
				std::unordered_map<std::string,size_t> signatureMaxOvershoot;
				std::unordered_map<std::string,double> signatureWeights;
				std::unordered_set<std::string> modalSignatures;
				std::unordered_set<std::string> discardedSignatures;
				std::unordered_set<std::string> novelDiscardedSignatures;
				std::unordered_map<ImportanceValue,size_t> reachedGroupCounts;
				for (const auto& candidateRef: clfLevelCandidates) {
					const Traial& candidate(candidateRef.get());
					const std::string signature(
						oracle_path_signature(candidate, oracleStateNames));
					const bool retained = candidate.level == nextLvlForDiag;
					const size_t overshootLayer =
						candidate.level > nextSequentialLvl
						? static_cast<size_t>(candidate.level - nextSequentialLvl)
						: 0ul;
					signatureCounts[signature]++;
					signatureWeights[signature] += candidate.weight;
					signatureMaxOvershoot[signature] =
						std::max(signatureMaxOvershoot[signature], overshootLayer);
					reachedGroupCounts[candidate.level]++;
					if (retained) {
						signatureModalCounts[signature]++;
						modalSignatures.insert(signature);
					} else {
						signatureDiscardedCounts[signature]++;
						discardedSignatures.insert(signature);
					}
				}
				for (const auto& signature: discardedSignatures)
					if (modalSignatures.find(signature) == modalSignatures.end())
						novelDiscardedSignatures.insert(signature);

				for (const auto& item: signatureCounts) {
					ClfAtlasPathMeta pathMeta;
					pathMeta.poolDecision = diagStep;
					pathMeta.level = lvl;
					pathMeta.pathSignature = item.first;
					pathMeta.ancestorGate = atlas_ancestor_gate(item.first);
					pathMeta.count = item.second;
					pathMeta.modalCount = signatureModalCounts[item.first];
					pathMeta.discardedCount = signatureDiscardedCounts[item.first];
					pathMeta.maxOvershootLayer = signatureMaxOvershoot[item.first];
					pathMeta.totalWeight = signatureWeights[item.first];
					clfAtlasPathMetas.push_back(pathMeta);
				}

				size_t candidateIndex = 0ul;
				for (const auto& candidateRef: clfLevelCandidates) {
					const Traial& candidate(candidateRef.get());
					const size_t currentCandidateIndex = candidateIndex++;
					const bool retained = candidate.level == nextLvlForDiag;
					const double fraction = clfAtlasHorizon > 0.0
						? static_cast<double>(candidate.lifeTime) / clfAtlasHorizon
						: (LVL_MAX > 0ul
						   ? static_cast<double>(lvl) / static_cast<double>(LVL_MAX)
						   : 0.0);
					const size_t stratum = std::min<size_t>(2ul,
						static_cast<size_t>(std::floor(
							3.0 * std::max(0.0, fraction))));
					const std::string signature(
						oracle_path_signature(candidate, oracleStateNames));
					const size_t overshootLayer =
						candidate.level > nextSequentialLvl
						? static_cast<size_t>(candidate.level - nextSequentialLvl)
						: 0ul;
					ClfAtlasCandidateMeta meta;
					meta.poolDecision = diagStep;
					meta.candidateIndex = currentCandidateIndex;
					meta.side = retained ? 0ul : 1ul;
					meta.stratum = stratum;
					meta.level = lvl;
					meta.candidateReachedValue = candidate.level;
					meta.modalReachedValue = nextLvlForDiag;
					meta.retained = retained;
					meta.rare = property_->is_rare(candidate.state);
					meta.candidateCount = clfLevelCandidates.size();
					meta.modalCount = modalCount;
					meta.reachedGroupCount = reachedGroupCounts[candidate.level];
					meta.reachedGroupTotal = reachedGroupCounts.size();
					meta.pathSignatureCount = signatureCounts.size();
					meta.modalSignatureCount = modalSignatures.size();
					meta.discardedSignatureCount = discardedSignatures.size();
					meta.novelDiscardedSignatureCount =
						novelDiscardedSignatures.size();
					meta.overshootCount = overshootCount;
					meta.overshootLayer = overshootLayer;
					meta.frameSize = clfLevelCandidates.size();
					meta.stratumSize = 0ul;
					meta.sampleSize = 0ul;
					meta.inclusionProbability = 0.0;
					meta.candidateWeight = candidate.weight;
					meta.allWeight = clfAllWeight;
					meta.modalWeight = clfKeepWeight;
					meta.discardedWeight = clfDropWeight;
					meta.thresholdLow = thresholdLow.str();
					meta.thresholdHigh = thresholdHigh.str();
					meta.configuredPoolMode = "modal";
					meta.effectivePoolMode = "modal";
					meta.reachedHist = reach_hist_to_string(reachCountLocal);
					meta.pathSignature = signature;
					meta.ancestorGate = atlas_ancestor_gate(signature);
					oracleClone(clfAtlasCandidates, candidate);
					clfAtlasMetas.push_back(meta);
				}
			}
			tpool.return_traials(clfLevelCandidates);
		}
		diagStep++;
		if (!traialsNext.empty()) {
			auto nextLvl = static_cast<ImportanceValue>(0u);
			if ("all_success" == effectivePoolMode) {
				nextLvl = nextSequentialLvl;
				if (LVL_MAX < nextLvl && LVL_MAX != arbitraryMaxLevel)
					nextLvl = static_cast<ImportanceValue>(LVL_MAX);
				if (LVL_MAX < nextLvl && LVL_MAX == arbitraryMaxLevel)
					nextLvl = LVL_MAX;  // patch for badly chosen arbitraryMaxLevel
				assert(nextLvl <= LVL_MAX);
				normalise_success_pool_to_next_level(traialsNext, nextLvl);
				if (branchFkObserve) {
					for (auto& lineage: branchFkNextLineages)
						lineage.reached_level = static_cast<unsigned>(nextLvl);
					branchFkParentLineages.clear();
					for (const auto& lineage: branchFkNextLineages)
						branchFkParentLineages.push_back(lineage.candidate_id);
				}
			} else if (weightedStage0Active) {
				if (!weightedModalDecision.valid ||
				    weightedModalDecision.modal_mass <= 0.0)
					throw_FigException(
						"weighted SFE stage-0 cannot normalise an empty modal pool");
				nextLvl = nextLvlForDiag;
				filter_reached_value(traialsNext, nextLvl);
				if (branchFkObserve) {
					const auto retained = branch_fk_filter_modal_lineage(
						branchFkNextLineages, static_cast<unsigned>(nextLvl));
					branchFkParentLineages.clear();
					for (const auto& lineage: retained)
						branchFkParentLineages.push_back(lineage.candidate_id);
				}
				double normalisedMass = 0.0;
				for (auto& traialRef: traialsNext) {
					Traial& traial(traialRef.get());
					traial.weight = normalised_local_mass(
						traial.weight, weightedModalDecision.modal_mass);
					normalisedMass += traial.weight;
				}
				if (std::fabs(normalisedMass - 1.0) >
				    weighted_sfe_stage0_modal_tolerance())
					throw_FigException(
						"weighted SFE stage-0 normalised modal parent mass does not sum to one");
			} else {
				nextLvl = filter_next_value(traialsNext, reachCountLocal);
				if (branchFkObserve) {
					const auto retained = branch_fk_filter_modal_lineage(
						branchFkNextLineages, static_cast<unsigned>(nextLvl));
					branchFkParentLineages.clear();
					for (const auto& lineage: retained)
						branchFkParentLineages.push_back(lineage.candidate_id);
				}
			}
			assert(lvl < nextLvl);
			if (LVL_MAX < nextLvl && LVL_MAX == arbitraryMaxLevel)
				nextLvl = LVL_MAX;  // patch for badly chosen arbitraryMaxLevel
			assert(nextLvl <= LVL_MAX);
			lvl = nextLvl;
			if (toBuildThresholds_ && lvl == LVL_MAX)
				pathToRare.emplace_back(lvl,1.0);
		}
		else if (branchFkObserve) {
			branchFkParentLineages.clear();
		}
	} while (!traialsNext.empty() &&
			 lvl < LVL_MAX + (toBuildThresholds_ ? 0 : 1 ));

	tpool.return_traials(traialsNow);
	tpool.return_traials(traialsNext);

	if (bats_sfe_count_only_enabled()) {
		const char* adaptivePopulationSummaryPhase =
			toBuildThresholds_ || !bats_adaptive_level_population_enabled()
			? "disabled"
			: (adaptivePopulationCalibrationPhase_ ? "calibration" : "production");
		std::ostringstream msg;
		msg << "SFE_COUNT_SUMMARY"
		    << " mode=" << (toBuildThresholds_ ? "threshold_build" : "production")
		    << " adaptive_population_phase=" << adaptivePopulationSummaryPhase
		    << " level_count=" << diagStep
		    << " production_trajectory_count="
		    << (toBuildThresholds_ ? 0ul : countSummaryProductionTrajectories)
		    << " simulation_step_count="
		    << (toBuildThresholds_ ? 0ul : countSummarySimulationSteps)
		    << " event_transition_count="
		    << (toBuildThresholds_ ? 0ul : countSummaryEventTransitions)
		    << " threshold_build_trajectory_count="
		    << (toBuildThresholds_ ? countSummaryProductionTrajectories : 0ul)
		    << " threshold_build_simulation_step_count="
		    << (toBuildThresholds_ ? countSummarySimulationSteps : 0ul)
		    << " threshold_build_event_transition_count="
		    << (toBuildThresholds_ ? countSummaryEventTransitions : 0ul)
		    << " threshold_build_wall_time_ms="
		    << (toBuildThresholds_ ? countSummaryWallTimeMs : 0.0)
		    << " level_wall_time_ms="
		    << (toBuildThresholds_ ? 0.0 : countSummaryWallTimeMs);
		ModelSuite::tech_log(msg.str() + "\n");
	}

	if (oracleActive) {
		for (size_t stratum = 0ul; stratum < oracleSelected.size(); ++stratum) {
			while (oracleSelected[stratum].size() < 10ul &&
			       !oracleFallback[stratum].empty()) {
				oracleSelected[stratum].push_back(oracleFallback[stratum].back());
				oracleFallback[stratum].pop_back();
			}
			if (oracleSelected[stratum].size() != 10ul) {
				std::ostringstream message;
				message << "N4Q oracle could not fill 10 states in stratum "
				        << stratum << " (captured="
				        << oracleSelected[stratum].size() << ")";
				throw_FigException(message.str());
			}
		}

		const OracleSeedPlan seedPlan(oracle_load_seed_plan(oracleSeedFile));
		const auto oracleThresholds(impFun_->thresholds());
		std::ostringstream oracleThresholdValues;
		for (size_t i = 0ul; i < oracleThresholds.size(); ++i) {
			if (i > 0ul)
				oracleThresholdValues << "|";
			oracleThresholdValues << oracleThresholds[i].first;
		}
		const ImportanceValue oracleImportanceMax = impFun_->max_value(true);
		const size_t oracleEvalRounds = std::max<size_t>(
			1ul, oracle_env_size("N4Q_ORACLE_EVAL_ROUNDS", 200ul));
		volatile ImportanceValue oracleEvalSink = 0u;
		size_t oracleEvalCalls = 0ul;
		const auto oracleEvalStart = std::chrono::steady_clock::now();
		for (size_t round = 0ul; round < oracleEvalRounds; ++round) {
			for (const auto& snapshots: oracleSelected) {
				for (const auto& snapshotRef: snapshots) {
					const Traial& snapshot(snapshotRef.get());
					oracleEvalSink ^= impFun_->importance_of(
						snapshot.state, snapshot.clocks_values());
					oracleEvalCalls++;
				}
			}
		}
		const auto oracleEvalElapsed = std::chrono::duration<double,std::nano>(
			std::chrono::steady_clock::now() - oracleEvalStart).count();
		const double oracleEvalNsPerCall = oracleEvalCalls > 0ul
			? oracleEvalElapsed / static_cast<double>(oracleEvalCalls) : 0.0;
		(void)oracleEvalSink;

		const std::string stem = oracleOutputDir + "/" +
			oracle_file_token(oracleModel) + "_" + oracle_file_token(oracleSource);
		std::ofstream rawStates((stem + "_raw_states.csv").c_str(), std::ios::trunc);
		std::ofstream continuations((stem + "_continuations.csv").c_str(), std::ios::trunc);
		if (!rawStates || !continuations)
			throw_FigException("cannot open N4Q oracle output files under " + oracleOutputDir);
		rawStates << "model_id,source,state_id,stratum,level,weight,life_time,"
		             "source_importance,source_importance_max,source_threshold_values,"
		             "oracle_level_effort,"
		             "stratum_basis,"
		             "source_eval_ns_per_call,source_eval_call_count,"
		             "state_values,clock_values,path_signature,hits,trials,h_hat,"
		             "wilson_low,wilson_high,wilson_half_width,failure_count,"
		             "same_seed_reproducible,different_seed_distinct,"
		             "resampled_active_clock_count,"
		             "clone_unchanged,initial_clock_lifetime_preserved\n";
		continuations << "model_id,source,state_id,rep,probe,continuation_seed,"
		                 "hit,status,life_before,life_after,initial_preserved,"
		                 "resampled_clock_count,final_signature\n";

		const size_t minTrials = std::max<size_t>(200ul,
			oracle_env_size("N4Q_ORACLE_MIN_TRIALS", 200ul));
		const size_t maxTrials = std::max<size_t>(minTrials,
			oracle_env_size("N4Q_ORACLE_MAX_TRIALS", 2000ul));
		const double halfWidthStop = oracle_env_double(
			"N4Q_ORACLE_WILSON_HALF_WIDTH", 0.03);
		const unsigned long originalSeed = Clock::rng_seed();
		if (0ul == originalSeed)
			throw_FigException("N4Q oracle requires a deterministic non-zero base RNG seed");
		const auto modelClocks(model_->clocks());
		std::unordered_map<std::string,size_t> oracleStatePositions;
		for (size_t i = 0ul; i < oracleStateNames.size(); ++i)
			oracleStatePositions.emplace(oracleStateNames[i], i);
		auto oracleHasSuffix = [](const std::string& value,
		                          const std::string& suffix) {
			return value.size() >= suffix.size() &&
			       0 == value.compare(value.size()-suffix.size(),
			                          suffix.size(), suffix);
		};
		auto oracleResampleActiveClocks = [&](Traial& clone) {
			if (property_->is_rare(clone.state) || property_->is_stop(clone.state))
				return static_cast<size_t>(0ul);
			auto timeouts(TraialPool::get_timeouts(clone));
			if (timeouts.size() != modelClocks.size())
				throw_FigException("N4Q oracle clock vector size mismatch");
			size_t resampled = 0ul;
			for (size_t i = 0ul; i < timeouts.size(); ++i) {
				auto& timeout(timeouts[i]);
				const bool failureClock = oracleHasSuffix(timeout.name, "_fc");
				const bool repairClock = oracleHasSuffix(timeout.name, "_rc");
				if (!failureClock && !repairClock)
					continue;
				const std::string base(timeout.name.substr(0, timeout.name.size()-3ul));
				const auto position(oracleStatePositions.find(base + "_broken"));
				if (position == oracleStatePositions.end())
					continue;
				const auto broken(clone.state[position->second]);
				const bool active = failureClock ? 0 == broken : 2 == broken;
				if (!active || timeout.sampled <= 0.0f || timeout.value <= 0.0f)
					continue;
				const Clock& clock(modelClocks[i].get());
				clock.resample(timeout.sampled, timeout.value);
				resampled++;
			}
			TraialPool::set_timeouts(clone, std::move(timeouts));
			return resampled;
		};
		EventWatcher continuationWatcher = [this](
			const Property& property, Traial& traial, Event&) {
				return property.is_rare(traial.state) ||
				       property.is_stop(traial.state);
			};

		auto runContinuation = [&](const Traial& snapshot,
		                           unsigned long continuationSeed) {
			OracleContinuationOutcome outcome;
			for (const auto& clockRef: modelClocks)
				clockRef.get().reset_distribution_rng_state();
			Clock::reseed_for_diagnostics(continuationSeed);
			Traial& clone(tpool.get_traial());
			clone = snapshot;
			outcome.initialPreserved =
				clone.state == snapshot.state &&
				clone.lifeTime == snapshot.lifeTime &&
				clone.level == snapshot.level &&
				clone.weight == snapshot.weight &&
				oracle_same_timeouts(clone, snapshot);
			outcome.resampledClockCount = oracleResampleActiveClocks(clone);
			try {
				model_->simulation_step(clone, *property_, continuationWatcher);
				outcome.ok = true;
				outcome.hit = property_->is_rare(clone.state);
				outcome.status = "ok";
			} catch (const FigException& error) {
				outcome.status = oracle_safe_text(error.msg());
			} catch (const std::exception& error) {
				outcome.status = oracle_safe_text(error.what());
			} catch (...) {
				outcome.status = "unknown_exception";
			}
			outcome.lifeAfter = clone.lifeTime;
			outcome.finalSignature = oracle_full_signature(clone, oracleStateNames);
			tpool.return_traial(std::move(clone));
			return outcome;
		};

		auto writeContinuation = [&](const std::string& stateId,
		                             size_t rep,
		                             const std::string& probe,
		                             unsigned long continuationSeed,
		                             const Traial& snapshot,
		                             const OracleContinuationOutcome& outcome) {
			continuations << oracle_safe_text(oracleModel) << ","
			              << oracle_safe_text(oracleSource) << ","
			              << stateId << "," << rep << "," << probe << ","
			              << continuationSeed << "," << (outcome.hit ? 1 : 0) << ","
			              << oracle_safe_text(outcome.status) << ","
			              << std::setprecision(12) << snapshot.lifeTime << ","
			              << outcome.lifeAfter << ","
			              << (outcome.initialPreserved ? 1 : 0) << ","
			              << outcome.resampledClockCount << ","
			              << oracle_safe_text(outcome.finalSignature) << "\n";
		};

		size_t stateIndex = 0ul;
		const char* stratumNames[] = {"early", "middle", "late"};
		for (size_t stratum = 0ul; stratum < oracleSelected.size(); ++stratum) {
			for (const auto& snapshotRef: oracleSelected[stratum]) {
				const Traial& snapshot(snapshotRef.get());
				std::ostringstream stateIdBuilder;
				stateIdBuilder << oracleSource << "_" << std::setw(3)
				               << std::setfill('0') << stateIndex++;
				const std::string stateId(stateIdBuilder.str());
				auto planIt = seedPlan.find(stateId);
				if (planIt == seedPlan.end() || planIt->second.size() < minTrials)
					throw_FigException("incomplete N4Q oracle seed plan for " + stateId);
				const auto& seeds(planIt->second);
				const size_t attemptLimit = std::min(maxTrials, seeds.size());
				if (0ul == seeds[0])
					throw_FigException("zero continuation seed for " + stateId);

				const std::string snapshotBefore(
					oracle_full_signature(snapshot, oracleStateNames));
				const auto probeA(runContinuation(snapshot, seeds[0]));
				const auto probeB(runContinuation(snapshot, seeds[0]));
				writeContinuation(stateId, 0ul, "same_seed_a", seeds[0], snapshot, probeA);
				writeContinuation(stateId, 0ul, "same_seed_b", seeds[0], snapshot, probeB);
				const bool sameSeedReproducible = probeA.ok && probeB.ok &&
					probeA.hit == probeB.hit &&
					probeA.finalSignature == probeB.finalSignature;

				size_t hits = 0ul;
				size_t trials = 0ul;
				size_t failures = 0ul;
				bool allInitialPreserved = probeA.initialPreserved && probeB.initialPreserved;
				std::string firstFinal, secondFinal;
				std::pair<double,double> interval(0.0, 1.0);
				for (size_t attempt = 0ul; attempt < attemptLimit; ++attempt) {
					if (0ul == seeds[attempt]) {
						failures++;
						continue;
					}
					const auto outcome(runContinuation(snapshot, seeds[attempt]));
					writeContinuation(
						stateId, attempt+1ul, "0", seeds[attempt], snapshot, outcome);
					allInitialPreserved = allInitialPreserved && outcome.initialPreserved;
					if (!outcome.ok) {
						failures++;
						continue;
					}
					trials++;
					if (outcome.hit)
						hits++;
					if (trials == 1ul)
						firstFinal = outcome.finalSignature;
					else if (trials == 2ul)
						secondFinal = outcome.finalSignature;
					interval = oracle_wilson_interval(hits, trials);
					if (trials >= minTrials && 0ul == failures &&
					    (interval.second-interval.first)/2.0 <= halfWidthStop)
						break;
				}
				interval = oracle_wilson_interval(hits, trials);
				const double hHat = trials > 0ul
					? static_cast<double>(hits)/static_cast<double>(trials) : 0.0;
				const bool differentSeedDistinct = !firstFinal.empty() &&
					!secondFinal.empty() && firstFinal != secondFinal;
				const bool cloneUnchanged = snapshotBefore ==
					oracle_full_signature(snapshot, oracleStateNames);
				const ImportanceValue sourceImportance = impFun_->importance_of(
					snapshot.state,
					snapshot.clocks_values());
				rawStates << oracle_safe_text(oracleModel) << ","
				          << oracle_safe_text(oracleSource) << "," << stateId << ","
				          << stratumNames[stratum] << "," << snapshot.level << ","
				          << std::setprecision(12) << snapshot.weight << ","
				          << snapshot.lifeTime << ","
				          << sourceImportance << "," << oracleImportanceMax << ","
				          << oracle_safe_text(oracleThresholdValues.str()) << ","
				          << oracleLevelEffort << ","
				          << (oracleHorizon > 0.0 ? "life_time" : "importance")
				          << ","
				          << oracleEvalNsPerCall << "," << oracleEvalCalls << ","
				          << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
				          << oracle_safe_text(oracle_clock_values(snapshot)) << ","
				          << oracle_safe_text(oracle_path_signature(snapshot, oracleStateNames)) << ","
				          << hits << "," << trials << "," << hHat << ","
				          << interval.first << "," << interval.second << ","
				          << (interval.second-interval.first)/2.0 << ","
				          << failures << "," << (sameSeedReproducible ? 1 : 0) << ","
				          << (differentSeedDistinct ? 1 : 0) << ","
				          << probeA.resampledClockCount << ","
				          << (cloneUnchanged ? 1 : 0) << ","
				          << (allInitialPreserved ? 1 : 0) << "\n";
			}
		}

		rawStates.flush();
		continuations.flush();
		if (!rawStates || !continuations)
			throw_FigException("failed flushing N4Q oracle output files");
		std::_Exit(EXIT_SUCCESS);
		Clock::reseed_for_diagnostics(originalSeed);
		for (const auto& clockRef: modelClocks)
			clockRef.get().reset_distribution_rng_state();
		for (auto& snapshots: oracleSelected)
			tpool.return_traials(snapshots);
		for (auto& snapshots: oracleFallback)
			tpool.return_traials(snapshots);
		interrupted = true;
	}

	if (clfAtlasActive) {
		const OracleSeedPlan seedPlan(oracle_load_seed_plan(clfAtlasSeedFile));
		const auto oracleThresholds(impFun_->thresholds());
		std::ostringstream oracleThresholdValues;
		for (size_t i = 0ul; i < oracleThresholds.size(); ++i) {
			if (i > 0ul)
				oracleThresholdValues << "|";
			oracleThresholdValues << oracleThresholds[i].first;
		}
		const ImportanceValue oracleImportanceMax = impFun_->max_value(true);
		const std::string stem = clfAtlasOutputDir + "/" +
			oracle_file_token(clfAtlasModel) + "_" +
			oracle_file_token(clfAtlasSource) + "_" +
			oracle_file_token(clfAtlasSourceSeed);
		std::ofstream candidateFrame((stem + "_candidate_frame.csv").c_str(), std::ios::trunc);
		std::ofstream pathSummary((stem + "_path_summary.csv").c_str(), std::ios::trunc);
		std::ofstream sampledStates((stem + "_sampled_states.csv").c_str(), std::ios::trunc);
		std::ofstream continuations((stem + "_continuations.csv").c_str(), std::ios::trunc);
		if (!candidateFrame || !pathSummary || !sampledStates || !continuations)
			throw_FigException("cannot open CLF Atlas output files under " + clfAtlasOutputDir);
		candidateFrame <<
			"model_id,source,source_seed,state_id,pool_decision,candidate_index,"
			"side,stratum,level,threshold_low,threshold_high,"
			"candidate_reached_value,modal_reached_value,retained,discarded,rare,"
			"path_signature,ancestor_gate,overshoot_layer,frame_size,stratum_size,"
			"sample_size,inclusion_probability,candidate_count,modal_count,"
			"reached_group_count,reached_group_total,path_signature_count,"
			"modal_signature_count,discarded_signature_count,"
			"novel_discarded_signature_count,overshoot_count,candidate_weight,"
			"all_weight,modal_weight,discarded_weight,configured_pool_mode,"
			"effective_pool_mode,reached_hist,state_values,clock_values,life_time,"
			"sampled\n";
		pathSummary <<
			"model_id,source,source_seed,pool_decision,level,path_signature,"
			"ancestor_gate,count,modal_count,discarded_count,max_overshoot_layer,"
			"total_weight\n";
		sampledStates <<
			"model_id,source,source_seed,state_id,pool_decision,candidate_index,"
			"side,stratum,level,weight,life_time,threshold_low,threshold_high,"
			"candidate_reached_value,modal_reached_value,retained,discarded,rare,"
			"path_signature,ancestor_gate,overshoot_layer,frame_size,stratum_size,"
			"sample_size,inclusion_probability,candidate_weight,all_weight,"
			"modal_weight,discarded_weight,source_importance,source_importance_max,"
			"source_threshold_values,state_values,clock_values,hits,trials,h_hat,"
			"wilson_low,wilson_high,wilson_half_width,failure_count,"
			"same_seed_reproducible,different_seed_distinct,"
			"resampled_active_clock_count,clone_unchanged,"
			"initial_clock_lifetime_preserved\n";
		continuations <<
			"model_id,source,source_seed,state_id,rep,probe,continuation_seed,"
			"hit,status,life_before,life_after,initial_preserved,"
			"resampled_clock_count,final_signature\n";

		const char* stratumNames[] = {"early", "middle", "late"};
		const char* sideNames[] = {"modal", "discarded"};
		std::vector<bool> selected(clfAtlasMetas.size(), false);
		std::vector<std::string> stateIds(clfAtlasMetas.size(), "");
		std::array<std::array<std::vector<size_t>,3>,2> buckets;
		for (size_t i = 0ul; i < clfAtlasMetas.size(); ++i) {
			const auto& meta(clfAtlasMetas[i]);
			if (meta.side < 2ul && meta.stratum < 3ul)
				buckets[meta.side][meta.stratum].push_back(i);
		}
		auto chooseFromBucket = [&](std::vector<size_t>& bucket,
		                            size_t limit,
		                            std::unordered_set<std::string>& seen) {
			size_t picked = 0ul;
			std::sort(bucket.begin(), bucket.end(),
				[&](size_t lhs, size_t rhs) {
					const auto& a(clfAtlasMetas[lhs]);
					const auto& b(clfAtlasMetas[rhs]);
					if (a.pathSignature != b.pathSignature)
						return a.pathSignature < b.pathSignature;
					if (a.poolDecision != b.poolDecision)
						return a.poolDecision < b.poolDecision;
					return a.candidateIndex < b.candidateIndex;
				});
			for (const auto idx: bucket) {
				if (picked >= limit)
					break;
				if (selected[idx])
					continue;
				if (!seen.insert(clfAtlasMetas[idx].pathSignature).second)
					continue;
				selected[idx] = true;
				picked++;
			}
			for (const auto idx: bucket) {
				if (picked >= limit)
					break;
				if (selected[idx])
					continue;
				selected[idx] = true;
				picked++;
			}
			return picked;
		};
		std::array<std::array<size_t,3>,2> sampleSizes =
			{{{{0ul, 0ul, 0ul}}, {{0ul, 0ul, 0ul}}}};
		for (size_t side = 0ul; side < 2ul; ++side) {
			size_t pickedSide = 0ul;
			std::unordered_set<std::string> seenSignatures;
			const size_t quotas[3] = {4ul, 3ul, 3ul};
			for (size_t stratum = 0ul; stratum < 3ul; ++stratum) {
				const size_t quota = std::min<size_t>(
					quotas[stratum], buckets[side][stratum].size());
				const size_t picked = chooseFromBucket(
					buckets[side][stratum], quota, seenSignatures);
				sampleSizes[side][stratum] += picked;
				pickedSide += picked;
			}
			for (size_t stratum = 0ul; pickedSide < 10ul && stratum < 3ul; ++stratum) {
				const size_t picked = chooseFromBucket(
					buckets[side][stratum], 10ul - pickedSide, seenSignatures);
				sampleSizes[side][stratum] += picked;
				pickedSide += picked;
			}
		}
		size_t sampledOrdinal = 0ul;
		for (size_t i = 0ul; i < selected.size(); ++i) {
			if (!selected[i])
				continue;
			std::ostringstream stateIdBuilder;
			stateIdBuilder << clfAtlasSource << "_" << clfAtlasSourceSeed
			               << "_" << std::setw(5) << std::setfill('0')
			               << sampledOrdinal++;
			stateIds[i] = stateIdBuilder.str();
		}
		for (auto& meta: clfAtlasMetas) {
			const size_t stratumSize =
				(meta.side < 2ul && meta.stratum < 3ul)
				? buckets[meta.side][meta.stratum].size() : 0ul;
			const size_t sampleSize =
				(meta.side < 2ul && meta.stratum < 3ul)
				? sampleSizes[meta.side][meta.stratum] : 0ul;
			meta.stratumSize = stratumSize;
			meta.sampleSize = sampleSize;
			meta.inclusionProbability = stratumSize > 0ul
				? static_cast<double>(sampleSize) / stratumSize : 0.0;
		}

		for (const auto& meta: clfAtlasPathMetas) {
			pathSummary << oracle_safe_text(clfAtlasModel) << ","
			            << oracle_safe_text(clfAtlasSource) << ","
			            << oracle_safe_text(clfAtlasSourceSeed) << ","
			            << meta.poolDecision << "," << meta.level << ","
			            << oracle_safe_text(meta.pathSignature) << ","
			            << oracle_safe_text(meta.ancestorGate) << ","
			            << meta.count << "," << meta.modalCount << ","
			            << meta.discardedCount << "," << meta.maxOvershootLayer
			            << "," << std::setprecision(12) << meta.totalWeight << "\n";
		}
		for (size_t index = 0ul; index < clfAtlasMetas.size(); ++index) {
			const Traial& snapshot(clfAtlasCandidates[index].get());
			const ClfAtlasCandidateMeta& meta(clfAtlasMetas[index]);
			candidateFrame << oracle_safe_text(clfAtlasModel) << ","
			               << oracle_safe_text(clfAtlasSource) << ","
			               << oracle_safe_text(clfAtlasSourceSeed) << ","
			               << oracle_safe_text(stateIds[index]) << ","
			               << meta.poolDecision << "," << meta.candidateIndex << ","
			               << sideNames[meta.side] << "," << stratumNames[meta.stratum] << ","
			               << meta.level << "," << meta.thresholdLow << ","
			               << meta.thresholdHigh << "," << meta.candidateReachedValue
			               << "," << meta.modalReachedValue << ","
			               << (meta.retained ? 1 : 0) << ","
			               << (meta.retained ? 0 : 1) << ","
			               << (meta.rare ? 1 : 0) << ","
			               << oracle_safe_text(meta.pathSignature) << ","
			               << oracle_safe_text(meta.ancestorGate) << ","
			               << meta.overshootLayer << "," << meta.frameSize << ","
			               << meta.stratumSize << "," << meta.sampleSize << ","
			               << meta.inclusionProbability << "," << meta.candidateCount
			               << "," << meta.modalCount << "," << meta.reachedGroupCount
			               << "," << meta.reachedGroupTotal << ","
			               << meta.pathSignatureCount << "," << meta.modalSignatureCount
			               << "," << meta.discardedSignatureCount << ","
			               << meta.novelDiscardedSignatureCount << ","
			               << meta.overshootCount << "," << std::setprecision(12)
			               << meta.candidateWeight << "," << meta.allWeight << ","
			               << meta.modalWeight << "," << meta.discardedWeight << ","
			               << oracle_safe_text(meta.configuredPoolMode) << ","
			               << oracle_safe_text(meta.effectivePoolMode) << ","
			               << oracle_safe_text(meta.reachedHist) << ","
			               << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
			               << oracle_safe_text(oracle_clock_values(snapshot)) << ","
			               << snapshot.lifeTime << "," << (selected[index] ? 1 : 0)
			               << "\n";
		}
		if (0ul == sampledOrdinal) {
			candidateFrame.flush();
			pathSummary.flush();
			sampledStates.flush();
			continuations.flush();
			std::_Exit(EXIT_SUCCESS);
		}

		const size_t trialsRequired = std::max<size_t>(
			1ul, oracle_env_size("CLF_ATLAS_CONTINUATION_TRIALS", 256ul));
		const unsigned long originalSeed = Clock::rng_seed();
		if (0ul == originalSeed)
			throw_FigException("CLF Atlas requires a deterministic non-zero base RNG seed");
		const auto modelClocks(model_->clocks());
		std::unordered_map<std::string,size_t> oracleStatePositions;
		for (size_t i = 0ul; i < oracleStateNames.size(); ++i)
			oracleStatePositions.emplace(oracleStateNames[i], i);
		auto oracleHasSuffix = [](const std::string& value,
		                          const std::string& suffix) {
			return value.size() >= suffix.size() &&
			       0 == value.compare(value.size()-suffix.size(),
			                          suffix.size(), suffix);
		};
		auto oracleResampleActiveClocks = [&](Traial& clone) {
			if (property_->is_rare(clone.state) || property_->is_stop(clone.state))
				return static_cast<size_t>(0ul);
			auto timeouts(TraialPool::get_timeouts(clone));
			if (timeouts.size() != modelClocks.size())
				throw_FigException("CLF Atlas clock vector size mismatch");
			size_t resampled = 0ul;
			for (size_t i = 0ul; i < timeouts.size(); ++i) {
				auto& timeout(timeouts[i]);
				const bool failureClock = oracleHasSuffix(timeout.name, "_fc");
				const bool repairClock = oracleHasSuffix(timeout.name, "_rc");
				if (!failureClock && !repairClock)
					continue;
				const std::string base(timeout.name.substr(0, timeout.name.size()-3ul));
				const auto position(oracleStatePositions.find(base + "_broken"));
				if (position == oracleStatePositions.end())
					continue;
				const auto broken(clone.state[position->second]);
				const bool active = failureClock ? 0 == broken : 2 == broken;
				if (!active || timeout.sampled <= 0.0f || timeout.value <= 0.0f)
					continue;
				const Clock& clock(modelClocks[i].get());
				clock.resample(timeout.sampled, timeout.value);
				resampled++;
			}
			TraialPool::set_timeouts(clone, std::move(timeouts));
			return resampled;
		};
		EventWatcher continuationWatcher = [this](
			const Property& property, Traial& traial, Event&) {
				return property.is_rare(traial.state) ||
				       property.is_stop(traial.state);
			};
		auto runContinuation = [&](const Traial& snapshot,
		                           unsigned long continuationSeed) {
			OracleContinuationOutcome outcome;
			for (const auto& clockRef: modelClocks)
				clockRef.get().reset_distribution_rng_state();
			Clock::reseed_for_diagnostics(continuationSeed);
			Traial& clone(tpool.get_traial());
			clone = snapshot;
			outcome.initialPreserved =
				clone.state == snapshot.state &&
				clone.lifeTime == snapshot.lifeTime &&
				clone.level == snapshot.level &&
				clone.weight == snapshot.weight &&
				oracle_same_timeouts(clone, snapshot);
			outcome.resampledClockCount = oracleResampleActiveClocks(clone);
			try {
				if (property_->is_rare(clone.state)) {
					outcome.ok = true;
					outcome.hit = true;
					outcome.status = "already_rare";
				} else {
					model_->simulation_step(clone, *property_, continuationWatcher);
					outcome.ok = true;
					outcome.hit = property_->is_rare(clone.state);
					outcome.status = "ok";
				}
			} catch (const FigException& error) {
				outcome.status = oracle_safe_text(error.msg());
			} catch (const std::exception& error) {
				outcome.status = oracle_safe_text(error.what());
			} catch (...) {
				outcome.status = "unknown_exception";
			}
			outcome.lifeAfter = clone.lifeTime;
			outcome.finalSignature = oracle_full_signature(clone, oracleStateNames);
			tpool.return_traial(std::move(clone));
			return outcome;
		};
		auto writeContinuation = [&](const std::string& stateId,
		                             size_t rep,
		                             const std::string& probe,
		                             unsigned long continuationSeed,
		                             const Traial& snapshot,
		                             const OracleContinuationOutcome& outcome) {
			continuations << oracle_safe_text(clfAtlasModel) << ","
			              << oracle_safe_text(clfAtlasSource) << ","
			              << oracle_safe_text(clfAtlasSourceSeed) << ","
			              << stateId << "," << rep << "," << probe << ","
			              << continuationSeed << "," << (outcome.hit ? 1 : 0)
			              << "," << oracle_safe_text(outcome.status) << ","
			              << std::setprecision(12) << snapshot.lifeTime << ","
			              << outcome.lifeAfter << ","
			              << (outcome.initialPreserved ? 1 : 0) << ","
			              << outcome.resampledClockCount << ","
			              << oracle_safe_text(outcome.finalSignature) << "\n";
		};

		for (size_t index = 0ul; index < clfAtlasCandidates.size(); ++index) {
			if (!selected[index])
				continue;
			const Traial& snapshot(clfAtlasCandidates[index].get());
			const ClfAtlasCandidateMeta& meta(clfAtlasMetas[index]);
			const std::string& stateId(stateIds[index]);
			auto planIt = seedPlan.find(stateId);
			if (planIt == seedPlan.end() || planIt->second.size() < trialsRequired)
				throw_FigException("incomplete CLF Atlas seed plan for " + stateId);
			const auto& seeds(planIt->second);
			if (0ul == seeds[0])
				throw_FigException("zero continuation seed for " + stateId);
			const std::string snapshotBefore(
				oracle_full_signature(snapshot, oracleStateNames));
			const auto probeA(runContinuation(snapshot, seeds[0]));
			const auto probeB(runContinuation(snapshot, seeds[0]));
			writeContinuation(stateId, 0ul, "same_seed_a", seeds[0], snapshot, probeA);
			writeContinuation(stateId, 0ul, "same_seed_b", seeds[0], snapshot, probeB);
			const bool sameSeedReproducible = probeA.ok && probeB.ok &&
				probeA.hit == probeB.hit &&
				probeA.finalSignature == probeB.finalSignature;
			size_t hits = 0ul;
			size_t trials = 0ul;
			size_t failures = 0ul;
			bool allInitialPreserved = probeA.initialPreserved && probeB.initialPreserved;
			std::string firstFinal, secondFinal;
			for (size_t attempt = 0ul; attempt < trialsRequired; ++attempt) {
				if (0ul == seeds[attempt]) {
					failures++;
					continue;
				}
				const auto outcome(runContinuation(snapshot, seeds[attempt]));
				writeContinuation(
					stateId, attempt+1ul, "0", seeds[attempt], snapshot, outcome);
				allInitialPreserved = allInitialPreserved && outcome.initialPreserved;
				if (!outcome.ok) {
					failures++;
					continue;
				}
				trials++;
				if (outcome.hit)
					hits++;
				if (trials == 1ul)
					firstFinal = outcome.finalSignature;
				else if (trials == 2ul)
					secondFinal = outcome.finalSignature;
			}
			const auto interval(oracle_wilson_interval(hits, trials));
			const double hHat = trials > 0ul
				? static_cast<double>(hits)/static_cast<double>(trials) : 0.0;
			const bool differentSeedDistinct = !firstFinal.empty() &&
				!secondFinal.empty() && firstFinal != secondFinal;
			const bool cloneUnchanged = snapshotBefore ==
				oracle_full_signature(snapshot, oracleStateNames);
			const ImportanceValue sourceImportance = impFun_->importance_of(
				snapshot.state,
				snapshot.clocks_values());
			sampledStates << oracle_safe_text(clfAtlasModel) << ","
			              << oracle_safe_text(clfAtlasSource) << ","
			              << oracle_safe_text(clfAtlasSourceSeed) << ","
			              << stateId << "," << meta.poolDecision << ","
			              << meta.candidateIndex << "," << sideNames[meta.side]
			              << "," << stratumNames[meta.stratum] << ","
			              << meta.level << "," << std::setprecision(12)
			              << snapshot.weight << "," << snapshot.lifeTime << ","
			              << meta.thresholdLow << "," << meta.thresholdHigh << ","
			              << meta.candidateReachedValue << ","
			              << meta.modalReachedValue << ","
			              << (meta.retained ? 1 : 0) << ","
			              << (meta.retained ? 0 : 1) << ","
			              << (meta.rare ? 1 : 0) << ","
			              << oracle_safe_text(meta.pathSignature) << ","
			              << oracle_safe_text(meta.ancestorGate) << ","
			              << meta.overshootLayer << "," << meta.frameSize << ","
			              << meta.stratumSize << "," << meta.sampleSize << ","
			              << meta.inclusionProbability << "," << meta.candidateWeight
			              << "," << meta.allWeight << "," << meta.modalWeight
			              << "," << meta.discardedWeight << "," << sourceImportance
			              << "," << oracleImportanceMax << ","
			              << oracle_safe_text(oracleThresholdValues.str()) << ","
			              << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
			              << oracle_safe_text(oracle_clock_values(snapshot)) << ","
			              << hits << "," << trials << "," << hHat << ","
			              << interval.first << "," << interval.second << ","
			              << (interval.second-interval.first)/2.0 << ","
			              << failures << "," << (sameSeedReproducible ? 1 : 0)
			              << "," << (differentSeedDistinct ? 1 : 0) << ","
			              << probeA.resampledClockCount << ","
			              << (cloneUnchanged ? 1 : 0) << ","
			              << (allInitialPreserved ? 1 : 0) << "\n";
		}
		candidateFrame.flush();
		pathSummary.flush();
		sampledStates.flush();
		continuations.flush();
		if (!candidateFrame || !pathSummary || !sampledStates || !continuations)
			throw_FigException("failed flushing CLF Atlas output files");
		std::_Exit(EXIT_SUCCESS);
		Clock::reseed_for_diagnostics(originalSeed);
		for (const auto& clockRef: modelClocks)
			clockRef.get().reset_distribution_rng_state();
		tpool.return_traials(clfAtlasCandidates);
		interrupted = true;
	}

	if (clfOracleActive) {
		const OracleSeedPlan seedPlan(oracle_load_seed_plan(clfOracleSeedFile));
		const auto oracleThresholds(impFun_->thresholds());
		std::ostringstream oracleThresholdValues;
		for (size_t i = 0ul; i < oracleThresholds.size(); ++i) {
			if (i > 0ul)
				oracleThresholdValues << "|";
			oracleThresholdValues << oracleThresholds[i].first;
		}
		const ImportanceValue oracleImportanceMax = impFun_->max_value(true);
		const std::string stem = clfOracleOutputDir + "/" +
			oracle_file_token(clfOracleModel) + "_" +
			oracle_file_token(clfOracleSource);
		std::ofstream poolDecisions((stem + "_pool_decisions.csv").c_str(), std::ios::trunc);
		std::ofstream rawStates((stem + "_raw_states.csv").c_str(), std::ios::trunc);
		std::ofstream continuations((stem + "_continuations.csv").c_str(), std::ios::trunc);
		if (!poolDecisions || !rawStates || !continuations)
			throw_FigException("cannot open CLF oracle output files under " + clfOracleOutputDir);
		poolDecisions << "model_id,source,state_id,pool_decision,candidate_index,"
		                 "stratum,level,threshold_low,threshold_high,"
		                 "candidate_reached_value,modal_reached_value,"
		                 "retained,discarded,rare,candidate_count,modal_count,"
		                 "group_count,group_count_total,overshoot_count,"
		                 "candidate_weight,group_weight,all_weight,keep_weight,drop_weight,"
		                 "configured_pool_mode,effective_pool_mode,path_signature\n";
		rawStates << "model_id,source,state_id,pool_decision,candidate_index,"
		             "stratum,level,weight,life_time,threshold_low,threshold_high,"
		             "candidate_reached_value,modal_reached_value,"
		             "retained,discarded,rare,candidate_count,modal_count,"
		             "group_count,group_count_total,overshoot_count,"
		             "candidate_weight,group_weight,all_weight,keep_weight,drop_weight,"
		             "configured_pool_mode,effective_pool_mode,"
		             "source_importance,source_importance_max,source_threshold_values,"
		             "state_values,clock_values,path_signature,hits,trials,h_hat,"
		             "wilson_low,wilson_high,wilson_half_width,failure_count,"
		             "same_seed_reproducible,different_seed_distinct,"
		             "resampled_active_clock_count,"
		             "clone_unchanged,initial_clock_lifetime_preserved\n";
		continuations << "model_id,source,state_id,rep,probe,continuation_seed,"
		                 "hit,status,life_before,life_after,initial_preserved,"
		                 "resampled_clock_count,final_signature\n";
		if (clfOracleSamples.empty()) {
			poolDecisions.flush();
			rawStates.flush();
			continuations.flush();
			std::_Exit(EXIT_SUCCESS);
		}

		const size_t minTrials = std::max<size_t>(200ul,
			oracle_env_size("CLF_ORACLE_MIN_TRIALS", 200ul));
		const size_t maxTrials = std::max<size_t>(minTrials,
			oracle_env_size("CLF_ORACLE_MAX_TRIALS", 2000ul));
		const double halfWidthStop = oracle_env_double(
			"CLF_ORACLE_WILSON_HALF_WIDTH", 0.03);
		const unsigned long originalSeed = Clock::rng_seed();
		if (0ul == originalSeed)
			throw_FigException("CLF oracle requires a deterministic non-zero base RNG seed");
		const auto modelClocks(model_->clocks());
		std::unordered_map<std::string,size_t> oracleStatePositions;
		for (size_t i = 0ul; i < oracleStateNames.size(); ++i)
			oracleStatePositions.emplace(oracleStateNames[i], i);
		auto oracleHasSuffix = [](const std::string& value,
		                          const std::string& suffix) {
			return value.size() >= suffix.size() &&
			       0 == value.compare(value.size()-suffix.size(),
			                          suffix.size(), suffix);
		};
		auto oracleResampleActiveClocks = [&](Traial& clone) {
			if (property_->is_rare(clone.state) || property_->is_stop(clone.state))
				return static_cast<size_t>(0ul);
			auto timeouts(TraialPool::get_timeouts(clone));
			if (timeouts.size() != modelClocks.size())
				throw_FigException("CLF oracle clock vector size mismatch");
			size_t resampled = 0ul;
			for (size_t i = 0ul; i < timeouts.size(); ++i) {
				auto& timeout(timeouts[i]);
				const bool failureClock = oracleHasSuffix(timeout.name, "_fc");
				const bool repairClock = oracleHasSuffix(timeout.name, "_rc");
				if (!failureClock && !repairClock)
					continue;
				const std::string base(timeout.name.substr(0, timeout.name.size()-3ul));
				const auto position(oracleStatePositions.find(base + "_broken"));
				if (position == oracleStatePositions.end())
					continue;
				const auto broken(clone.state[position->second]);
				const bool active = failureClock ? 0 == broken : 2 == broken;
				if (!active || timeout.sampled <= 0.0f || timeout.value <= 0.0f)
					continue;
				const Clock& clock(modelClocks[i].get());
				clock.resample(timeout.sampled, timeout.value);
				resampled++;
			}
			TraialPool::set_timeouts(clone, std::move(timeouts));
			return resampled;
		};
		EventWatcher continuationWatcher = [this](
			const Property& property, Traial& traial, Event&) {
				return property.is_rare(traial.state) ||
				       property.is_stop(traial.state);
			};

		auto runContinuation = [&](const Traial& snapshot,
		                           unsigned long continuationSeed) {
			OracleContinuationOutcome outcome;
			for (const auto& clockRef: modelClocks)
				clockRef.get().reset_distribution_rng_state();
			Clock::reseed_for_diagnostics(continuationSeed);
			Traial& clone(tpool.get_traial());
			clone = snapshot;
			outcome.initialPreserved =
				clone.state == snapshot.state &&
				clone.lifeTime == snapshot.lifeTime &&
				clone.level == snapshot.level &&
				clone.weight == snapshot.weight &&
				oracle_same_timeouts(clone, snapshot);
			outcome.resampledClockCount = oracleResampleActiveClocks(clone);
			try {
				if (property_->is_rare(clone.state)) {
					outcome.ok = true;
					outcome.hit = true;
					outcome.status = "already_rare";
				} else {
					model_->simulation_step(clone, *property_, continuationWatcher);
					outcome.ok = true;
					outcome.hit = property_->is_rare(clone.state);
					outcome.status = "ok";
				}
			} catch (const FigException& error) {
				outcome.status = oracle_safe_text(error.msg());
			} catch (const std::exception& error) {
				outcome.status = oracle_safe_text(error.what());
			} catch (...) {
				outcome.status = "unknown_exception";
			}
			outcome.lifeAfter = clone.lifeTime;
			outcome.finalSignature = oracle_full_signature(clone, oracleStateNames);
			tpool.return_traial(std::move(clone));
			return outcome;
		};

		auto writeContinuation = [&](const std::string& stateId,
		                             size_t rep,
		                             const std::string& probe,
		                             unsigned long continuationSeed,
		                             const Traial& snapshot,
		                             const OracleContinuationOutcome& outcome) {
			continuations << oracle_safe_text(clfOracleModel) << ","
			              << oracle_safe_text(clfOracleSource) << ","
			              << stateId << "," << rep << "," << probe << ","
			              << continuationSeed << "," << (outcome.hit ? 1 : 0) << ","
			              << oracle_safe_text(outcome.status) << ","
			              << std::setprecision(12) << snapshot.lifeTime << ","
			              << outcome.lifeAfter << ","
			              << (outcome.initialPreserved ? 1 : 0) << ","
			              << outcome.resampledClockCount << ","
			              << oracle_safe_text(outcome.finalSignature) << "\n";
		};

		const char* stratumNames[] = {"early", "middle", "late"};
		for (size_t index = 0ul; index < clfOracleSamples.size(); ++index) {
			const Traial& snapshot(clfOracleSamples[index].get());
			const ClfOracleSampleMeta& meta(clfOracleMetas[index]);
			std::ostringstream stateIdBuilder;
			stateIdBuilder << clfOracleSource << "_" << std::setw(3)
			               << std::setfill('0') << index;
			const std::string stateId(stateIdBuilder.str());
			poolDecisions << oracle_safe_text(clfOracleModel) << ","
			              << oracle_safe_text(clfOracleSource) << ","
			              << stateId << "," << meta.poolDecision << ","
			              << meta.candidateIndex << ","
			              << stratumNames[meta.stratum] << "," << meta.level << ","
			              << meta.thresholdLow << "," << meta.thresholdHigh << ","
			              << meta.candidateReachedValue << ","
			              << meta.modalReachedValue << ","
			              << (meta.retained ? 1 : 0) << ","
			              << (meta.retained ? 0 : 1) << ","
			              << (meta.rare ? 1 : 0) << ","
			              << meta.candidateCount << "," << meta.modalCount << ","
			              << meta.groupCount << "," << meta.groupCountTotal << ","
			              << meta.overshootCount << ","
			              << std::setprecision(12) << meta.candidateWeight << ","
			              << meta.groupWeight << "," << meta.allWeight << ","
			              << meta.keepWeight << "," << meta.dropWeight << ","
			              << oracle_safe_text(meta.configuredPoolMode) << ","
			              << oracle_safe_text(meta.effectivePoolMode) << ","
			              << oracle_safe_text(meta.pathSignature) << "\n";
			auto planIt = seedPlan.find(stateId);
			if (planIt == seedPlan.end() || planIt->second.size() < minTrials)
				throw_FigException("incomplete CLF oracle seed plan for " + stateId);
			const auto& seeds(planIt->second);
			const size_t attemptLimit = std::min(maxTrials, seeds.size());
			if (0ul == seeds[0])
				throw_FigException("zero continuation seed for " + stateId);

			const std::string snapshotBefore(
				oracle_full_signature(snapshot, oracleStateNames));
			const auto probeA(runContinuation(snapshot, seeds[0]));
			const auto probeB(runContinuation(snapshot, seeds[0]));
			writeContinuation(stateId, 0ul, "same_seed_a", seeds[0], snapshot, probeA);
			writeContinuation(stateId, 0ul, "same_seed_b", seeds[0], snapshot, probeB);
			const bool sameSeedReproducible = probeA.ok && probeB.ok &&
				probeA.hit == probeB.hit &&
				probeA.finalSignature == probeB.finalSignature;

			size_t hits = 0ul;
			size_t trials = 0ul;
			size_t failures = 0ul;
			bool allInitialPreserved = probeA.initialPreserved && probeB.initialPreserved;
			std::string firstFinal, secondFinal;
			std::pair<double,double> interval(0.0, 1.0);
			for (size_t attempt = 0ul; attempt < attemptLimit; ++attempt) {
				if (0ul == seeds[attempt]) {
					failures++;
					continue;
				}
				const auto outcome(runContinuation(snapshot, seeds[attempt]));
				writeContinuation(
					stateId, attempt+1ul, "0", seeds[attempt], snapshot, outcome);
				allInitialPreserved = allInitialPreserved && outcome.initialPreserved;
				if (!outcome.ok) {
					failures++;
					continue;
				}
				trials++;
				if (outcome.hit)
					hits++;
				if (trials == 1ul)
					firstFinal = outcome.finalSignature;
				else if (trials == 2ul)
					secondFinal = outcome.finalSignature;
				interval = oracle_wilson_interval(hits, trials);
				if (trials >= minTrials && 0ul == failures &&
				    (interval.second-interval.first)/2.0 <= halfWidthStop)
					break;
			}
			interval = oracle_wilson_interval(hits, trials);
			const double hHat = trials > 0ul
				? static_cast<double>(hits)/static_cast<double>(trials) : 0.0;
			const bool differentSeedDistinct = !firstFinal.empty() &&
				!secondFinal.empty() && firstFinal != secondFinal;
			const bool cloneUnchanged = snapshotBefore ==
				oracle_full_signature(snapshot, oracleStateNames);
			const ImportanceValue sourceImportance = impFun_->importance_of(
				snapshot.state,
				snapshot.clocks_values());
			rawStates << oracle_safe_text(clfOracleModel) << ","
			          << oracle_safe_text(clfOracleSource) << "," << stateId << ","
			          << meta.poolDecision << "," << meta.candidateIndex << ","
			          << stratumNames[meta.stratum] << "," << meta.level << ","
			          << std::setprecision(12) << snapshot.weight << ","
			          << snapshot.lifeTime << ","
			          << meta.thresholdLow << "," << meta.thresholdHigh << ","
			          << meta.candidateReachedValue << ","
			          << meta.modalReachedValue << ","
			          << (meta.retained ? 1 : 0) << ","
			          << (meta.retained ? 0 : 1) << ","
			          << (meta.rare ? 1 : 0) << ","
			          << meta.candidateCount << "," << meta.modalCount << ","
			          << meta.groupCount << "," << meta.groupCountTotal << ","
			          << meta.overshootCount << ","
			          << meta.candidateWeight << "," << meta.groupWeight << ","
			          << meta.allWeight << "," << meta.keepWeight << ","
			          << meta.dropWeight << ","
			          << oracle_safe_text(meta.configuredPoolMode) << ","
			          << oracle_safe_text(meta.effectivePoolMode) << ","
			          << sourceImportance << "," << oracleImportanceMax << ","
			          << oracle_safe_text(oracleThresholdValues.str()) << ","
			          << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
			          << oracle_safe_text(oracle_clock_values(snapshot)) << ","
			          << oracle_safe_text(meta.pathSignature) << ","
			          << hits << "," << trials << "," << hHat << ","
			          << interval.first << "," << interval.second << ","
			          << (interval.second-interval.first)/2.0 << ","
			          << failures << "," << (sameSeedReproducible ? 1 : 0) << ","
			          << (differentSeedDistinct ? 1 : 0) << ","
			          << probeA.resampledClockCount << ","
			          << (cloneUnchanged ? 1 : 0) << ","
			          << (allInitialPreserved ? 1 : 0) << "\n";
		}
		poolDecisions.flush();
		rawStates.flush();
		continuations.flush();
		if (!poolDecisions || !rawStates || !continuations)
			throw_FigException("failed flushing CLF oracle output files");
		std::_Exit(EXIT_SUCCESS);
		Clock::reseed_for_diagnostics(originalSeed);
		for (const auto& clockRef: modelClocks)
			clockRef.get().reset_distribution_rng_state();
		tpool.return_traials(clfOracleSamples);
		interrupted = true;
	}

	if (saclfOracleActive) {
		const OracleSeedPlan seedPlan(oracle_load_seed_plan(saclfOracleSeedFile));
		const auto oracleThresholds(impFun_->thresholds());
		std::ostringstream oracleThresholdValues;
		for (size_t i = 0ul; i < oracleThresholds.size(); ++i) {
			if (i > 0ul)
				oracleThresholdValues << "|";
			oracleThresholdValues << oracleThresholds[i].first;
		}
		const ImportanceValue oracleImportanceMax = impFun_->max_value(true);
		const std::string stem = saclfOracleOutputDir + "/" +
			oracle_file_token(saclfOracleModel) + "_" +
			oracle_file_token(saclfOracleSource) + "_" +
			oracle_file_token(saclfOracleSourceSeed);
		std::ofstream candidateFrame((stem + "_candidate_frame.csv").c_str(), std::ios::trunc);
		std::ofstream decisionSummary((stem + "_decision_summary.csv").c_str(), std::ios::trunc);
		std::ofstream sampledStates((stem + "_sampled_states.csv").c_str(), std::ios::trunc);
		std::ofstream continuations((stem + "_continuations.csv").c_str(), std::ios::trunc);
		if (!candidateFrame || !decisionSummary || !sampledStates || !continuations)
			throw_FigException("cannot open SACLF oracle output files under " + saclfOracleOutputDir);
		candidateFrame <<
			"model_id,source,source_seed,state_id,pool_decision,candidate_index,"
			"level,threshold_low,threshold_high,candidate_reached_value,"
			"modal_reached_value,retained,discarded,rare,structural_signature,"
			"frame_size,stratum_size,sample_size,inclusion_probability,"
			"candidate_quality,candidate_weight,lineage_parent_index,"
			"candidate_count,modal_count,reached_group_count,reached_group_total,"
			"structural_signature_count,modal_signature_count,"
			"discarded_signature_count,novel_discarded_signature_count,"
			"structural_opportunity,saclf_activated,configured_pool_mode,"
			"effective_pool_mode,overshoot_count,reached_hist,state_values,"
			"clock_values,life_time\n";
		decisionSummary <<
			"model_id,source,source_seed,pool_decision,level,threshold_low,"
			"threshold_high,candidate_count,retained_count,modal_count,"
			"discarded_success_count,structural_signature_count,"
			"modal_signature_count,discarded_signature_count,"
			"novel_discarded_signature_count,structural_opportunity,"
			"saclf_activated,configured_pool_mode,effective_pool_mode,"
			"overshoot_count,lineage_count,reached_hist\n";
		sampledStates <<
			"model_id,source,source_seed,state_id,pool_decision,candidate_index,"
			"level,weight,life_time,threshold_low,threshold_high,"
			"candidate_reached_value,modal_reached_value,retained,discarded,rare,"
			"structural_signature,frame_size,stratum_size,sample_size,"
			"inclusion_probability,candidate_quality,candidate_weight,"
			"source_importance,source_importance_max,source_threshold_values,"
			"state_values,clock_values,hits,trials,h_hat,wilson_low,"
			"wilson_high,wilson_half_width,failure_count,same_seed_reproducible,"
			"different_seed_distinct,resampled_active_clock_count,clone_unchanged,"
			"initial_clock_lifetime_preserved\n";
		continuations <<
			"model_id,source,source_seed,state_id,rep,probe,continuation_seed,"
			"hit,status,life_before,life_after,initial_preserved,"
			"resampled_clock_count,final_signature\n";

		for (const auto& meta: saclfOracleDecisionMetas) {
			decisionSummary << oracle_safe_text(saclfOracleModel) << ","
			                << oracle_safe_text(saclfOracleSource) << ","
			                << oracle_safe_text(saclfOracleSourceSeed) << ","
			                << meta.poolDecision << "," << meta.level << ","
			                << meta.thresholdLow << "," << meta.thresholdHigh << ","
			                << meta.candidateCount << "," << meta.modalCount << ","
			                << meta.modalCount << ","
			                << (meta.candidateCount > meta.modalCount
			                    ? meta.candidateCount - meta.modalCount : 0ul) << ","
			                << meta.structuralSignatureCount << ","
			                << meta.modalSignatureCount << ","
			                << meta.discardedSignatureCount << ","
			                << meta.novelDiscardedSignatureCount << ","
			                << (meta.structuralOpportunity ? 1 : 0) << ","
			                << (meta.saclfActivated ? 1 : 0) << ","
			                << oracle_safe_text(meta.configuredPoolMode) << ","
			                << oracle_safe_text(meta.effectivePoolMode) << ","
			                << meta.overshootCount << ",0,"
			                << oracle_safe_text(meta.reachedHist) << "\n";
		}

		const size_t minTrials = std::max<size_t>(200ul,
			oracle_env_size("SACLF_ORACLE_MIN_TRIALS", 200ul));
		const size_t maxTrials = std::max<size_t>(minTrials,
			oracle_env_size("SACLF_ORACLE_MAX_TRIALS", 2000ul));
		const double halfWidthStop = oracle_env_double(
			"SACLF_ORACLE_WILSON_HALF_WIDTH", 0.03);
		const unsigned long originalSeed = Clock::rng_seed();
		if (0ul == originalSeed)
			throw_FigException("SACLF oracle requires a deterministic non-zero base RNG seed");
		const auto modelClocks(model_->clocks());
		std::unordered_map<std::string,size_t> oracleStatePositions;
		for (size_t i = 0ul; i < oracleStateNames.size(); ++i)
			oracleStatePositions.emplace(oracleStateNames[i], i);
		auto oracleHasSuffix = [](const std::string& value,
		                          const std::string& suffix) {
			return value.size() >= suffix.size() &&
			       0 == value.compare(value.size()-suffix.size(),
			                          suffix.size(), suffix);
		};
		auto oracleResampleActiveClocks = [&](Traial& clone) {
			if (property_->is_rare(clone.state) || property_->is_stop(clone.state))
				return static_cast<size_t>(0ul);
			auto timeouts(TraialPool::get_timeouts(clone));
			if (timeouts.size() != modelClocks.size())
				throw_FigException("SACLF oracle clock vector size mismatch");
			size_t resampled = 0ul;
			for (size_t i = 0ul; i < timeouts.size(); ++i) {
				auto& timeout(timeouts[i]);
				const bool failureClock = oracleHasSuffix(timeout.name, "_fc");
				const bool repairClock = oracleHasSuffix(timeout.name, "_rc");
				if (!failureClock && !repairClock)
					continue;
				const std::string base(timeout.name.substr(0, timeout.name.size()-3ul));
				const auto position(oracleStatePositions.find(base + "_broken"));
				if (position == oracleStatePositions.end())
					continue;
				const auto broken(clone.state[position->second]);
				const bool active = failureClock ? 0 == broken : 2 == broken;
				if (!active || timeout.sampled <= 0.0f || timeout.value <= 0.0f)
					continue;
				const Clock& clock(modelClocks[i].get());
				clock.resample(timeout.sampled, timeout.value);
				resampled++;
			}
			TraialPool::set_timeouts(clone, std::move(timeouts));
			return resampled;
		};
		EventWatcher continuationWatcher = [this](
			const Property& property, Traial& traial, Event&) {
				return property.is_rare(traial.state) ||
				       property.is_stop(traial.state);
			};
		auto runContinuation = [&](const Traial& snapshot,
		                           unsigned long continuationSeed) {
			OracleContinuationOutcome outcome;
			for (const auto& clockRef: modelClocks)
				clockRef.get().reset_distribution_rng_state();
			Clock::reseed_for_diagnostics(continuationSeed);
			Traial& clone(tpool.get_traial());
			clone = snapshot;
			outcome.initialPreserved =
				clone.state == snapshot.state &&
				clone.lifeTime == snapshot.lifeTime &&
				clone.level == snapshot.level &&
				clone.weight == snapshot.weight &&
				oracle_same_timeouts(clone, snapshot);
			outcome.resampledClockCount = oracleResampleActiveClocks(clone);
			try {
				if (property_->is_rare(clone.state)) {
					outcome.ok = true;
					outcome.hit = true;
					outcome.status = "already_rare";
				} else {
					model_->simulation_step(clone, *property_, continuationWatcher);
					outcome.ok = true;
					outcome.hit = property_->is_rare(clone.state);
					outcome.status = "ok";
				}
			} catch (const FigException& error) {
				outcome.status = oracle_safe_text(error.msg());
			} catch (const std::exception& error) {
				outcome.status = oracle_safe_text(error.what());
			} catch (...) {
				outcome.status = "unknown_exception";
			}
			outcome.lifeAfter = clone.lifeTime;
			outcome.finalSignature = oracle_full_signature(clone, oracleStateNames);
			tpool.return_traial(std::move(clone));
			return outcome;
		};
		auto writeContinuation = [&](const std::string& stateId,
		                             size_t rep,
		                             const std::string& probe,
		                             unsigned long continuationSeed,
		                             const Traial& snapshot,
		                             const OracleContinuationOutcome& outcome) {
			continuations << oracle_safe_text(saclfOracleModel) << ","
			              << oracle_safe_text(saclfOracleSource) << ","
			              << oracle_safe_text(saclfOracleSourceSeed) << ","
			              << stateId << "," << rep << "," << probe << ","
			              << continuationSeed << "," << (outcome.hit ? 1 : 0) << ","
			              << oracle_safe_text(outcome.status) << ","
			              << std::setprecision(12) << snapshot.lifeTime << ","
			              << outcome.lifeAfter << ","
			              << (outcome.initialPreserved ? 1 : 0) << ","
			              << outcome.resampledClockCount << ","
			              << oracle_safe_text(outcome.finalSignature) << "\n";
		};

		for (size_t index = 0ul; index < saclfOracleSamples.size(); ++index) {
			const Traial& snapshot(saclfOracleSamples[index].get());
			const SaclfOracleCandidateMeta& meta(saclfOracleMetas[index]);
			std::ostringstream stateIdBuilder;
			stateIdBuilder << saclfOracleSource << "_" << saclfOracleSourceSeed
			               << "_" << std::setw(5) << std::setfill('0') << index;
			const std::string stateId(stateIdBuilder.str());
			candidateFrame << oracle_safe_text(saclfOracleModel) << ","
			               << oracle_safe_text(saclfOracleSource) << ","
			               << oracle_safe_text(saclfOracleSourceSeed) << ","
			               << stateId << "," << meta.poolDecision << ","
			               << meta.candidateIndex << "," << meta.level << ","
			               << meta.thresholdLow << "," << meta.thresholdHigh << ","
			               << meta.candidateReachedValue << ","
			               << meta.modalReachedValue << ","
			               << (meta.retained ? 1 : 0) << ","
			               << (meta.retained ? 0 : 1) << ","
			               << (meta.rare ? 1 : 0) << ","
			               << oracle_safe_text(meta.structuralSignature) << ","
			               << meta.frameSize << "," << meta.stratumSize << ","
			               << meta.sampleSize << "," << meta.inclusionProbability << ","
			               << meta.candidateQuality << ","
			               << std::setprecision(12) << meta.candidateWeight << ",,"
			               << meta.candidateCount << "," << meta.modalCount << ","
			               << meta.reachedGroupCount << "," << meta.reachedGroupTotal << ","
			               << meta.structuralSignatureCount << ","
			               << meta.modalSignatureCount << ","
			               << meta.discardedSignatureCount << ","
			               << meta.novelDiscardedSignatureCount << ","
			               << (meta.structuralOpportunity ? 1 : 0) << ","
			               << (meta.saclfActivated ? 1 : 0) << ","
			               << oracle_safe_text(meta.configuredPoolMode) << ","
			               << oracle_safe_text(meta.effectivePoolMode) << ","
			               << meta.overshootCount << ","
			               << oracle_safe_text(meta.reachedHist) << ","
			               << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
			               << oracle_safe_text(oracle_clock_values(snapshot)) << ","
			               << snapshot.lifeTime << "\n";

			auto planIt = seedPlan.find(stateId);
			if (planIt == seedPlan.end() || planIt->second.size() < minTrials)
				throw_FigException("incomplete SACLF oracle seed plan for " + stateId);
			const auto& seeds(planIt->second);
			const size_t attemptLimit = std::min(maxTrials, seeds.size());
			if (0ul == seeds[0])
				throw_FigException("zero continuation seed for " + stateId);
			const std::string snapshotBefore(
				oracle_full_signature(snapshot, oracleStateNames));
			const auto probeA(runContinuation(snapshot, seeds[0]));
			const auto probeB(runContinuation(snapshot, seeds[0]));
			writeContinuation(stateId, 0ul, "same_seed_a", seeds[0], snapshot, probeA);
			writeContinuation(stateId, 0ul, "same_seed_b", seeds[0], snapshot, probeB);
			const bool sameSeedReproducible = probeA.ok && probeB.ok &&
				probeA.hit == probeB.hit &&
				probeA.finalSignature == probeB.finalSignature;
			size_t hits = 0ul;
			size_t trials = 0ul;
			size_t failures = 0ul;
			bool allInitialPreserved = probeA.initialPreserved && probeB.initialPreserved;
			std::string firstFinal, secondFinal;
			std::pair<double,double> interval(0.0, 1.0);
			for (size_t attempt = 0ul; attempt < attemptLimit; ++attempt) {
				if (0ul == seeds[attempt]) {
					failures++;
					continue;
				}
				const auto outcome(runContinuation(snapshot, seeds[attempt]));
				writeContinuation(
					stateId, attempt+1ul, "0", seeds[attempt], snapshot, outcome);
				allInitialPreserved = allInitialPreserved && outcome.initialPreserved;
				if (!outcome.ok) {
					failures++;
					continue;
				}
				trials++;
				if (outcome.hit)
					hits++;
				if (trials == 1ul)
					firstFinal = outcome.finalSignature;
				else if (trials == 2ul)
					secondFinal = outcome.finalSignature;
				interval = oracle_wilson_interval(hits, trials);
				if (trials >= minTrials && 0ul == failures &&
				    (interval.second-interval.first)/2.0 <= halfWidthStop)
					break;
			}
			interval = oracle_wilson_interval(hits, trials);
			const double hHat = trials > 0ul
				? static_cast<double>(hits)/static_cast<double>(trials) : 0.0;
			const bool differentSeedDistinct = !firstFinal.empty() &&
				!secondFinal.empty() && firstFinal != secondFinal;
			const bool cloneUnchanged = snapshotBefore ==
				oracle_full_signature(snapshot, oracleStateNames);
			const ImportanceValue sourceImportance = impFun_->importance_of(
				snapshot.state,
				snapshot.clocks_values());
			sampledStates << oracle_safe_text(saclfOracleModel) << ","
			              << oracle_safe_text(saclfOracleSource) << ","
			              << oracle_safe_text(saclfOracleSourceSeed) << ","
			              << stateId << "," << meta.poolDecision << ","
			              << meta.candidateIndex << "," << meta.level << ","
			              << std::setprecision(12) << snapshot.weight << ","
			              << snapshot.lifeTime << "," << meta.thresholdLow << ","
			              << meta.thresholdHigh << "," << meta.candidateReachedValue << ","
			              << meta.modalReachedValue << ","
			              << (meta.retained ? 1 : 0) << ","
			              << (meta.retained ? 0 : 1) << ","
			              << (meta.rare ? 1 : 0) << ","
			              << oracle_safe_text(meta.structuralSignature) << ","
			              << meta.frameSize << "," << meta.stratumSize << ","
			              << meta.sampleSize << "," << meta.inclusionProbability << ","
			              << meta.candidateQuality << "," << meta.candidateWeight << ","
			              << sourceImportance << "," << oracleImportanceMax << ","
			              << oracle_safe_text(oracleThresholdValues.str()) << ","
			              << oracle_safe_text(oracle_state_values(snapshot, oracleStateNames)) << ","
			              << oracle_safe_text(oracle_clock_values(snapshot)) << ","
			              << hits << "," << trials << "," << hHat << ","
			              << interval.first << "," << interval.second << ","
			              << (interval.second-interval.first)/2.0 << ","
			              << failures << "," << (sameSeedReproducible ? 1 : 0) << ","
			              << (differentSeedDistinct ? 1 : 0) << ","
			              << probeA.resampledClockCount << ","
			              << (cloneUnchanged ? 1 : 0) << ","
			              << (allInitialPreserved ? 1 : 0) << "\n";
		}
		candidateFrame.flush();
		decisionSummary.flush();
		sampledStates.flush();
		continuations.flush();
		if (!candidateFrame || !decisionSummary || !sampledStates || !continuations)
			throw_FigException("failed flushing SACLF oracle output files");
		std::_Exit(EXIT_SUCCESS);
		Clock::reseed_for_diagnostics(originalSeed);
		for (const auto& clockRef: modelClocks)
			clockRef.get().reset_distribution_rng_state();
		tpool.return_traials(saclfOracleSamples);
		interrupted = true;
	}
}

} // namespace fig  // // // // // // // // // // // // // // // // // // // //
