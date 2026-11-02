// Variance-Cost Sequential Trajectory Planning (VCSTP).
#ifndef VCSTP_H
#define VCSTP_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace fig
{

struct VcstpConfig
{
    size_t min_population;
    size_t max_population;
    size_t min_samples;
    double delta_path;
    double epsilon;
    double kappa;
    double alpha_p;
    double beta_p;
    double alpha_rho;
    double beta_rho;
    bool protect_fixed_variance;
    bool require_strict_cost_reduction;
    bool minimize_event_cost_under_fixed_variance;
    double variance_tolerance;
    size_t replan_interval;

    VcstpConfig() :
        min_population(8ul), max_population(360ul), min_samples(48ul),
        delta_path(0.0025), epsilon(0.05), kappa(0.20),
        alpha_p(0.5), beta_p(0.5), alpha_rho(0.5), beta_rho(0.5),
        protect_fixed_variance(false), require_strict_cost_reduction(false),
        minimize_event_cost_under_fixed_variance(false),
        variance_tolerance(0.0), replan_interval(1ul)
    {}
};

struct VcstpObservation
{
    size_t trials;
    size_t successes;
    size_t retained;
    size_t event_transitions;

    VcstpObservation() :
        trials(0ul), successes(0ul), retained(0ul), event_transitions(0ul) {}
};

struct VcstpProblem
{
    VcstpConfig config;
    std::vector<VcstpObservation> observations;
    std::vector<size_t> fixed_populations;
    std::vector<size_t> previous_populations;
    std::vector<std::vector<size_t> > paths;
    std::vector<size_t> path_counts;
};

struct VcstpBounds
{
    bool valid;
    std::string reason;
    std::vector<size_t> lower;
    std::vector<size_t> upper;

    VcstpBounds() : valid(false), reason(), lower(), upper() {}
};

struct VcstpRiskTarget
{
    double fixed_risk;
    double effective_delta;
    bool absolute_target_met;

    VcstpRiskTarget() :
        fixed_risk(1.0), effective_delta(1.0), absolute_target_met(false)
    {}
};

struct VcstpPlan
{
    bool valid;
    bool fallback;
    bool absolute_risk_target_met;
    bool variance_guard_applied;
    bool cost_reduced;
    bool event_cost_reduced;
    std::string reason;
    std::vector<size_t> populations;
    std::vector<size_t> stage1_populations;
    std::vector<double> p;
    std::vector<double> rho;
    std::vector<double> gradient;
    double stage1_variance;
    double final_variance;
    double fixed_variance;
    double exact_single_path_variance;
    double fixed_extinction_risk;
    double extinction_risk;
    double effective_delta;
    double fixed_predicted_event_cost;
    double final_predicted_event_cost;
    size_t fixed_cost;
    size_t stage1_cost;
    size_t final_cost;
    size_t frontier_states;

    VcstpPlan() :
        valid(false), fallback(false), absolute_risk_target_met(false),
        variance_guard_applied(false), cost_reduced(false),
        event_cost_reduced(false),
        reason(), populations(), stage1_populations(), p(), rho(), gradient(),
        stage1_variance(std::numeric_limits<double>::infinity()),
        final_variance(std::numeric_limits<double>::infinity()),
        fixed_variance(std::numeric_limits<double>::infinity()),
        exact_single_path_variance(std::numeric_limits<double>::quiet_NaN()),
        fixed_extinction_risk(1.0), extinction_risk(1.0),
        effective_delta(1.0),
        fixed_predicted_event_cost(std::numeric_limits<double>::infinity()),
        final_predicted_event_cost(std::numeric_limits<double>::infinity()),
        fixed_cost(0ul), stage1_cost(0ul),
        final_cost(0ul), frontier_states(0ul)
    {}
};

inline double
vcstp_smoothed_probability(const size_t count, const size_t trials,
                           const double alpha, const double beta)
{
    const double denominator = static_cast<double>(trials) + alpha + beta;
    if (!(alpha > 0.0) || !(beta > 0.0) || !(denominator > 0.0) ||
        count > trials)
        return std::numeric_limits<double>::quiet_NaN();
    return (static_cast<double>(count) + alpha) / denominator;
}

inline std::vector<size_t>
vcstp_active_levels(const std::vector<std::vector<size_t> >& paths)
{
    std::vector<size_t> active;
    for (size_t path = 0ul; path < paths.size(); ++path)
        active.insert(active.end(), paths[path].begin(), paths[path].end());
    std::sort(active.begin(), active.end());
    active.erase(std::unique(active.begin(), active.end()), active.end());
    return active;
}

inline std::vector<double>
vcstp_smoothed_successes(const VcstpProblem& problem)
{
    std::vector<double> result(problem.observations.size(), 0.5);
    for (size_t level = 0ul; level < problem.observations.size(); ++level) {
        const VcstpObservation& item(problem.observations[level]);
        result[level] = vcstp_smoothed_probability(
            item.successes, item.trials,
            problem.config.alpha_p, problem.config.beta_p);
    }
    return result;
}

inline std::vector<double>
vcstp_smoothed_retentions(const VcstpProblem& problem)
{
    std::vector<double> result(problem.observations.size(), 0.5);
    for (size_t level = 0ul; level < problem.observations.size(); ++level) {
        const VcstpObservation& item(problem.observations[level]);
        result[level] = vcstp_smoothed_probability(
            item.retained, item.trials,
            problem.config.alpha_rho, problem.config.beta_rho);
    }
    return result;
}

inline std::vector<double>
vcstp_gradient(const std::vector<std::vector<size_t> >& paths,
               const std::vector<double>& p)
{
    std::vector<double> gradient(p.size(), 0.0);
    for (size_t path = 0ul; path < paths.size(); ++path) {
        for (size_t position = 0ul; position < paths[path].size(); ++position) {
            const size_t level = paths[path][position];
            if (level >= p.size())
                continue;
            double term = 1.0;
            for (size_t other = 0ul; other < paths[path].size(); ++other) {
                if (other == position)
                    continue;
                const size_t otherLevel = paths[path][other];
                if (otherLevel >= p.size()) {
                    term = 0.0;
                    break;
                }
                term *= p[otherLevel];
            }
            gradient[level] += term;
        }
    }
    return gradient;
}

inline std::vector<double>
vcstp_gradient_weighted(const std::vector<std::vector<size_t> >& paths,
                        const std::vector<size_t>& pathCounts,
                        const std::vector<double>& p)
{
    if (pathCounts.size() != paths.size())
        return vcstp_gradient(paths, p);
    size_t total = 0ul;
    for (size_t path = 0ul; path < pathCounts.size(); ++path)
        total += pathCounts[path];
    if (total == 0ul)
        return vcstp_gradient(paths, p);
    std::vector<double> gradient(p.size(), 0.0);
    for (size_t path = 0ul; path < paths.size(); ++path) {
        const double weight = static_cast<double>(pathCounts[path]) /
                              static_cast<double>(total);
        for (size_t position = 0ul; position < paths[path].size(); ++position) {
            const size_t level = paths[path][position];
            if (level >= p.size())
                continue;
            double term = weight;
            for (size_t other = 0ul; other < paths[path].size(); ++other) {
                if (other == position)
                    continue;
                const size_t otherLevel = paths[path][other];
                if (otherLevel >= p.size()) {
                    term = 0.0;
                    break;
                }
                term *= p[otherLevel];
            }
            gradient[level] += term;
        }
    }
    return gradient;
}

inline std::vector<double>
vcstp_path_access_frequencies(const std::vector<std::vector<size_t> >& paths,
                              const std::vector<size_t>& pathCounts,
                              const size_t levels)
{
    std::vector<double> result(levels, 0.0);
    size_t total = 0ul;
    if (pathCounts.size() == paths.size())
        for (size_t path = 0ul; path < pathCounts.size(); ++path)
            total += pathCounts[path];
    const bool weighted = total > 0ul;
    if (!weighted)
        total = paths.size();
    if (total == 0ul)
        return result;
    for (size_t path = 0ul; path < paths.size(); ++path) {
        const double weight = static_cast<double>(
            weighted ? pathCounts[path] : 1ul) / static_cast<double>(total);
        std::vector<size_t> unique(paths[path]);
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        for (size_t pos = 0ul; pos < unique.size(); ++pos)
            if (unique[pos] < levels)
                result[unique[pos]] += weight;
    }
    return result;
}

inline std::vector<double>
vcstp_event_costs(const VcstpProblem& problem)
{
    std::vector<double> result(problem.observations.size(), 1.0);
    for (size_t level = 0ul; level < problem.observations.size(); ++level) {
        const VcstpObservation& item(problem.observations[level]);
        if (item.trials > 0ul)
            result[level] = std::max(
                1.0, static_cast<double>(item.event_transitions) /
                     static_cast<double>(item.trials));
    }
    return result;
}

inline double
vcstp_predicted_event_cost(const std::vector<size_t>& populations,
                           const std::vector<double>& access,
                           const std::vector<double>& eventCosts,
                           const std::vector<size_t>& active)
{
    double result = 0.0;
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= populations.size() || level >= access.size() ||
            level >= eventCosts.size())
            return std::numeric_limits<double>::infinity();
        result += access[level] * eventCosts[level] *
                  static_cast<double>(populations[level]);
    }
    return result;
}

