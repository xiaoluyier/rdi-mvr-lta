// Rolling, stage-wise optimization of level populations for modal SFE.
#ifndef ROLLINGLEVELPOPULATION_H
#define ROLLINGLEVELPOPULATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace fig
{

struct RollingLevelPopulationConfig
{
    size_t min_population;
    size_t max_population;
    double extinction_target;
    double epsilon;
    double cost_uncertainty;
    double max_stage_change;

    RollingLevelPopulationConfig() :
        min_population(24ul),
        max_population(360ul),
        extinction_target(0.0025),
        epsilon(0.05),
        cost_uncertainty(1.0),
        max_stage_change(0.50)
    {}
};

struct RollingLevelObservation
{
    size_t trials;
    size_t successes;
    size_t retained;
    size_t event_transitions;

    RollingLevelObservation() :
        trials(0ul), successes(0ul), retained(0ul), event_transitions(0ul)
    {}
};

struct RollingLevelPopulationPlan
{
    bool valid;
    bool fallback;
    std::string reason;
    std::vector<size_t> populations;
    double predicted_cost;
    double proxy_value;
    double budget;

    RollingLevelPopulationPlan() :
        valid(false), fallback(false), reason(), populations(),
        predicted_cost(0.0), proxy_value(0.0), budget(0.0)
    {}
};

inline double
rolling_probability(const size_t count, const size_t trials)
{
    if (trials == 0ul)
        return 0.0;
    return (static_cast<double>(count) + 0.5) /
           (static_cast<double>(trials) + 1.0);
}

inline double
rolling_cost(const RollingLevelObservation& observation,
             const RollingLevelPopulationConfig& config)
{
    if (observation.trials == 0ul)
        return 0.0;
    const double mean = static_cast<double>(observation.event_transitions) /
                        static_cast<double>(observation.trials);
    if (!std::isfinite(mean) || mean <= 0.0)
        return 0.0;
    const double uncertainty =
        mean / std::sqrt(static_cast<double>(observation.trials));
    const double robust = mean + config.cost_uncertainty * uncertainty;
    return std::max(1.0e-12, std::min(1.0e12, robust));
}

inline double
rolling_proxy_value(const std::vector<size_t>& populations,
                    const std::vector<double>& q)
{
    double value = 0.0;
    for (size_t level = 0ul; level < populations.size(); ++level) {
        const double probability = std::max(1.0e-12, std::min(1.0 - 1.0e-12,
                                                               q[level]));
        value += (1.0 - probability) /
                 (static_cast<double>(populations[level]) * probability);
    }
    return value;
}

inline RollingLevelPopulationPlan
rolling_plan_levels(const std::vector<RollingLevelObservation>& observations,
                    const std::vector<size_t>& fallback_populations,
                    const double budget,
                    const RollingLevelPopulationConfig& config,
                    const std::vector<size_t>* previous_populations = nullptr)
{
    RollingLevelPopulationPlan plan;
    plan.budget = std::max(0.0, budget);
    const size_t levelCount = observations.size();
    if (levelCount == 0ul || fallback_populations.size() != levelCount) {
        plan.fallback = true;
        plan.reason = "invalid_level_inputs";
        plan.populations = fallback_populations;
        return plan;
    }
    if (config.min_population == 0ul ||
        config.max_population < config.min_population ||
        config.extinction_target <= 0.0 || config.extinction_target >= 1.0 ||
        config.epsilon < 0.0 || config.max_stage_change < 0.0) {
        plan.fallback = true;
        plan.reason = "invalid_configuration";
        plan.populations = fallback_populations;
        return plan;
    }

    std::vector<double> q(levelCount, 0.0);
    std::vector<double> rho(levelCount, 0.0);
    std::vector<double> cost(levelCount, 0.0);
    double minObservedQ = 1.0;
    double minObservedRho = 1.0;
    std::vector<double> observedCosts;
    for (size_t level = 0ul; level < levelCount; ++level) {
        const RollingLevelObservation& item = observations[level];
        if (item.trials > 0ul) {
            q[level] = rolling_probability(item.successes, item.trials);
            rho[level] = rolling_probability(item.retained, item.trials);
            cost[level] = rolling_cost(item, config);
            minObservedQ = std::min(minObservedQ, q[level]);
            minObservedRho = std::min(minObservedRho, rho[level]);
            if (cost[level] > 0.0)
                observedCosts.push_back(cost[level]);
        }
    }
    if (observedCosts.empty()) {
        plan.fallback = true;
        plan.reason = "insufficient_observations";
        plan.populations = fallback_populations;
        return plan;
    }
    std::sort(observedCosts.begin(), observedCosts.end());
    const double defaultCost = observedCosts[observedCosts.size() / 2ul];
    for (size_t level = 0ul; level < levelCount; ++level) {
        if (observations[level].trials == 0ul) {
            q[level] = minObservedQ;
            rho[level] = minObservedRho;
            cost[level] = defaultCost;
        }
        q[level] = std::max(1.0e-12, std::min(1.0 - 1.0e-12, q[level]));
        rho[level] = std::max(1.0e-12, std::min(1.0 - 1.0e-12, rho[level]));
        cost[level] = std::max(1.0e-12, cost[level]);
    }

    std::vector<size_t> lower(levelCount, config.min_population);
    std::vector<size_t> upper(levelCount, config.max_population);
    for (size_t level = 0ul; level < levelCount; ++level) {
        // The nominal extinction floor (1-rho)^N <= delta only applies when
        // there is positive retention evidence: with rho == 0 every
        // trajectory fails independently and no N can prevent extinction,
        // and with zero trials there is no evidence to plan from.
        const bool retentionEvidence = observations[level].trials > 0ul &&
                                        observations[level].retained > 0ul;
        if (retentionEvidence) {
            const double riskRaw = std::ceil(
                std::log(config.extinction_target) / std::log1p(-rho[level]));
            const size_t riskMinimum =
                (!std::isfinite(riskRaw) || riskRaw <= 0.0)
                ? config.max_population
                : static_cast<size_t>(riskRaw);
            lower[level] = std::max(lower[level],
                                    std::min(config.max_population, riskMinimum));
        }
        if (previous_populations != nullptr &&
            previous_populations->size() == levelCount) {
            const double previous =
                static_cast<double>((*previous_populations)[level]);
            const size_t changeLower = static_cast<size_t>(std::floor(
                previous * std::max(0.0, 1.0 - config.max_stage_change)));
            const size_t changeUpper = static_cast<size_t>(std::ceil(
                previous * (1.0 + config.max_stage_change)));
            // The stage-change limit is a soft bound: when it conflicts with
            // the extinction/size bounds, relax it instead of freezing the
            // plan on the previous populations forever.
            const size_t windowLower = std::max(lower[level],
                std::min(config.max_population, changeLower));
            const size_t windowUpper = std::min(upper[level],
                std::max(config.min_population, changeUpper));
            if (windowLower <= windowUpper) {
                lower[level] = windowLower;
                upper[level] = windowUpper;
            }
        }
        if (lower[level] > upper[level]) {
            plan.fallback = true;
            plan.reason = "bounds_infeasible";
            plan.populations = fallback_populations;
            return plan;
        }
    }

    double lowerCost = 0.0;
    for (size_t level = 0ul; level < levelCount; ++level)
        lowerCost += cost[level] * static_cast<double>(lower[level]);
    if (!std::isfinite(plan.budget) || plan.budget + 1.0e-9 < lowerCost) {
        plan.fallback = true;
        plan.reason = "budget_infeasible";
        plan.populations = fallback_populations;
        return plan;
    }

    std::vector<size_t> best = lower;
    double bestCost = lowerCost;
    while (true) {
        size_t selected = levelCount;
        double selectedGain = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            if (best[level] >= upper[level])
                continue;
            const double n = static_cast<double>(best[level]);
            const double gain = (1.0 - q[level]) /
                (q[level] * n * (n + 1.0));
            const double gainPerCost = gain / cost[level];
            if (bestCost + cost[level] <= plan.budget + 1.0e-9 &&
                (gainPerCost > selectedGain + 1.0e-15 ||
                 (std::fabs(gainPerCost - selectedGain) <= 1.0e-15 &&
                  level < selected))) {
                selected = level;
                selectedGain = gainPerCost;
            }
        }
        if (selected == levelCount)
            break;
        ++best[selected];
        bestCost += cost[selected];
    }
    const double bestProxy = rolling_proxy_value(best, q);

    // Cost-compression pass: the same proxy value within epsilon is enough.
    const double target = bestProxy * (1.0 + config.epsilon);
    std::vector<size_t> compressed = lower;
    double compressedCost = lowerCost;
    while (rolling_proxy_value(compressed, q) > target + 1.0e-15) {
        size_t selected = levelCount;
        double selectedGainPerCost = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            if (compressed[level] >= upper[level])
                continue;
            const double n = static_cast<double>(compressed[level]);
            const double gain = (1.0 - q[level]) /
                (q[level] * n * (n + 1.0));
            const double gainPerCost = gain / cost[level];
            if (gainPerCost > selectedGainPerCost + 1.0e-15 ||
                (std::fabs(gainPerCost - selectedGainPerCost) <= 1.0e-15 &&
                 level < selected)) {
                selected = level;
                selectedGainPerCost = gainPerCost;
            }
        }
        if (selected == levelCount)
            break;
        ++compressed[selected];
        compressedCost += cost[selected];
    }
    if (rolling_proxy_value(compressed, q) > target + 1.0e-12 ||
        compressedCost > plan.budget + 1.0e-9) {
        plan.fallback = true;
        plan.reason = "proxy_constraint_infeasible";
        plan.populations = fallback_populations;
        return plan;
    }
    plan.valid = true;
    plan.populations = compressed;
    plan.predicted_cost = compressedCost;
    plan.proxy_value = rolling_proxy_value(compressed, q);
    plan.reason = "none";
    return plan;
}

} // namespace fig

#endif // ROLLINGLEVELPOPULATION_H
