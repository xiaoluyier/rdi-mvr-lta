//==============================================================================
//
//  SequentialStateAllocation.h
//
//  Minimal state-conditioned sequential allocation for modal SFE.
//
//==============================================================================

#ifndef SEQUENTIALSTATEALLOCATION_H
#define SEQUENTIALSTATEALLOCATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>


namespace fig
{

inline void
ssta_hash_byte(std::uint64_t& hash, const unsigned char byte)
{
    hash ^= static_cast<std::uint64_t>(byte);
    hash *= 1099511628211ull;
}


inline void
ssta_hash_size(std::uint64_t& hash, size_t value)
{
    for (size_t i = 0ul; i < sizeof(size_t); ++i) {
        ssta_hash_byte(hash, static_cast<unsigned char>(value & 0xffu));
        value >>= 8u;
    }
}


inline double
ssta_resampling_offset(const size_t baseSeed,
                       const size_t runIndex,
                       const size_t level,
                       const std::string& signature)
{
    std::uint64_t hash = 1469598103934665603ull;
    ssta_hash_size(hash, baseSeed);
    ssta_hash_size(hash, runIndex);
    ssta_hash_size(hash, level);
    for (const unsigned char byte: signature)
        ssta_hash_byte(hash, byte);
    const std::uint64_t mantissa = hash >> 11u;
    return static_cast<double>(mantissa) / 9007199254740992.0;
}

inline bool
ssta_has_suffix(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}


template<typename Values>
std::string
ssta_pand_state_signature(const std::vector<std::string>& names,
                          const Values& values)
{
    std::string signature;
    const size_t count = std::min(names.size(), values.size());
    for (size_t i = 0ul; i < count; ++i) {
        if (!ssta_has_suffix(names[i], "_st"))
            continue;
        if (!signature.empty())
            signature += "|";
        signature += names[i] + "=" + std::to_string(values[i]);
    }
    return signature.empty() ? "-" : signature;
}


struct SstaAdvanceStats
{
    size_t trials;
    size_t advanced;

    SstaAdvanceStats(const size_t trialCount = 0ul,
                     const size_t advancedCount = 0ul) :
        trials(trialCount),
        advanced(advancedCount)
    {
        if (advanced > trials)
            throw std::invalid_argument(
                "SSTA advanced count must not exceed trial count");
    }
};


inline double
ssta_advance_probability(const SstaAdvanceStats& stats)
{
    return (static_cast<double>(stats.advanced) + 0.5) /
           (static_cast<double>(stats.trials) + 1.0);
}


class SequentialStateHistory
{
private:
    typedef std::pair<size_t,std::string> Key;
    std::map<Key,SstaAdvanceStats> history_;

public:
    void reset()
    {
        history_.clear();
    }

    SstaAdvanceStats get(const size_t level,
                         const std::string& signature) const
    {
        const auto found = history_.find(Key(level, signature));
        return found == history_.end() ? SstaAdvanceStats() : found->second;
    }

    void observe_batch(const size_t level,
                       const std::string& signature,
                       const size_t trials,
                       const size_t advanced)
    {
        if (advanced > trials)
            throw std::invalid_argument(
                "SSTA batch advanced count must not exceed trial count");
        SstaAdvanceStats& stats = history_[Key(level, signature)];
        stats.trials += trials;
        stats.advanced += advanced;
    }
};


struct SstaGroupRequest
{
    std::string signature;
    double mass;
    SstaAdvanceStats history;

    SstaGroupRequest(const std::string& groupSignature,
                     const double groupMass,
                     const SstaAdvanceStats& groupHistory) :
        signature(groupSignature),
        mass(groupMass),
        history(groupHistory)
    {}
};


struct SstaGroupAllocation
{
    std::string signature;
    double mass;
    SstaAdvanceStats history;
    double advance_probability;
    double score;
    size_t children;