inline size_t
vcstp_path_cost(const std::vector<size_t>& populations,
                const std::vector<size_t>& active)
{
    size_t result = 0ul;
    for (size_t i = 0ul; i < active.size(); ++i)
        if (active[i] < populations.size())
            result += populations[active[i]];
    return result;
}

inline double
vcstp_delta_variance(const std::vector<size_t>& populations,
                     const std::vector<double>& p,
                     const std::vector<double>& gradient,
                     const std::vector<size_t>& active)
{
    double result = 0.0;
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= populations.size() || level >= p.size() ||
            level >= gradient.size() || populations[level] == 0ul)
            return std::numeric_limits<double>::infinity();
        result += gradient[level] * gradient[level] * p[level] *
                  (1.0 - p[level]) /
                  static_cast<double>(populations[level]);
    }
    return result;
}

inline double
vcstp_exact_single_path_variance(const std::vector<size_t>& populations,
                                 const std::vector<double>& p,
                                 const std::vector<size_t>& path)
{
    double secondMoment = 1.0;
    double meanSquared = 1.0;
    for (size_t i = 0ul; i < path.size(); ++i) {
        const size_t level = path[i];
        if (level >= populations.size() || level >= p.size() ||
            populations[level] == 0ul)
            return std::numeric_limits<double>::infinity();
        const double variance = p[level] * (1.0 - p[level]) /
                                static_cast<double>(populations[level]);
        secondMoment *= p[level] * p[level] + variance;
        meanSquared *= p[level] * p[level];
    }
    return std::max(0.0, secondMoment - meanSquared);
}

