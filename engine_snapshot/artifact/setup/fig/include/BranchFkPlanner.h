//==============================================================================
// Planner interface for branch-preserving SFE experiments.
// It deliberately refuses proxy inputs that have not been validated as a
// branch-lineage influence measure. Observation mode alone cannot enable it.
//==============================================================================

#ifndef BRANCHFKPLANNER_H
#define BRANCHFKPLANNER_H

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace fig
{

struct BranchFkLevelObservation
{
    size_t level;
    size_t trials;
    size_t successes;
    size_t event_transitions;
    bool influence_validated;
    double influence;
    double unit_event_cost;
    size_t baseline_population;

    BranchFkLevelObservation() : level(0ul), trials(0ul), successes(0ul),
        event_transitions(0ul), influence_validated(false), influence(0.0),
        unit_event_cost(0.0), baseline_population(0ul) {}

    BranchFkLevelObservation(size_t level_id, size_t observed_trials,
                             size_t observed_successes, size_t events,
                             double branch_influence, size_t baseline) :
        level(level_id), trials(observed_trials), successes(observed_successes),
        event_transitions(events), influence_validated(true),
        influence(branch_influence),
        unit_event_cost(observed_trials > 0ul
            ? static_cast<double>(events) / observed_trials : 0.0),
        baseline_population(baseline) {}
};

struct BranchFkOstreConfig
{
    size_t min_population;
    size_t max_population;
    size_t min_successes;
    double budget;
    BranchFkOstreConfig() : min_population(1ul), max_population(1000000ul),
        min_successes(1ul), budget(0.0) {}
};

struct BranchFkPlan
{
    bool valid;
    std::string reason;
    std::string influence_source_required;
    std::vector<size_t> populations;
    double predicted_cost;

    BranchFkPlan() : valid(false), reason("not_planned"),
        influence_source_required("validated_branch_lineage"), populations(),
        predicted_cost(0.0) {}
};

inline BranchFkPlan
branch_fk_plan_levels(const std::vector<BranchFkLevelObservation>& observations,
                      double budget,
                      size_t min_population)
{
    BranchFkPlan plan;
    if (observations.empty() || !(budget > 0.0) || min_population == 0ul) {
        plan.reason = "invalid_configuration";
        return plan;
    }
    double minimum_cost = 0.0;
    for (const auto& observation: observations) {
        if (!observation.influence_validated) {
            plan.reason = "unvalidated_influence";
            return plan;
        }
        if (!std::isfinite(observation.influence) || observation.influence < 0.0 ||
            !std::isfinite(observation.unit_event_cost) ||
            !(observation.unit_event_cost > 0.0)) {
            plan.reason = "invalid_observation";
            return plan;
        }
        minimum_cost += observation.unit_event_cost * min_population;
    }
    if (minimum_cost > budget) {
        plan.reason = "minimum_population_exceeds_budget";
        return plan;
    }

    plan.populations.assign(observations.size(), min_population);
    plan.predicted_cost = minimum_cost;
    while (true) {
        size_t best = observations.size();
        double best_gain = -1.0;
        for (size_t level = 0ul; level < observations.size(); ++level) {
            const double cost = observations[level].unit_event_cost;
            if (plan.predicted_cost + cost > budget + 1.0e-12)
                continue;
            const double n = static_cast<double>(plan.populations[level]);
            const double gain = observations[level].influence *
                (1.0 / n - 1.0 / (n + 1.0)) / cost;
            if (gain > best_gain) {
                best_gain = gain;
                best = level;
            }
        }
        if (best == observations.size())
            break;
        ++plan.populations[best];
        plan.predicted_cost += observations[best].unit_event_cost;
    }
    plan.valid = true;
    plan.reason = "ok";
    return plan;
}

inline BranchFkPlan
branch_fk_ostre_plan(const std::vector<BranchFkLevelObservation>& observations,
                     const BranchFkOstreConfig& config)
{
    BranchFkPlan plan;
    if (observations.empty() || config.min_population == 0ul ||
        config.max_population < config.min_population || !(config.budget > 0.0)) {
        plan.reason = "invalid_configuration";
        return plan;
    }
    double lower_cost = 0.0;
    plan.populations.assign(observations.size(), config.min_population);
    for (size_t level = 0ul; level < observations.size(); ++level) {
        const auto& observation = observations[level];
        if (!observation.influence_validated) {
            plan.populations.clear();
            plan.reason = "unvalidated_influence";
            return plan;
        }
        if (observation.trials == 0ul || observation.successes == 0ul ||
            !(observation.unit_event_cost > 0.0) ||
            !std::isfinite(observation.influence) || observation.influence < 0.0) {
            plan.populations.clear();
            plan.reason = config.min_successes > 0ul
                ? "survival_floor_infeasible" : "invalid_observation";
            return plan;
        }
        const double q = static_cast<double>(observation.successes) /
                         observation.trials;
        const size_t survival_lower = static_cast<size_t>(std::ceil(
            static_cast<double>(config.min_successes) / q));
        if (survival_lower > config.max_population) {
            plan.populations.clear();
            plan.reason = "survival_floor_infeasible";
            return plan;
        }
        plan.populations[level] = std::max(config.min_population,
                                           survival_lower);
        lower_cost += observation.unit_event_cost * plan.populations[level];
    }
    if (lower_cost > config.budget + 1.0e-12) {
        plan.populations.clear();
        plan.reason = "survival_floor_infeasible";
        return plan;
    }
    plan.predicted_cost = lower_cost;
    while (true) {
        size_t best = observations.size();
        double best_gain = -1.0;
        for (size_t level = 0ul; level < observations.size(); ++level) {
            if (plan.populations[level] >= config.max_population)
                continue;
            const double cost = observations[level].unit_event_cost;
            if (plan.predicted_cost + cost > config.budget + 1.0e-12)
                continue;
            const double n = static_cast<double>(plan.populations[level]);
            const double gain = observations[level].influence *
                (1.0 / n - 1.0 / (n + 1.0)) / cost;
            if (gain > best_gain) { best_gain = gain; best = level; }
        }
        if (best == observations.size())
            break;
        ++plan.populations[best];
        plan.predicted_cost += observations[best].unit_event_cost;
    }
    plan.valid = true;
    plan.reason = "ok";
    return plan;
}

} // namespace fig

#endif // BRANCHFKPLANNER_H
