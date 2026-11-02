//==============================================================================
//
//  ThresholdsBuilderAdaptiveSimple.cpp
//
//  Copyleft 2016-
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


// C
#include <cmath>  // std::ceil(), std::log(), std::round()
#include <cassert>
// C++
#include <algorithm>
#include <limits>
#include <sstream>
#include <vector>
// External code
#include <uint128_t.h>  // uint128::uint128_0
// FIG
#include <ThresholdsBuilderAdaptiveSimple.h>
#include <ModelSuite.h>
#include <BatsDiagnostics.h>


namespace fig  // // // // // // // // // // // // // // // // // // // // // //
{

namespace
{

double
clamp_effort_probability(double value)
{
	const double minP = bats_effort_min_p();
	if (value < minP)
		return minP;
	if (value > 0.95)
		return 0.95;
	return value;
}


void
apply_bats_effort_schedule(ThresholdsVec& thresholds,
                           const unsigned globEff,
                           const std::string& simEngineName)
{
	const std::string mode = bats_effort_mode();
	if (mode == "uniform" || mode.empty())
		return;
	if (mode != "variance") {
		bats_diag_log("BATS_EFFORT mode=" + mode
		              + " applied=0 reason=unknown_mode");
		return;
	}
	if (thresholds.size() <= 2ul)
		return;

	const size_t first = 1ul;
	const size_t last = thresholds.size() - 2ul;
	const size_t levels = last - first + 1ul;
	const double targetP = bats_target_p() > 0.0
	                       ? bats_target_p()
	                       : 1.0 / static_cast<double>(std::max(1u, globEff));
	const bool sfeMode = simEngineName == "sfe";
	const unsigned effectiveBaseEff = sfeMode ? std::max(8u, globEff) : globEff;
	const double minMult = bats_effort_min_mult();
	const double maxMult = std::max(minMult, bats_effort_max_mult());
	const unsigned minEff = static_cast<unsigned>(
	    std::max(1.0, std::ceil(static_cast<double>(effectiveBaseEff) * minMult)));
	const unsigned maxEff = static_cast<unsigned>(
	    std::min(static_cast<double>(ThresholdsBuilder::MAX_EFFORT),
	             std::max(static_cast<double>(minEff),
	                      std::ceil(static_cast<double>(effectiveBaseEff) * maxMult))));

	std::vector<double> pProxy(levels, clamp_effort_probability(targetP));
	std::vector<double> weights(levels, 1.0);
	for (size_t i = first ; i <= last ; i++) {
		const size_t outIdx = i - first;
		const double p = pProxy[outIdx];
		weights[outIdx] = std::sqrt((1.0 - p) / p);
	}

	double weightSum = 0.0;
	for (const auto weight: weights)
		weightSum += weight;
	if (weightSum <= std::numeric_limits<double>::epsilon())
		return;
	const double scale = static_cast<double>(effectiveBaseEff) *
	                     static_cast<double>(levels) / weightSum;
	unsigned requestedSum = 0u;
	unsigned effectiveSum = 0u;
	std::ostringstream vectorLog;
	for (size_t i = first ; i <= last ; i++) {
		const size_t outIdx = i - first;
		unsigned effort = static_cast<unsigned>(std::round(weights[outIdx] * scale));
		effort = std::max(1u, effort);
		effort = std::max(minEff, std::min(maxEff, effort));
		thresholds[i].second = effort;
		requestedSum += effort;
		effectiveSum += sfeMode ? std::max(8u, effort) : effort;
		if (outIdx > 0ul)
			vectorLog << ",";
		vectorLog << thresholds[i].first << ":" << effort
		          << ":p" << pProxy[outIdx];
		bats_diag_log("BATS_EFFORT_LEVEL index=" + std::to_string(i)
		              + " threshold_low=" + std::to_string(thresholds[i].first)
		              + " threshold_high=" + std::to_string(thresholds[i+1ul].first)
		              + " artificial=0"
		              + " p_proxy=" + std::to_string(pProxy[outIdx])
		              + " weight=" + std::to_string(weights[outIdx])
		              + " requested_effort=" + std::to_string(effort)
		              + " effective_effort=" + std::to_string(sfeMode ? std::max(8u, effort) : effort));
	}
	bats_diag_log("BATS_EFFORT mode=variance levels=" + std::to_string(levels)
	              + " base_effort=" + std::to_string(globEff)
	              + " effective_base_effort=" + std::to_string(effectiveBaseEff)
	              + " sim_engine=" + simEngineName
	              + " requested_sum=" + std::to_string(requestedSum)
	              + " effective_sum=" + std::to_string(effectiveSum)
	              + " min_mult=" + std::to_string(minMult)
	              + " max_mult=" + std::to_string(maxMult)
	              + " gap_power=0"
	              + " target_p=" + std::to_string(targetP)
	              + " vector=" + vectorLog.str());
}

} // namespace

ThresholdsBuilderAdaptiveSimple::ThresholdsBuilderAdaptiveSimple(
    const unsigned& n,
    const unsigned& k) :
        ThresholdsBuilderAdaptive(n),
        property_(nullptr),
        globEff_(0u),
        k_(k),
        thresholds_()
{ /* Not much to do around here */ }


void
ThresholdsBuilderAdaptiveSimple::setup(std::shared_ptr<const Property> property,
                                       const void *globalEffort)
{
	auto ge = *static_cast<const int*>(globalEffort);
	assert(0 < ge);
	assert(ge <= static_cast<decltype(ge)>(MAX_EFFORT));
	property_ = property;
	globEff_ = static_cast<decltype(globEff_)>(ge);
}


ThresholdsVec
ThresholdsBuilderAdaptiveSimple::build_thresholds(std::shared_ptr<const ImportanceFunction> impFun)
{
	// For flat importance function we need a dummy thresholds vector
	if (globEff_ < 2u) {
		ImportanceVec({impFun->initial_value(),impFun->max_value()+1}).swap(thresholds_);
		goto consistency_check;
	}

	// Choose values for n_ and k_
	simEngineName_ = impFun->sim_engine_bound();
	tune(ModelSuite::get_instance().modules_network()->num_transitions(),
	     impFun->max_value() - impFun->min_value(),
	     globEff_);
	ModelSuite::tech_log("Building thresholds with \""+ name +"\" for global "
	                     "effort = " + std::to_string(globEff_) + " and k/n = "
						 + std::to_string(k_) + "/" + std::to_string(n_) + " ≈ "
						 + std::to_string(static_cast<float>(k_)/n_) + "\n");

	// Run the adaptive algorithm to choose the thresholds
	build_thresholds_vector(*impFun);
	show_thresholds(thresholds_);
consistency_check:
	assert(!thresholds_.empty());
	assert(thresholds_[0] == impFun->initial_value());
	assert(thresholds_.back() == 1 + impFun->max_value());

	// Build ThresholdsVec to return
	ThresholdsVec thresholds;
	thresholds.reserve(thresholds_.size());
	for (auto imp: thresholds_)
		thresholds.emplace_back(imp, globEff_);
	thresholds.front().second = 1ul;  // fake first thr
	thresholds.back().second = 1ul;   // fake last thre
	apply_bats_effort_schedule(thresholds, globEff_, simEngineName_);

	return thresholds;
}


void
ThresholdsBuilderAdaptiveSimple::tune(const size_t& numTrans,
                                      const ImportanceValue& maxImportance,
                                      const unsigned& globalEffort)
{
    //assert(uint128::uint128_0 < numStates);
	assert(0ul < numTrans);
	assert(0u < globalEffort);

	ImportanceVec().swap(thresholds_);
	thresholds_.reserve(MAX_NUM_THRESHOLDS);
	assert(thresholds_.size() == 0u);
	assert(thresholds_.capacity() > 0u);

	/// @todo FIXME Following choices are heuristic -- Replace with calculi
	///             based on mathematical analysis if possible

	// Heuristic for 'n_':
	//   the more importance values, the more independent runs we need
	//   for some of them to be successfull.
	//   The same applies to the number of edges (aka symbolic transitions).
	ThresholdsBuilderAdaptive::tune(numTrans, maxImportance, globalEffort);

    // Heuristic for 'k_':
    //   splitsPerThr * levelUpProb == 1  ("balanced growth")
    //   where levelUpProb == k_/n_
	k_ = static_cast<decltype(k_)>(
	            std::round(n_ / static_cast<float>(globalEffort)));
	const double targetP = bats_target_p();
	if (targetP > 0.0) {
		const auto requestedK = static_cast<unsigned>(std::round(n_ * targetP));
		k_ = std::max(1u, std::min(requestedK, n_ - 1u));
		bats_diag_log("BATS_TARGET_P value=" + std::to_string(targetP)
		              + " n=" + std::to_string(n_)
		              + " k=" + std::to_string(k_)
		              + " actual_p=" + std::to_string(static_cast<double>(k_) / n_));
	}

    assert(0u < k_ || static_cast<ImportanceValue>(1u) >= maxImportance);
    assert(k_ < n_ || static_cast<ImportanceValue>(1u) >= maxImportance);
}

} // namespace fig  // // // // // // // // // // // // // // // // // // // //