inline double
vcstp_log_survival_contribution(const double rho, const size_t population)
{
    if (!(rho > 0.0) || population == 0ul)
        return -std::numeric_limits<double>::infinity();
    if (rho >= 1.0)
        return 0.0;
    const double logExtinction =
        static_cast<double>(population) * std::log1p(-rho);
    const double survival = -std::expm1(logExtinction);
    if (!(survival > 0.0))
        return -std::numeric_limits<double>::infinity();
    return std::log(survival);
}

inline double
vcstp_extinction_risk(const std::vector<size_t>& populations,
                      const std::vector<size_t>& active,
                      const std::vector<double>& rho)
{
    double logSurvival = 0.0;
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= populations.size() || level >= rho.size())
            return 1.0;
        const double contribution = vcstp_log_survival_contribution(
            rho[level], populations[level]);
        if (!std::isfinite(contribution))
            return 1.0;
        logSurvival += contribution;
    }
    const double risk = -std::expm1(std::min(0.0, logSurvival));
    return std::max(0.0, std::min(1.0, risk));
}

inline double
vcstp_weighted_path_extinction_risk(
    const std::vector<size_t>& populations,
    const std::vector<std::vector<size_t> >& paths,
    const std::vector<size_t>& pathCounts,
    const std::vector<double>& rho)
{
    if (paths.empty())
        return 1.0;
    size_t total = 0ul;
    if (pathCounts.size() == paths.size())
        for (size_t path = 0ul; path < pathCounts.size(); ++path)
            total += pathCounts[path];
    const bool weighted = total > 0ul;
    if (!weighted)
        total = paths.size();
    double result = 0.0;
    for (size_t path = 0ul; path < paths.size(); ++path) {
        std::vector<size_t> active(paths[path]);
        std::sort(active.begin(), active.end());
        active.erase(std::unique(active.begin(), active.end()), active.end());
        const double weight = static_cast<double>(
            weighted ? pathCounts[path] : 1ul) / static_cast<double>(total);
        result += weight * vcstp_extinction_risk(populations, active, rho);
    }
    return std::max(0.0, std::min(1.0, result));
}

inline VcstpRiskTarget
vcstp_effective_risk_target(const std::vector<size_t>& fixedPopulations,
                            const std::vector<size_t>& active,
                            const std::vector<double>& rho,
                            const double requestedDelta)
{
    VcstpRiskTarget result;
    result.fixed_risk = vcstp_extinction_risk(fixedPopulations, active, rho);
    result.absolute_target_met = result.fixed_risk <= requestedDelta + 1.0e-15;
    result.effective_delta = result.absolute_target_met
                           ? requestedDelta : result.fixed_risk;
    return result;
}

inline bool
vcstp_lexicographically_less(const std::vector<size_t>& lhs,
                             const std::vector<size_t>& rhs,
                             const std::vector<size_t>& active)
{
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (lhs[level] != rhs[level])
            return lhs[level] < rhs[level];
    }
    return false;
}

inline size_t
vcstp_max_population(const std::vector<size_t>& values,
                     const std::vector<size_t>& active)
{
    size_t result = 0ul;
    for (size_t i = 0ul; i < active.size(); ++i)
        result = std::max(result, values[active[i]]);
    return result;
}

inline bool
vcstp_population_tie_better(const std::vector<size_t>& lhs,
                            const std::vector<size_t>& rhs,
                            const std::vector<size_t>& active)
{
    const size_t lhsCost = vcstp_path_cost(lhs, active);
    const size_t rhsCost = vcstp_path_cost(rhs, active);
    if (lhsCost != rhsCost)
        return lhsCost < rhsCost;
    const size_t lhsMax = vcstp_max_population(lhs, active);
    const size_t rhsMax = vcstp_max_population(rhs, active);
    if (lhsMax != rhsMax)
        return lhsMax < rhsMax;
    return vcstp_lexicographically_less(lhs, rhs, active);
}