    SstaGroupAllocation() :
        signature(),
        mass(0.0),
        history(),
        advance_probability(0.5),
        score(0.0),
        children(0ul)
    {}
};


struct SstaAllocationPlan
{
    bool valid;
    std::string reason;
    size_t total_children;
    std::vector<SstaGroupAllocation> groups;

    SstaAllocationPlan() :
        valid(false),
        reason(),
        total_children(0ul),
        groups()
    {}
};


inline SstaAllocationPlan
ssta_allocate_groups(const size_t totalChildren,
                     const std::vector<SstaGroupRequest>& requests,
                     const double probabilityFloor)
{
    if (totalChildren == 0ul)
        throw std::invalid_argument("SSTA total population must be positive");
    if (!std::isfinite(probabilityFloor) ||
        probabilityFloor <= 0.0 || probabilityFloor >= 1.0)
        throw std::invalid_argument(
            "SSTA probability floor must lie in (0,1)");

    SstaAllocationPlan plan;
    if (requests.empty()) {
        plan.reason = "no_positive_mass_group";
        return plan;
    }
    if (requests.size() > totalChildren) {
        plan.reason = "group_count_exceeds_population";
        return plan;
    }

    double totalMass = 0.0;
    for (const auto& request: requests) {
        if (!std::isfinite(request.mass) || request.mass <= 0.0)
            throw std::invalid_argument(
                "SSTA group mass must be finite and positive");
        totalMass += request.mass;
    }
    if (!std::isfinite(totalMass) || totalMass <= 0.0)
        throw std::invalid_argument(
            "SSTA total group mass must be finite and positive");

    plan.groups.resize(requests.size());
    double totalScore = 0.0;
    for (size_t i = 0ul; i < requests.size(); ++i) {
        SstaGroupAllocation& group = plan.groups[i];
        group.signature = requests[i].signature;
        group.mass = requests[i].mass / totalMass;
        group.history = requests[i].history;
        group.advance_probability = ssta_advance_probability(group.history);
        const double q = std::max(group.advance_probability, probabilityFloor);
        group.score = group.mass * std::sqrt((1.0 - q) / q);
        group.children = 1ul;
        totalScore += group.score;
    }

    if (!std::isfinite(totalScore) || totalScore <= 0.0) {
        totalScore = 0.0;
        for (auto& group: plan.groups) {
            group.score = group.mass;
            totalScore += group.score;
        }
    }

    const size_t remaining = totalChildren - plan.groups.size();
    std::vector<double> remainders(plan.groups.size(), 0.0);
    size_t assignedExtra = 0ul;
    for (size_t i = 0ul; i < plan.groups.size(); ++i) {
        const double ideal = static_cast<double>(remaining) *
                             plan.groups[i].score / totalScore;
        const size_t extra = static_cast<size_t>(std::floor(ideal));
        plan.groups[i].children += extra;
        assignedExtra += extra;
        remainders[i] = ideal - static_cast<double>(extra);
    }

    std::vector<size_t> order(plan.groups.size(), 0ul);
    std::iota(order.begin(), order.end(), 0ul);
    std::stable_sort(order.begin(), order.end(),
        [&](const size_t lhs, const size_t rhs) {
            if (remainders[lhs] != remainders[rhs])
                return remainders[lhs] > remainders[rhs];
            return plan.groups[lhs].signature < plan.groups[rhs].signature;
        });
    for (size_t i = 0ul; i < remaining - assignedExtra; ++i)
        plan.groups[order[i]].children++;

    plan.valid = true;
    plan.reason = "none";
    plan.total_children = totalChildren;
    return plan;
}


inline std::vector<size_t>
ssta_systematic_resample(const std::vector<double>& weights,
                         const size_t sampleCount,
                         const double offsetUnit)
{
    if (weights.empty())
        throw std::invalid_argument("SSTA resampling requires parent weights");
    if (sampleCount == 0ul)
        throw std::invalid_argument("SSTA resampling sample count must be positive");
    if (!std::isfinite(offsetUnit) || offsetUnit < 0.0 || offsetUnit >= 1.0)
        throw std::invalid_argument(
            "SSTA resampling offset must lie in [0,1)");

    double totalWeight = 0.0;
    for (const double weight: weights) {
        if (!std::isfinite(weight) || weight < 0.0)
            throw std::invalid_argument(
                "SSTA parent weight must be finite and non-negative");
        totalWeight += weight;
    }
    if (!std::isfinite(totalWeight) || totalWeight <= 0.0)
        throw std::invalid_argument(
            "SSTA parent weights must contain positive mass");

    std::vector<size_t> selected;
    selected.reserve(sampleCount);
    size_t parent = 0ul;
    double cumulative = weights[0] / totalWeight;
    for (size_t sample = 0ul; sample < sampleCount; ++sample) {
        const double position =
            (offsetUnit + static_cast<double>(sample)) /
            static_cast<double>(sampleCount);
        while (parent + 1ul < weights.size() && position >= cumulative) {
            ++parent;
            cumulative += weights[parent] / totalWeight;
        }
        selected.push_back(parent);
    }
    return selected;
}


// Fixed-Budget State-Conditioned Sequential Trajectory Allocation (FB-SSTA).
// The legacy SSTA helpers above are intentionally preserved unchanged so that
// disabling the new switch retains the exact historical execution path.

struct FbSstaOutcomeStats
{
    size_t trials;
    size_t successes;
    size_t retained;
    size_t event_transitions;

