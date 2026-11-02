//==============================================================================
//
//  ThresholdsBuilderHybrid.cpp
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
#include <cassert>
#include <cmath>
#include <pthread.h>
// C++
#include <utility>
#include <sstream>
#include <thread>
#include <functional>  // std::ref()
#include <algorithm>
#include <limits>
#include <vector>
// FIG
#include <ThresholdsBuilderHybrid.h>
#include <ImportanceFunction.h>
#include <FigException.h>
#include <FigLog.h>
#include <BatsDiagnostics.h>


namespace fig  // // // // // // // // // // // // // // // // // // // // // //
{

namespace
{

constexpr ImportanceValue MAX_BATS_ARTIFICIAL = 8u;

ImportanceValue
fixed_repair_stride(const ImportanceFunction& impFun,
                    const ImportanceValue& margin)
{
	const auto artRange = impFun.max_value() - margin;
	return std::max<ImportanceValue>(
	    1u,
	    (artRange + MAX_BATS_ARTIFICIAL - 1u) / MAX_BATS_ARTIFICIAL);
}


ImportanceVec
build_tail_quantile_repair_thresholds(const ImportanceFunction& impFun,
                                      const ImportanceValue& margin,
                                      const ImportanceVec& reachedSample,
                                      bool& fallback,
                                      std::string& fallbackReason)
{
	fallback = false;
	fallbackReason.clear();

	const ImportanceValue maxImp = impFun.max_value();
	const ImportanceValue boundary = maxImp + static_cast<ImportanceValue>(1u);
	ImportanceVec result;
	result.reserve(MAX_BATS_ARTIFICIAL + 2u);
	result.push_back(margin);

	ImportanceVec tail;
	tail.reserve(reachedSample.size());
	for (const auto imp: reachedSample) {
		if (imp > margin && imp <= maxImp)
			tail.push_back(imp);
	}

	if (tail.size() < 3ul) {
		fallback = true;
		fallbackReason = "sparse_tail";
		return result;
	}

	std::sort(tail.begin(), tail.end());
	const ImportanceValue tailMax = tail.back();
	if (tailMax <= margin) {
		fallback = true;
		fallbackReason = "no_tail_progress";
		return result;
	}

	const size_t maxArtificial =
	    std::min<size_t>(MAX_BATS_ARTIFICIAL, maxImp - margin);
	for (size_t i = 1ul ; i <= maxArtificial ; i++) {
		const double q = static_cast<double>(i) /
		                 static_cast<double>(maxArtificial + 1ul);
		const size_t pos = std::min<size_t>(
		    tail.size() - 1ul,
		    static_cast<size_t>(std::floor(q * static_cast<double>(tail.size() - 1ul))));
		const ImportanceValue empirical = tail[pos];
		const ImportanceValue linear = margin + static_cast<ImportanceValue>(
		    std::ceil(q * static_cast<double>(boundary - margin)));
		const double linearWeight = q * q;
		const ImportanceValue candidate = static_cast<ImportanceValue>(std::ceil(
		    (1.0 - linearWeight) * static_cast<double>(empirical)
		    + linearWeight * static_cast<double>(linear)));
		if (candidate > margin && candidate < boundary)
			result.push_back(candidate);
	}

	result.push_back(boundary);
	std::sort(result.begin(), result.end());
	auto newEnd = std::unique(result.begin(), result.end());
	result.erase(newEnd, result.end());

	if (result.size() < 3ul || result.back() != boundary) {
		fallback = true;
		fallbackReason = "degenerate_chain";
		return result;
	}

	return result;
}


size_t
artificial_threshold_count(const ImportanceVec& thresholds,
                           const size_t artificialFromIndex)
{
	if (thresholds.size() <= artificialFromIndex + 1ul)
		return 0ul;
	return thresholds.size() - artificialFromIndex - 1ul;
}


double
clamp_probability(double value)
{
	const double minP = bats_effort_min_p();
	if (value < minP)
		return minP;
	if (value > 0.95)
		return 0.95;
	return value;
}


void
apply_bats_effort_schedule(ThresholdsVec& result,
                           const unsigned globEff,
                           const size_t artificialFromIndex,
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
	if (result.size() <= 2ul)
		return;

	const size_t first = 1ul;
	const size_t last = result.size() - 2ul;
	const size_t levels = last - first + 1ul;
	const double targetP = bats_target_p() > 0.0
	                       ? bats_target_p()
	                       : 1.0 / static_cast<double>(std::max(1u, globEff));
	const double gapPower = bats_effort_gap_power();
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
	const size_t artificialStart = artificialFromIndex == static_cast<size_t>(-1)
	                               ? result.size()
	                               : (artificialFromIndex > 0ul
	                                  ? artificialFromIndex - 1ul
	                                  : artificialFromIndex);

	std::vector<double> pProxy(levels, targetP);
	std::vector<double> weights(levels, 1.0);
	size_t maxGap = 1ul;
	for (size_t i = first ; i <= last ; i++) {
		const size_t gap = result[i+1ul].first > result[i].first
		                   ? result[i+1ul].first - result[i].first
		                   : 1ul;
		maxGap = std::max(maxGap, gap);
	}
	for (size_t i = first ; i <= last ; i++) {
		const size_t outIdx = i - first;
		const size_t gap = result[i+1ul].first > result[i].first
		                   ? result[i+1ul].first - result[i].first
		                   : 1ul;
		double p = targetP;
		if (i >= artificialStart) {
			const double gapFactor = std::pow(
			    static_cast<double>(gap) / static_cast<double>(maxGap),
			    gapPower);
			p = targetP * std::exp(-gapFactor);
		}
		p = clamp_probability(p);
		pProxy[outIdx] = p;
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
		result[i].second = effort;
		requestedSum += effort;
		effectiveSum += sfeMode ? std::max(8u, effort) : effort;
		if (outIdx > 0ul)
			vectorLog << ",";
		vectorLog << result[i].first << ":" << effort
		          << ":p" << pProxy[outIdx];
		bats_diag_log("BATS_EFFORT_LEVEL index=" + std::to_string(i)
		              + " threshold_low=" + std::to_string(result[i].first)
		              + " threshold_high=" + std::to_string(result[i+1ul].first)
		              + " artificial=" + std::to_string(i >= artificialStart ? 1 : 0)
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
	              + " gap_power=" + std::to_string(gapPower)
	              + " target_p=" + std::to_string(targetP)
	              + " vector=" + vectorLog.str());
}

} // namespace

ThresholdsBuilderHybrid::ThresholdsBuilderHybrid(ImportanceValue minImpRange,
                                                 ImportanceValue expandEvery) :
    ThresholdsBuilder("hyb"),
    ThresholdsBuilderFixed(minImpRange,expandEvery),
    globEff_(0u)
{ /* Not much to do around here */ }


void
ThresholdsBuilderHybrid::setup(std::shared_ptr<const Property> prop,
                               const void* ge)
{
	globEff_ = static_cast<decltype(globEff_)>(*static_cast<const int*>(ge));
	assert(0 < globEff_);
	assert(globEff_ <= MAX_EFFORT);
	ThresholdsBuilderFixed::setup(prop, ge);
	ThresholdsBuilderAdaptiveSimple::setup(prop, ge);
}


ThresholdsVec
ThresholdsBuilderHybrid::build_thresholds(std::shared_ptr<const ImportanceFunction> impFun)
{
	size_t artificialFromIndex = static_cast<size_t>(-1);
	// Impose an execution wall time limit...
	const std::chrono::minutes TIMEOUT = ADAPTIVE_TIMEOUT;  // take by value!
	std::thread timer([] (bool& halt, const std::chrono::minutes& limit)
					  { std::this_thread::sleep_for(limit); halt=true; },
					  std::ref(halted_), TIMEOUT);
	try {
		// Start out using an adaptive technique, which may just work btw...
		halted_ = false;
		ThresholdsBuilderSMC::build_thresholds(impFun);
		pthread_cancel(timer.native_handle());  // ...it worked!

	} catch (FigException&) {
		// Adaptive algorithm couldn't finish but achievements remain
		// stored in the vector member 'thresholds_'
		assert(thresholds_.empty() ||
		       thresholds_.back() >= impFun->initial_value());
		const size_t MARGIN(thresholds_.empty() ? impFun->initial_value()
		                                        : thresholds_.back());
		if (highVerbosity)
			figTechLog << "\nSequential Monte Carlo couldn't reach the rare event!"
			           << "\nNow setting artificial thresholds above the "
					   << "ImportanceValue " << MARGIN << "\n";
		else
			figTechLog << "\nChoosing artificial thresholds above " << MARGIN << "\n";
		postPro_ = impFun->post_processing();
		artificialFromIndex = thresholds_.size();
		const std::string repairMode = bats_repair_mode();
		bool repairFallback = false;
		std::string fallbackReason;
		const auto& reachedSample = last_reached_importance_sample();
		ImportanceValue tailMax = MARGIN;
		size_t tailCount = 0ul;
		for (const auto imp: reachedSample) {
			if (imp > MARGIN && imp <= impFun->max_value()) {
				tailMax = std::max(tailMax, imp);
				tailCount++;
			}
		}
		stride_ = fixed_repair_stride(*impFun, MARGIN);
		if (repairMode == "tail_quantile") {
			ImportanceVec repaired = build_tail_quantile_repair_thresholds(
			    *impFun, MARGIN, reachedSample, repairFallback, fallbackReason);
			if (!repairFallback) {
				for (size_t i = 1ul ; i < repaired.size() ; i++)
					thresholds_.push_back(repaired[i]);
				std::sort(thresholds_.begin(), thresholds_.end());
				auto newEnd = std::unique(thresholds_.begin(), thresholds_.end());
				thresholds_.erase(newEnd, thresholds_.end());
			} else {
				ThresholdsBuilderFixed::build_thresholds(*impFun,
				                                         MARGIN-impFun->initial_value(),
				                                         stride_,
				                                         thresholds_);
			}
		} else {
			if (repairMode != "fixed")
				repairFallback = true;
			fallbackReason = repairMode == "fixed" ? "none" : "unknown_mode";
			ThresholdsBuilderFixed::build_thresholds(*impFun,
			                                         MARGIN-impFun->initial_value(),
			                                         stride_,
			                                         thresholds_);
		}
		const size_t artificialCount = artificial_threshold_count(thresholds_, artificialFromIndex);
		bats_diag_log("HYB_REPAIR margin=" + std::to_string(MARGIN)
		              + " max_importance=" + std::to_string(impFun->max_value())
		              + " stride=" + std::to_string(stride_)
		              + " artificial_from_index=" + std::to_string(artificialFromIndex)
		              + " artificial_count=" + std::to_string(artificialCount)
		              + " repair_mode=" + repairMode
		              + " repair_fallback=" + std::to_string(repairFallback ? 1 : 0)
		              + " fallback_reason=" + fallbackReason
		              + " repair_sample_count=" + std::to_string(reachedSample.size())
		              + " tail_count=" + std::to_string(tailCount)
		              + " tail_max=" + std::to_string(tailMax));
		halted_ = true;

	} catch (std::exception&) {
		timer.detach();
		throw;
	}

	// Tidy-up
	ImportanceVec thresholds;
	std::swap(thresholds, thresholds_);
	timer.detach();

	if (halted_)
		show_thresholds(thresholds);
	assert(!thresholds.empty());
	assert(thresholds[0] == impFun->initial_value());
	assert(thresholds.back() == 1 + impFun->max_value());
	if (bats_diag_enabled() || bats_sfe_count_only_enabled()) {
		std::ostringstream chain;
		for (size_t i = 0ul ; i < thresholds.size() ; i++) {
			if (i > 0ul)
				chain << ",";
			chain << thresholds[i];
		}
		std::ostringstream msg;
		msg << "THRESHOLD_CHAIN values=" << chain.str()
		    << " artificial_from_index="
		    << (artificialFromIndex == static_cast<size_t>(-1)
		        ? std::string("-1")
		        : std::to_string(artificialFromIndex));
		if (bats_diag_enabled())
			bats_diag_log(msg.str());
		else
			ModelSuite::tech_log(msg.str() + "\n");
	}

	// Build ThresholdsVec to return
	ThresholdsVec result;
	result.reserve(thresholds.size());
	for (auto imp: thresholds)
		result.emplace_back(imp, globEff_);
	result.front().second = 1ul;  // fake first thr
	result.back().second = 1ul;   // fake last thre
	apply_bats_effort_schedule(result, globEff_, artificialFromIndex,
	                           impFun->sim_engine_bound());

	return result;
}


ImportanceValue
ThresholdsBuilderHybrid::choose_stride(const size_t& impRange) const
{
	ImportanceValue basicStride(1u), expansionFactor(1u);
	assert(globEff_ > 1u);
	if (impRange < MIN_IMP_RANGE)
		return basicStride;  // Don't even bother

	// What follows is clearly arbitrary, but then we warned the user
	// in the class' docstring, didn't we?
	switch (postPro_.type)
	{
	case (PostProcessing::NONE):
	case (PostProcessing::SHIFT):
		basicStride = globEff_ <  4u ? 2u :      // 2,3 -------------> 2
		              globEff_ <  7u ? 3u :      // 4,5,6 -----------> 3
		              globEff_ < 11u ? 4u :      // 7,8,9,10 --------> 4
		              globEff_ < 16u ? 5u : 6u;  // 11,12,13,14,15 --> 5
//		XXX A narrower alternative:
//		basicStride = globEff_ <  6u ? 2u :      // 2,3,4,5 ---------> 2
//					  globEff_ < 11u ? 3u :      // 6,7,8,9,10 ------> 3
//					  globEff_ < 16u ? 4u : 5u;  // 11,12,13,14,15 --> 4
		expansionFactor = std::ceil(static_cast<float>(impRange) / EXPAND_EVERY);
		// Make sure return type can represent the computed stride
		assert(std::log2(static_cast<float>(basicStride)*expansionFactor)
				< sizeof(decltype(basicStride))*8.0f);
		return basicStride * expansionFactor;
		break;

	case (PostProcessing::EXP):
		basicStride = globEff_ < 4u ? 1u :      // 2,3 ------> 1
		              globEff_ < 7u ? 2u : 3u;  // 4,5,6 ----> 2
		expansionFactor = std::ceil(std::log(impRange) / EXPAND_EVERY);
		// Make sure return type can represent the computed stride
		assert(basicStride*expansionFactor < sizeof(decltype(basicStride))*8u);
		return std::pow(postPro_.value, basicStride*expansionFactor);
		break;

	default:
		throw_FigException("invalid post-processing \"" + postPro_.name
		                  + "\" (" + std::to_string(postPro_.type) + ")");
		break;
	}
}


} // namespace fig  // // // // // // // // // // // // // // // // // // // //