inline bool
vcstp_stage1_better(const std::vector<size_t>& candidate,
                    const double candidateVariance,
                    const std::vector<size_t>& current,
                    const double currentVariance,
                    const std::vector<size_t>& active)
{
    const double tolerance = 1.0e-15 *
        std::max(1.0, std::max(std::fabs(candidateVariance),
                               std::fabs(currentVariance)));
    if (candidateVariance < currentVariance - tolerance)
        return true;
    if (std::fabs(candidateVariance - currentVariance) <= tolerance)
        return vcstp_population_tie_better(candidate, current, active);
    return false;
}

inline bool
vcstp_stage2_better(const std::vector<size_t>& candidate,
                    const std::vector<size_t>& current,
                    const std::vector<size_t>& active)
{
    return vcstp_population_tie_better(candidate, current, active);
}

inline bool
vcstp_config_valid(const VcstpConfig& config)
{
    return config.min_population > 0ul &&
           config.max_population >= config.min_population &&
           config.min_samples > 0ul &&
           config.delta_path > 0.0 && config.delta_path < 1.0 &&
           config.epsilon >= 0.0 && std::isfinite(config.epsilon) &&
           config.kappa >= 0.0 && config.kappa <= 1.0 &&
           config.alpha_p > 0.0 && config.beta_p > 0.0 &&
           config.alpha_rho > 0.0 && config.beta_rho > 0.0 &&
           config.variance_tolerance >= 0.0 &&
           std::isfinite(config.variance_tolerance) &&
           config.replan_interval > 0ul;
}

inline VcstpBounds
vcstp_bounds(const VcstpProblem& problem,
             const std::vector<size_t>& active);

inline VcstpBounds
vcstp_v3_bounds(const VcstpProblem& problem,
                const std::vector<size_t>& active)
{
    VcstpBounds result = vcstp_bounds(problem, active);
    if (result.valid) {
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            result.lower[level] = std::min(
                result.lower[level], problem.fixed_populations[level]);
            result.upper[level] = std::max(
                result.upper[level], problem.fixed_populations[level]);
        }
        return result;
    }
    const size_t levels = problem.observations.size();
    result = VcstpBounds();
    result.lower.assign(levels, 0ul);
    result.upper.assign(levels, 0ul);
    if (!vcstp_config_valid(problem.config) ||
        problem.fixed_populations.size() != levels || active.empty()) {
        result.reason = "invalid_inputs";
        return result;
    }
    const bool hasPrevious = problem.previous_populations.size() == levels;
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= levels) {
            result.reason = "path_level_out_of_range";
            return result;
        }
        size_t lower = problem.config.min_population;
        size_t upper = problem.config.max_population;
        if (hasPrevious) {
            const double previous = static_cast<double>(
                problem.previous_populations[level]);
            lower = std::max(lower, static_cast<size_t>(std::ceil(
                std::max(0.0, 1.0 - problem.config.kappa) * previous)));
            upper = std::min(upper, static_cast<size_t>(std::floor(
                (1.0 + problem.config.kappa) * previous)));
        }
        // The fixed allocation is the quality reference and must remain an
        // explicit feasible candidate even after an adaptive previous batch.
        lower = std::min(lower, problem.fixed_populations[level]);
        upper = std::max(upper, problem.fixed_populations[level]);
        if (lower > upper) {
            result.reason = "stability_bounds_infeasible";
            return result;
        }
        result.lower[level] = lower;
        result.upper[level] = upper;
    }
    result.valid = true;
    result.reason = "none";
    return result;
}

inline bool
vcstp_v3_population_allowed(const VcstpProblem& problem,
                            const size_t level,
                            const size_t population)
{
    if (level >= problem.fixed_populations.size())
        return false;
    if (population == problem.fixed_populations[level])
        return true;
    if (population < problem.config.min_population ||
        population > problem.config.max_population)
        return false;
    if (problem.previous_populations.size() !=
        problem.fixed_populations.size())
        return true;
    const double previous = static_cast<double>(
        problem.previous_populations[level]);
    const size_t lower = std::max(problem.config.min_population,
        static_cast<size_t>(std::ceil(
            std::max(0.0, 1.0 - problem.config.kappa) * previous)));
    const size_t upper = std::min(problem.config.max_population,
        static_cast<size_t>(std::floor(
            (1.0 + problem.config.kappa) * previous)));
    return population >= lower && population <= upper;
}