    FbSstaOutcomeStats(const size_t trialCount = 0ul,
                       const size_t successCount = 0ul,
                       const size_t retainedCount = 0ul,
                       const size_t eventTransitionCount = 0ul) :
        trials(trialCount),
        successes(successCount),
        retained(retainedCount),
        event_transitions(eventTransitionCount)
    {
        if (successes > trials || retained > trials)
            throw std::invalid_argument(
                "FB-SSTA outcome count must not exceed trial count");
    }
};


struct FbSstaEstimates
{
    double q;
    double rho;
    double cost;
    double effective_sample_size;

    FbSstaEstimates() :
        q(0.5), rho(0.5), cost(1.0), effective_sample_size(0.0)
    {}
};


inline double
fb_ssta_rate(const size_t successes, const size_t trials)
{
    return (static_cast<double>(successes) + 0.5) /
           (static_cast<double>(trials) + 1.0);
}


inline FbSstaEstimates
fb_ssta_shrink_estimates(const FbSstaOutcomeStats& group,
                         const FbSstaOutcomeStats& level,
                         const double priorStrength,
                         const double probabilityFloor)
{
    if (!std::isfinite(priorStrength) || priorStrength <= 0.0)
        throw std::invalid_argument(
            "FB-SSTA prior strength must be finite and positive");
    if (!std::isfinite(probabilityFloor) ||
        probabilityFloor <= 0.0 || probabilityFloor >= 0.5)
        throw std::invalid_argument(
            "FB-SSTA probability floor must lie in (0,0.5)");

    const double levelQ = fb_ssta_rate(level.successes, level.trials);
    const double levelRho = fb_ssta_rate(level.retained, level.trials);
    const double groupQ = fb_ssta_rate(group.successes, group.trials);
    const double groupRho = fb_ssta_rate(group.retained, group.trials);
    const double shrink = static_cast<double>(group.trials) /
        (static_cast<double>(group.trials) + priorStrength);
    const double levelCost = level.trials > 0ul
        ? static_cast<double>(level.event_transitions) /
          static_cast<double>(level.trials) : 1.0;
    const double groupCost = group.trials > 0ul
        ? static_cast<double>(group.event_transitions) /
          static_cast<double>(group.trials) : levelCost;

    FbSstaEstimates result;
    result.q = std::max(probabilityFloor, std::min(1.0 - probabilityFloor,
        shrink * groupQ + (1.0 - shrink) * levelQ));
    result.rho = std::max(probabilityFloor, std::min(1.0 - probabilityFloor,
        shrink * groupRho + (1.0 - shrink) * levelRho));
    result.cost = std::max(1.0,
        shrink * groupCost + (1.0 - shrink) * levelCost);
    result.effective_sample_size =
        static_cast<double>(group.trials) * shrink;
    return result;
}


class FbSequentialStateHistory
{
private:
    typedef std::pair<size_t,std::string> Key;
    std::map<Key,FbSstaOutcomeStats> groups_;
    std::map<size_t,FbSstaOutcomeStats> levels_;
    std::map<Key,size_t> previousAllocations_;

public:
    void reset()
    {
        groups_.clear();
        levels_.clear();
        previousAllocations_.clear();
    }