inline VcstpBounds
vcstp_bounds(const VcstpProblem& problem,
             const std::vector<size_t>& active)
{
    VcstpBounds result;
    const size_t levels = problem.observations.size();
    result.lower.assign(levels, 0ul);
    result.upper.assign(levels, 0ul);
    if (!vcstp_config_valid(problem.config) ||
        problem.fixed_populations.size() != levels || active.empty()) {
        result.reason = "invalid_inputs";
        return result;
    }
    const bool hasPrevious = problem.previous_populations.size() == levels;
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= levels) {
            result.reason = "path_level_out_of_range";
            return result;
        }
        size_t lower = problem.config.min_population;
        size_t upper = problem.config.max_population;
        if (hasPrevious) {
            const double previous =
                static_cast<double>(problem.previous_populations[level]);
            const size_t changeLower = static_cast<size_t>(std::ceil(
                std::max(0.0, 1.0 - problem.config.kappa) * previous));
            const size_t changeUpper = static_cast<size_t>(std::floor(
                (1.0 + problem.config.kappa) * previous));
            lower = std::max(lower, changeLower);
            upper = std::min(upper, changeUpper);
        }
        if (lower > upper) {
            result.reason = "stability_bounds_infeasible";
            return result;
        }
        result.lower[level] = lower;
        result.upper[level] = upper;
    }
    if (vcstp_path_cost(result.lower, active) >
        vcstp_path_cost(problem.fixed_populations, active)) {
        result.reason = "minimum_cost_exceeds_fixed_budget";
        return result;
    }
    result.valid = true;
    result.reason = "none";
    return result;
}

inline std::vector<size_t>
vcstp_fallback_populations(const VcstpProblem& problem,
                           const std::vector<size_t>& active)
{
    std::vector<size_t> result = problem.previous_populations.size() ==
        problem.fixed_populations.size()
        ? problem.previous_populations : problem.fixed_populations;
    const size_t budget = vcstp_path_cost(problem.fixed_populations, active);
    if (result.size() != problem.fixed_populations.size())
        result = problem.fixed_populations;
    while (vcstp_path_cost(result, active) > budget) {
        size_t selected = active.size();
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            if (result[level] > problem.config.min_population &&
                (selected == active.size() ||
                 result[level] > result[active[selected]] ||
                 (result[level] == result[active[selected]] &&
                  level > active[selected])))
                selected = i;
        }
        if (selected == active.size())
            break;
        --result[active[selected]];
    }
    if (vcstp_path_cost(result, active) > budget)
        result = problem.fixed_populations;
    return result;
}

namespace vcstp_detail
{

struct State
{
    size_t cost;
    double variance;
    double log_survival;
    std::vector<size_t> populations;

    State() : cost(0ul), variance(0.0), log_survival(0.0), populations() {}
};

inline bool dominates(const State& lhs, const State& rhs,
                      const std::vector<size_t>& active)
{
    const double tolerance = 1.0e-15;
    const bool noWorse = lhs.cost <= rhs.cost &&
        lhs.variance <= rhs.variance + tolerance &&
        lhs.log_survival + tolerance >= rhs.log_survival;
    if (!noWorse)
        return false;
    const bool strict = lhs.cost < rhs.cost ||
        lhs.variance < rhs.variance - tolerance ||
        lhs.log_survival > rhs.log_survival + tolerance;
    if (!strict)
        return false;
    if (lhs.cost < rhs.cost)
        return true;
    // With equal cost, stage 2 deliberately ignores variance inside its
    // tolerance band.  Keep the population-vector tie winner even when it
    // has slightly larger variance, otherwise pruning can change the exact
    // deterministic second-stage solution.
    return vcstp_population_tie_better(
        lhs.populations, rhs.populations, active);
}

inline std::vector<State> pareto_prune(const std::vector<State>& candidates,
                                       const std::vector<size_t>& active)
{
    std::vector<State> frontier;
    for (size_t i = 0ul; i < candidates.size(); ++i) {
        bool rejected = false;
        for (size_t j = 0ul; j < frontier.size();) {
            if (dominates(frontier[j], candidates[i], active)) {
                rejected = true;
                break;
            }
            if (dominates(candidates[i], frontier[j], active)) {
                frontier.erase(frontier.begin() + static_cast<std::ptrdiff_t>(j));
            } else {
                ++j;
            }
        }
        if (!rejected)
            frontier.push_back(candidates[i]);
    }
    return frontier;
}

} // namespace vcstp_detail

inline VcstpPlan
vcstp_plan(const VcstpProblem& problem)
{
    VcstpPlan plan;
    const std::vector<size_t> active = vcstp_active_levels(problem.paths);
    plan.populations = vcstp_fallback_populations(problem, active);
    if (!vcstp_config_valid(problem.config) || active.empty() ||
        problem.observations.size() != problem.fixed_populations.size()) {
        plan.fallback = true;
        plan.reason = "invalid_inputs";
        return plan;
    }
    plan.p = vcstp_smoothed_successes(problem);
    plan.rho = vcstp_smoothed_retentions(problem);
    plan.gradient = vcstp_gradient(problem.paths, plan.p);
    plan.fixed_cost = vcstp_path_cost(problem.fixed_populations, active);
    plan.final_cost = vcstp_path_cost(plan.populations, active);
    plan.fixed_extinction_risk = vcstp_weighted_path_extinction_risk(
        problem.fixed_populations, problem.paths, problem.path_counts,
        plan.rho);
    plan.absolute_risk_target_met =
        plan.fixed_extinction_risk <= problem.config.delta_path + 1.0e-15;
    plan.effective_delta = plan.absolute_risk_target_met
                         ? problem.config.delta_path
                         : plan.fixed_extinction_risk;
    plan.extinction_risk = vcstp_extinction_risk(
        plan.populations, active, plan.rho);
    plan.final_variance = vcstp_delta_variance(
        plan.populations, plan.p, plan.gradient, active);
    plan.fixed_variance = vcstp_delta_variance(
        problem.fixed_populations, plan.p, plan.gradient, active);
    if (problem.paths.size() == 1ul)
        plan.exact_single_path_variance = vcstp_exact_single_path_variance(
            plan.populations, plan.p, problem.paths.front());
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= problem.observations.size()) {
            plan.fallback = true;
            plan.reason = "path_level_out_of_range";
            return plan;
        }
        if (problem.observations[level].trials < problem.config.min_samples) {
            plan.fallback = true;
            plan.reason = "insufficient_information";
            return plan;
        }
    }

    const VcstpBounds bounds = vcstp_bounds(problem, active);
    if (!bounds.valid) {
        plan.fallback = true;
        plan.reason = bounds.reason;
        return plan;
    }
    const double requiredLogSurvival = std::log1p(-plan.effective_delta);

    std::vector<vcstp_detail::State> frontier(1ul);
    frontier[0].populations = problem.fixed_populations;
    for (size_t position = 0ul; position < active.size(); ++position) {
        const size_t level = active[position];
        std::vector<vcstp_detail::State> candidates;
        for (size_t stateIndex = 0ul; stateIndex < frontier.size(); ++stateIndex) {
            for (size_t population = bounds.lower[level];
                 population <= bounds.upper[level]; ++population) {
                if (frontier[stateIndex].cost + population > plan.fixed_cost)
                    break;
                vcstp_detail::State state(frontier[stateIndex]);
                state.cost += population;
                state.variance += plan.gradient[level] * plan.gradient[level] *
                    plan.p[level] * (1.0 - plan.p[level]) /
                    static_cast<double>(population);
                state.log_survival += vcstp_log_survival_contribution(
                    plan.rho[level], population);
                state.populations[level] = population;
                candidates.push_back(state);
            }
        }
        frontier = vcstp_detail::pareto_prune(candidates, active);
        if (frontier.empty()) {
            plan.fallback = true;
            plan.reason = "pareto_frontier_empty";
            return plan;
        }
    }
    plan.frontier_states = frontier.size();

    std::vector<const vcstp_detail::State*> feasible;
    for (size_t i = 0ul; i < frontier.size(); ++i)
        if (frontier[i].log_survival + 1.0e-15 >= requiredLogSurvival)
            feasible.push_back(&frontier[i]);
    if (feasible.empty()) {
        plan.fallback = true;
        plan.reason = "risk_constraint_infeasible";
        return plan;
    }

    const vcstp_detail::State* stage1 = feasible.front();
    for (size_t i = 1ul; i < feasible.size(); ++i)
        if (vcstp_stage1_better(feasible[i]->populations,
                                feasible[i]->variance,
                                stage1->populations, stage1->variance, active))
            stage1 = feasible[i];
    plan.stage1_populations = stage1->populations;
    plan.stage1_variance = stage1->variance;
    plan.stage1_cost = stage1->cost;
    const double fixedVarianceTolerance = 1.0e-15 *
        std::max(1.0, std::max(std::fabs(plan.stage1_variance),
                               std::fabs(plan.fixed_variance)));
    if (problem.config.protect_fixed_variance &&
        plan.fixed_variance < plan.stage1_variance - fixedVarianceTolerance) {
        plan.stage1_populations = problem.fixed_populations;
        plan.stage1_variance = plan.fixed_variance;
        plan.stage1_cost = plan.fixed_cost;
        plan.variance_guard_applied = true;
    }

    const double varianceLimit =
        (1.0 + problem.config.epsilon) * plan.stage1_variance;
    std::vector<size_t> stage2Populations(plan.stage1_populations);
    double stage2Variance = plan.stage1_variance;
    size_t stage2Cost = plan.stage1_cost;
    for (size_t i = 0ul; i < feasible.size(); ++i) {
        if (feasible[i]->variance > varianceLimit + 1.0e-15)
            continue;
        if (problem.config.require_strict_cost_reduction &&
            feasible[i]->cost >= plan.stage1_cost)
            continue;
        if (vcstp_stage2_better(feasible[i]->populations,
                                stage2Populations, active)) {
            stage2Populations = feasible[i]->populations;
            stage2Variance = feasible[i]->variance;
            stage2Cost = feasible[i]->cost;
        }
    }

    plan.valid = true;
    plan.fallback = false;
    plan.reason = "none";
    plan.populations = stage2Populations;
    plan.final_variance = stage2Variance;
    plan.final_cost = stage2Cost;
    plan.cost_reduced = plan.final_cost < plan.stage1_cost;
    plan.extinction_risk = vcstp_extinction_risk(
        plan.populations, active, plan.rho);
    if (problem.paths.size() == 1ul)
        plan.exact_single_path_variance = vcstp_exact_single_path_variance(
            plan.populations, plan.p, problem.paths.front());
    return plan;
}