    FbSstaOutcomeStats group(const size_t level,
                             const std::string& signature) const
    {
        const auto found = groups_.find(Key(level, signature));
        return found == groups_.end() ? FbSstaOutcomeStats() : found->second;
    }

    FbSstaOutcomeStats level(const size_t levelIndex) const
    {
        const auto found = levels_.find(levelIndex);
        return found == levels_.end() ? FbSstaOutcomeStats() : found->second;
    }

    size_t previous_allocation(const size_t level,
                               const std::string& signature) const
    {
        const auto found = previousAllocations_.find(Key(level, signature));
        return found == previousAllocations_.end() ? 0ul : found->second;
    }

    void observe_batch(const size_t level,
                       const std::string& signature,
                       const FbSstaOutcomeStats& batch,
                       const size_t allocation)
    {
        FbSstaOutcomeStats& groupStats = groups_[Key(level, signature)];
        groupStats.trials += batch.trials;
        groupStats.successes += batch.successes;
        groupStats.retained += batch.retained;
        groupStats.event_transitions += batch.event_transitions;
        FbSstaOutcomeStats& levelStats = levels_[level];
        levelStats.trials += batch.trials;
        levelStats.successes += batch.successes;
        levelStats.retained += batch.retained;
        levelStats.event_transitions += batch.event_transitions;
        previousAllocations_[Key(level, signature)] = allocation;
    }
};


struct FbSstaConfig
{
    size_t max_groups;
    double merge_mass_threshold;
    double exploration_mass_threshold;
    size_t exploration_children;
    double max_group_share;
    double max_replication_factor;
    double max_batch_change_share;
    double prior_strength;
    double probability_floor;
    double diversity_lambda;
    double information_threshold;

    FbSstaConfig() :
        max_groups(8ul),
        merge_mass_threshold(0.02),
        exploration_mass_threshold(0.03),
        exploration_children(1ul),
        max_group_share(0.60),
        max_replication_factor(4.0),
        max_batch_change_share(0.20),
        prior_strength(12.0),
        probability_floor(0.02),
        diversity_lambda(0.05),
        information_threshold(8.0)
    {}
};


struct FbSstaGroupRequest
{
    std::string signature;
    double mass;
    FbSstaOutcomeStats history;
    size_t previous_children;
    size_t fixed_children;

    FbSstaGroupRequest(const std::string& groupSignature,
                       const double groupMass,
                       const FbSstaOutcomeStats& groupHistory,
                       const size_t previousChildren = 0ul,
                       const size_t fixedChildren = 0ul) :
        signature(groupSignature),
        mass(groupMass),
        history(groupHistory),
        previous_children(previousChildren),
        fixed_children(fixedChildren)
    {}
};


struct FbSstaGroupAllocation
{
    std::string signature;
    double mass;
    FbSstaOutcomeStats history;
    FbSstaEstimates estimates;
    size_t previous_children;
    size_t fixed_children;
    size_t children;
    double score;

    FbSstaGroupAllocation() :
        signature(), mass(0.0), history(), estimates(),
        previous_children(0ul), fixed_children(0ul), children(0ul), score(0.0)
    {}
};


struct FbSstaAllocationPlan
{
    bool valid;
    bool merged;
    std::string fallback_reason;
    size_t total_children;
    double predicted_cost;
    double fixed_predicted_cost;
    double allocation_ess;
    std::vector<FbSstaGroupAllocation> groups;