inline VcstpPlan
vcstp_plan_v3(const VcstpProblem& problem)
{
    VcstpPlan plan;
    const std::vector<size_t> active = vcstp_active_levels(problem.paths);
    plan.populations = problem.fixed_populations;
    if (!problem.config.minimize_event_cost_under_fixed_variance ||
        !vcstp_config_valid(problem.config) || active.empty() ||
        problem.observations.size() != problem.fixed_populations.size()) {
        plan.fallback = true;
        plan.reason = "invalid_inputs";
        return plan;
    }
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        if (level >= problem.observations.size()) {
            plan.fallback = true;
            plan.reason = "path_level_out_of_range";
            return plan;
        }
        if (problem.observations[level].trials < problem.config.min_samples) {
            plan.fallback = true;
            plan.reason = "insufficient_information";
            return plan;
        }
    }
    const VcstpBounds bounds = vcstp_v3_bounds(problem, active);
    if (!bounds.valid) {
        plan.fallback = true;
        plan.reason = bounds.reason;
        return plan;
    }

    plan.p = vcstp_smoothed_successes(problem);
    plan.rho = vcstp_smoothed_retentions(problem);
    plan.gradient = vcstp_gradient_weighted(
        problem.paths, problem.path_counts, plan.p);
    const std::vector<double> access = vcstp_path_access_frequencies(
        problem.paths, problem.path_counts, problem.observations.size());
    const std::vector<double> eventCosts = vcstp_event_costs(problem);
    plan.fixed_cost = vcstp_path_cost(problem.fixed_populations, active);
    plan.final_cost = plan.fixed_cost;
    plan.fixed_variance = vcstp_delta_variance(
        problem.fixed_populations, plan.p, plan.gradient, active);
    plan.final_variance = plan.fixed_variance;
    plan.stage1_variance = plan.fixed_variance;
    plan.stage1_populations = problem.fixed_populations;
    plan.stage1_cost = plan.fixed_cost;
    plan.fixed_predicted_event_cost = vcstp_predicted_event_cost(
        problem.fixed_populations, access, eventCosts, active);
    plan.final_predicted_event_cost = plan.fixed_predicted_event_cost;
    const VcstpRiskTarget riskTarget = vcstp_effective_risk_target(
        problem.fixed_populations, active, plan.rho,
        problem.config.delta_path);
    plan.fixed_extinction_risk = riskTarget.fixed_risk;
    plan.effective_delta = riskTarget.effective_delta;
    plan.absolute_risk_target_met = riskTarget.absolute_target_met;
    plan.extinction_risk = plan.fixed_extinction_risk;

    const double varianceLimit = (1.0 + problem.config.variance_tolerance) *
                                 plan.fixed_variance;
    const double numericTolerance = 1.0e-12 *
        std::max(1.0, std::fabs(plan.fixed_predicted_event_cost));
    std::vector<double> varianceCoefficients(
        problem.observations.size(), 0.0);
    std::vector<double> costCoefficients(
        problem.observations.size(), 0.0);
    for (size_t i = 0ul; i < active.size(); ++i) {
        const size_t level = active[i];
        varianceCoefficients[level] = plan.gradient[level] *
            plan.gradient[level] * plan.p[level] * (1.0 - plan.p[level]);
        costCoefficients[level] = std::max(
            1.0e-12, access[level] * eventCosts[level]);
    }

    // Continuous relaxation of
    //   min sum d_l N_l  s.t. sum a_l/N_l <= V_fixed(1+eta).
    // Its KKT solution is N_l=sqrt(lambda*a_l/d_l), clipped to bounds.
    // Bisection finds lambda; upward integer rounding preserves the variance
    // constraint.  A short discrete cleanup then removes avoidable cost.
    const auto continuous_population = [&](const size_t level,
                                           const double lambda) {
        const double raw = varianceCoefficients[level] > 0.0
            ? std::sqrt(lambda * varianceCoefficients[level] /
                        costCoefficients[level])
            : static_cast<double>(bounds.lower[level]);
        return std::max(static_cast<double>(bounds.lower[level]),
               std::min(static_cast<double>(bounds.upper[level]), raw));
    };
    const auto continuous_variance = [&](const double lambda) {
        double result = 0.0;
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            const double population = continuous_population(level, lambda);
            if (!(population > 0.0))
                return std::numeric_limits<double>::infinity();
            result += varianceCoefficients[level] / population;
        }
        return result;
    };
    double lambdaLow = 0.0;
    double lambdaHigh = 1.0;
    while (continuous_variance(lambdaHigh) > varianceLimit &&
           lambdaHigh < 1.0e30)
        lambdaHigh *= 2.0;
    std::vector<size_t> current(problem.fixed_populations);
    if (continuous_variance(lambdaHigh) <= varianceLimit + 1.0e-15) {
        for (size_t iteration = 0ul; iteration < 80ul; ++iteration) {
            const double middle = 0.5 * (lambdaLow + lambdaHigh);
            if (continuous_variance(middle) <= varianceLimit)
                lambdaHigh = middle;
            else
                lambdaLow = middle;
        }
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            size_t population = static_cast<size_t>(std::ceil(
                continuous_population(level, lambdaHigh) - 1.0e-12));
            while (population <= bounds.upper[level] &&
                   !vcstp_v3_population_allowed(problem, level, population))
                ++population;
            current[level] = population <= bounds.upper[level]
                           ? population : problem.fixed_populations[level];
        }
    }
    double currentVariance = vcstp_delta_variance(
        current, plan.p, plan.gradient, active);
    double currentEventCost = vcstp_predicted_event_cost(
        current, access, eventCosts, active);
    double currentRisk = vcstp_weighted_path_extinction_risk(
        current, problem.paths, problem.path_counts, plan.rho);

    // The variance relaxation does not contain the extinction constraint.
    // If needed, add trajectories where each unit gives the largest risk
    // reduction per expected event cost.
    while (currentRisk > plan.effective_delta + 1.0e-12) {
        size_t selected = active.size();
        double bestEfficiency = -1.0;
        double selectedRisk = currentRisk;
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            if (!vcstp_v3_population_allowed(
                    problem, level, current[level] + 1ul))
                continue;
            std::vector<size_t> candidate(current);
            ++candidate[level];
            const double risk = vcstp_weighted_path_extinction_risk(
                candidate, problem.paths, problem.path_counts, plan.rho);
            const double efficiency = (currentRisk - risk) /
                                      costCoefficients[level];
            if (efficiency > bestEfficiency + 1.0e-18) {
                selected = i;
                bestEfficiency = efficiency;
                selectedRisk = risk;
            }
        }
        if (selected == active.size() || bestEfficiency <= 0.0) {
            current = problem.fixed_populations;
            break;
        }
        ++current[active[selected]];
        currentRisk = selectedRisk;
    }

    // Integer cleanup: remove any trajectory whose deletion keeps both
    // quality constraints satisfied, selecting the largest event-cost saving.
    while (true) {
        size_t selected = active.size();
        double bestSaving = 0.0;
        double selectedVariance = currentVariance;
        double selectedRisk = currentRisk;
        for (size_t i = 0ul; i < active.size(); ++i) {
            const size_t level = active[i];
            if (current[level] == 0ul ||
                !vcstp_v3_population_allowed(
                    problem, level, current[level] - 1ul))
                continue;
            std::vector<size_t> candidate(current);
            --candidate[level];
            const double variance = vcstp_delta_variance(
                candidate, plan.p, plan.gradient, active);
            if (variance > varianceLimit + 1.0e-15)
                continue;
            const double risk = vcstp_weighted_path_extinction_risk(
                candidate, problem.paths, problem.path_counts, plan.rho);
            if (risk > plan.effective_delta + 1.0e-12)
                continue;
            if (costCoefficients[level] > bestSaving) {
                selected = i;
                bestSaving = costCoefficients[level];
                selectedVariance = variance;
                selectedRisk = risk;
            }
        }
        if (selected == active.size())
            break;
        --current[active[selected]];
        currentVariance = selectedVariance;
        currentRisk = selectedRisk;
    }
    currentVariance = vcstp_delta_variance(
        current, plan.p, plan.gradient, active);
    currentEventCost = vcstp_predicted_event_cost(
        current, access, eventCosts, active);
    currentRisk = vcstp_weighted_path_extinction_risk(
        current, problem.paths, problem.path_counts, plan.rho);
    const bool improved =
        currentEventCost < plan.fixed_predicted_event_cost - numericTolerance;

    plan.valid = true;
    plan.fallback = false;
    plan.reason = improved ? "none" : "fixed_plan_optimal";
    plan.populations = current;
    plan.final_cost = vcstp_path_cost(current, active);
    plan.final_variance = currentVariance;
    plan.final_predicted_event_cost = currentEventCost;
    plan.extinction_risk = currentRisk;
    plan.cost_reduced = plan.final_cost < plan.fixed_cost;
    plan.event_cost_reduced =
        plan.final_predicted_event_cost <
        plan.fixed_predicted_event_cost - numericTolerance;
    if (!plan.event_cost_reduced) {
        plan.populations = problem.fixed_populations;
        plan.final_cost = plan.fixed_cost;
        plan.final_variance = plan.fixed_variance;
        plan.final_predicted_event_cost = plan.fixed_predicted_event_cost;
        plan.extinction_risk = plan.fixed_extinction_risk;
        plan.cost_reduced = false;
    }
    if (problem.paths.size() == 1ul)
        plan.exact_single_path_variance = vcstp_exact_single_path_variance(
            plan.populations, plan.p, problem.paths.front());
    return plan;
}

} // namespace fig

#endif // VCSTP_H