    FbSstaAllocationPlan() :
        valid(false), merged(false), fallback_reason(), total_children(0ul),
        predicted_cost(0.0), fixed_predicted_cost(0.0), allocation_ess(0.0),
        groups()
    {}
};


inline double
fb_ssta_child_mass(const double groupMass, const size_t children)
{
    if (!std::isfinite(groupMass) || groupMass < 0.0)
        throw std::invalid_argument(
            "FB-SSTA group mass must be finite and non-negative");
    if (children == 0ul)
        throw std::invalid_argument(
            "FB-SSTA child count must be positive");
    return groupMass / static_cast<double>(children);
}


inline std::vector<size_t>
fb_ssta_largest_remainder(const size_t total,
                          const std::vector<double>& weights,
                          const std::vector<std::string>& signatures)
{
    std::vector<size_t> result(weights.size(), 0ul);
    if (weights.empty())
        return result;
    double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
    if (!std::isfinite(sum) || sum <= 0.0)
        throw std::invalid_argument("FB-SSTA allocation weights must be positive");
    std::vector<double> remainder(weights.size(), 0.0);
    size_t assigned = 0ul;
    for (size_t i = 0ul; i < weights.size(); ++i) {
        const double ideal = static_cast<double>(total) * weights[i] / sum;
        result[i] = static_cast<size_t>(std::floor(ideal));
        remainder[i] = ideal - static_cast<double>(result[i]);
        assigned += result[i];
    }
    std::vector<size_t> order(weights.size(), 0ul);
    std::iota(order.begin(), order.end(), 0ul);
    std::stable_sort(order.begin(), order.end(), [&](size_t lhs, size_t rhs) {
        if (remainder[lhs] != remainder[rhs])
            return remainder[lhs] > remainder[rhs];
        return signatures[lhs] < signatures[rhs];
    });
    for (size_t i = 0ul; i < total - assigned; ++i)
        result[order[i]]++;
    return result;
}


inline double
fb_ssta_objective(const FbSstaGroupAllocation& group,
                  const size_t children,
                  const double lambda)
{
    if (children == 0ul)
        return std::numeric_limits<double>::infinity();
    const double q = group.estimates.q;
    const double variance = group.mass * group.mass * (1.0 - q) /
        (static_cast<double>(children) * q);
    const double preservation = lambda * group.mass *
        std::pow(1.0 - group.estimates.rho,
                 static_cast<double>(children));
    return variance + preservation;
}


inline FbSstaAllocationPlan
fb_ssta_allocate_groups(const size_t totalChildren,
                        const std::vector<FbSstaGroupRequest>& input,
                        const FbSstaConfig& config)
{
    if (totalChildren == 0ul)
        throw std::invalid_argument("FB-SSTA total population must be positive");
    FbSstaAllocationPlan plan;
    if (input.empty()) {
        plan.fallback_reason = "no_positive_mass_group";
        return plan;
    }
    if (config.max_groups == 0ul ||
        !std::isfinite(config.max_group_share) ||
        config.max_group_share <= 0.0 || config.max_group_share > 1.0)
        throw std::invalid_argument("invalid FB-SSTA configuration");

    std::vector<FbSstaGroupRequest> requests;
    for (const auto& request: input) {
        if (!std::isfinite(request.mass) || request.mass <= 0.0)
            throw std::invalid_argument(
                "FB-SSTA group mass must be finite and positive");
        requests.push_back(request);
    }
    std::stable_sort(requests.begin(), requests.end(),
        [](const FbSstaGroupRequest& lhs, const FbSstaGroupRequest& rhs) {
            if (lhs.mass != rhs.mass)
                return lhs.mass > rhs.mass;
            return lhs.signature < rhs.signature;
        });

    size_t rareCount = 0ul;
    for (const auto& request: requests)
        if (request.mass < config.merge_mass_threshold)
            ++rareCount;
    if (rareCount >= 2ul) {
        std::vector<FbSstaGroupRequest> compact;
        FbSstaOutcomeStats mergedStats;
        double mergedMass = 0.0;
        size_t mergedPrevious = 0ul;
        size_t mergedFixed = 0ul;
        for (const auto& request: requests) {
            if (request.mass >= config.merge_mass_threshold) {
                compact.push_back(request);
                continue;
            }
            mergedMass += request.mass;
            mergedStats.trials += request.history.trials;
            mergedStats.successes += request.history.successes;
            mergedStats.retained += request.history.retained;
            mergedStats.event_transitions += request.history.event_transitions;
            mergedPrevious += request.previous_children;
            mergedFixed += request.fixed_children;
        }
        compact.emplace_back("__other__", mergedMass, mergedStats,
                             mergedPrevious, mergedFixed);
        requests.swap(compact);
        plan.merged = true;
    }

    const size_t groupLimit = std::max<size_t>(1ul,
        std::min(config.max_groups, totalChildren));
    if (requests.size() > groupLimit) {
        const size_t keep = groupLimit > 1ul ? groupLimit - 1ul : 0ul;
        FbSstaOutcomeStats mergedStats;
        double mergedMass = 0.0;
        size_t mergedPrevious = 0ul;
        size_t mergedFixed = 0ul;
        for (size_t i = keep; i < requests.size(); ++i) {
            mergedMass += requests[i].mass;
            mergedStats.trials += requests[i].history.trials;
            mergedStats.successes += requests[i].history.successes;
            mergedStats.retained += requests[i].history.retained;
            mergedStats.event_transitions += requests[i].history.event_transitions;
            mergedPrevious += requests[i].previous_children;
            mergedFixed += requests[i].fixed_children;
        }
        requests.erase(requests.begin() + keep, requests.end());
        requests.emplace_back("__other__", mergedMass, mergedStats,
                              mergedPrevious, mergedFixed);
        plan.merged = true;
    }

    double totalMass = 0.0;
    FbSstaOutcomeStats levelStats;
    for (const auto& request: requests) {
        totalMass += request.mass;
        levelStats.trials += request.history.trials;
        levelStats.successes += request.history.successes;
        levelStats.retained += request.history.retained;
        levelStats.event_transitions += request.history.event_transitions;
    }
    std::vector<double> masses;
    std::vector<std::string> signatures;
    for (const auto& request: requests) {
        FbSstaGroupAllocation group;
        group.signature = request.signature;
        group.mass = request.mass / totalMass;
        group.history = request.history;
        group.previous_children = request.previous_children;
        group.fixed_children = request.fixed_children;
        group.estimates = fb_ssta_shrink_estimates(
            request.history, levelStats, config.prior_strength,
            config.probability_floor);
        group.score = group.mass * std::sqrt(
            (1.0 - group.estimates.q) / group.estimates.q);
        plan.groups.push_back(group);
        masses.push_back(group.mass);
        signatures.push_back(group.signature);
    }

    std::vector<size_t> allocation =
        fb_ssta_largest_remainder(totalChildren, masses, signatures);
    std::vector<size_t> fixedAllocation(plan.groups.size(), 0ul);
    size_t suppliedFixed = 0ul;
    for (size_t i = 0ul; i < plan.groups.size(); ++i) {
        fixedAllocation[i] = plan.groups[i].fixed_children;
        suppliedFixed += fixedAllocation[i];
    }
    if (suppliedFixed != totalChildren)
        fixedAllocation = allocation;
    double totalInformation = 0.0;
    for (const auto& group: plan.groups)
        totalInformation += group.estimates.effective_sample_size;
    bool distinguishable = false;
    for (size_t i = 1ul; i < plan.groups.size(); ++i) {
        distinguishable = distinguishable ||
            std::fabs(plan.groups[i].estimates.q - plan.groups[0].estimates.q) > 1e-12 ||
            std::fabs(plan.groups[i].estimates.rho - plan.groups[0].estimates.rho) > 1e-12 ||
            std::fabs(plan.groups[i].estimates.cost - plan.groups[0].estimates.cost) > 1e-12;
    }
    const bool informed = totalInformation >= config.information_threshold &&
                          distinguishable;
    if (!informed)
        plan.fallback_reason = "insufficient_information";

    const size_t shareCap = plan.groups.size() == 1ul
        ? totalChildren
        : std::max<size_t>(1ul, static_cast<size_t>(
            std::ceil(config.max_group_share * totalChildren)));
    // Every retained/merged group must receive at least one child.  The group
    // count is capped by totalChildren above, so this is always feasible and
    // avoids losing positive probability mass through a zero-child group.
    std::vector<size_t> lower(plan.groups.size(), 1ul);
    std::vector<size_t> upper(plan.groups.size(), totalChildren);
    const size_t changeCap = std::max<size_t>(1ul,
        static_cast<size_t>(std::ceil(config.max_batch_change_share * totalChildren)));
    for (size_t i = 0ul; i < plan.groups.size(); ++i) {
        if (plan.groups[i].mass >= config.exploration_mass_threshold)
            lower[i] = std::min(config.exploration_children, totalChildren);
        upper[i] = std::min(shareCap, std::max<size_t>(1ul,
            static_cast<size_t>(std::ceil(
                config.max_replication_factor *
                static_cast<double>(std::max<size_t>(1ul, fixedAllocation[i]))))));
        if (plan.groups[i].previous_children > 0ul) {
            lower[i] = std::max(lower[i],
                plan.groups[i].previous_children > changeCap
                ? plan.groups[i].previous_children - changeCap : 0ul);
            upper[i] = std::min(upper[i],
                plan.groups[i].previous_children + changeCap);
        }
        if (lower[i] > upper[i])
            lower[i] = upper[i];
    }

    // Deterministically project the quality-proportional fallback onto the
    // exploration/concentration/change box.  This keeps every fallback path
    // inside the same hard constraints as the informed optimizer.
    bool projectionChanged = true;
    while (projectionChanged) {
        projectionChanged = false;
        for (size_t donor = 0ul; donor < allocation.size(); ++donor) {
            if (allocation[donor] <= upper[donor])
                continue;
            size_t receiver = allocation.size();
            for (size_t i = 0ul; i < allocation.size(); ++i) {
                if (allocation[i] >= upper[i])
                    continue;
                if (receiver == allocation.size() ||
                    allocation[i] < allocation[receiver] ||
                    (allocation[i] == allocation[receiver] &&
                     signatures[i] < signatures[receiver]))
                    receiver = i;
            }
            if (receiver == allocation.size())
                break;
            allocation[donor]--;
            allocation[receiver]++;
            projectionChanged = true;
        }
        for (size_t receiver = 0ul; receiver < allocation.size(); ++receiver) {
            if (allocation[receiver] >= lower[receiver])
                continue;
            size_t donor = allocation.size();
            for (size_t i = 0ul; i < allocation.size(); ++i) {
                if (allocation[i] <= lower[i])
                    continue;
                if (donor == allocation.size() ||
                    allocation[i] > allocation[donor] ||
                    (allocation[i] == allocation[donor] &&
                     signatures[i] < signatures[donor]))
                    donor = i;
            }
            if (donor == allocation.size())
                break;
            allocation[donor]--;
            allocation[receiver]++;
            projectionChanged = true;
        }
    }

    // The original Fixed counterfactual is the predicted-cost reference.
    // Information-poor batches return to that exact allocation whenever it
    // satisfies the concentration box; otherwise the constrained quality
    // projection is used without changing the total population.
    for (size_t i = 0ul; i < plan.groups.size(); ++i)
        plan.fixed_predicted_cost += plan.groups[i].estimates.cost *
                                     static_cast<double>(fixedAllocation[i]);
    bool fixedFeasible = true;
    for (size_t i = 0ul; i < fixedAllocation.size(); ++i)
        fixedFeasible = fixedFeasible &&
            fixedAllocation[i] >= lower[i] && fixedAllocation[i] <= upper[i];
    if (!informed && fixedFeasible)
        allocation = fixedAllocation;
    if (informed) {
        bool changed = true;
        while (changed) {
            changed = false;
            double bestGain = 0.0;
            size_t bestDonor = 0ul, bestReceiver = 0ul;
            for (size_t donor = 0ul; donor < allocation.size(); ++donor) {
                if (allocation[donor] <= lower[donor])
                    continue;
                for (size_t receiver = 0ul; receiver < allocation.size(); ++receiver) {
                    if (donor == receiver || allocation[receiver] >= upper[receiver])
                        continue;
                    double cost = 0.0;
                    for (size_t i = 0ul; i < allocation.size(); ++i) {
                        const long adjusted = static_cast<long>(allocation[i]) +
                            (i == receiver ? 1l : 0l) - (i == donor ? 1l : 0l);
                        cost += plan.groups[i].estimates.cost *
                                static_cast<double>(adjusted);
                    }
                    if (cost > plan.fixed_predicted_cost + 1e-9)
                        continue;
                    const double before = fb_ssta_objective(
                        plan.groups[donor], allocation[donor], config.diversity_lambda) +
                        fb_ssta_objective(plan.groups[receiver], allocation[receiver],
                                          config.diversity_lambda);
                    const double after = fb_ssta_objective(
                        plan.groups[donor], allocation[donor] - 1ul,
                        config.diversity_lambda) +
                        fb_ssta_objective(plan.groups[receiver], allocation[receiver] + 1ul,
                                          config.diversity_lambda);
                    const double gain = before - after;
                    if (gain > bestGain + 1e-15 ||
                        (std::fabs(gain - bestGain) <= 1e-15 && gain > 0.0 &&
                         std::make_pair(plan.groups[receiver].signature,
                                        plan.groups[donor].signature) <
                         std::make_pair(plan.groups[bestReceiver].signature,
                                        plan.groups[bestDonor].signature))) {
                        bestGain = gain;
                        bestDonor = donor;
                        bestReceiver = receiver;
                    }
                }
            }
            if (bestGain > 0.0) {
                allocation[bestDonor]--;
                allocation[bestReceiver]++;
                changed = true;
            }
        }
    }

    size_t assigned = 0ul;
    double squareShare = 0.0;
    for (size_t i = 0ul; i < plan.groups.size(); ++i) {
        plan.groups[i].children = allocation[i];
        assigned += allocation[i];
        plan.predicted_cost += plan.groups[i].estimates.cost *
                               static_cast<double>(allocation[i]);
        const double share = static_cast<double>(allocation[i]) /
                             static_cast<double>(totalChildren);
        squareShare += share * share;
    }
    if (assigned != totalChildren ||
        plan.predicted_cost > plan.fixed_predicted_cost + 1e-9) {
        plan.fallback_reason = "constraint_fallback";
        if (!fixedFeasible)
            return plan;
        allocation = fixedAllocation;
        plan.predicted_cost = 0.0;
        squareShare = 0.0;
        for (size_t i = 0ul; i < plan.groups.size(); ++i) {
            plan.groups[i].children = allocation[i];
            plan.predicted_cost += plan.groups[i].estimates.cost *
                                   static_cast<double>(allocation[i]);
            const double share = static_cast<double>(allocation[i]) /
                                 static_cast<double>(totalChildren);
            squareShare += share * share;
        }
    }
    plan.allocation_ess = squareShare > 0.0 ? 1.0 / squareShare : 0.0;
    plan.total_children = totalChildren;
    plan.valid = true;
    return plan;
}

} // namespace fig

#endif // SEQUENTIALSTATEALLOCATION_H
