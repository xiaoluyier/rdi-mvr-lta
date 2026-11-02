// Cost-Constrained Sequential Level Allocation (CSLA).
#ifndef CSLALEVELALLOCATION_H
#define CSLALEVELALLOCATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace fig
{

struct CslaConfig
{
    size_t min_population;
    size_t max_population;
    size_t min_samples;
    double max_batch_change;
    double alpha_q;
    double beta_q;
    double alpha_rho;
    double beta_rho;
    double max_extinction_risk;
    double cost_floor;
    size_t exact_check_limit;

    CslaConfig() :
        min_population(8ul),
        max_population(360ul),
        min_samples(48ul),
        max_batch_change(0.25),
        alpha_q(0.5),
        beta_q(0.5),
        alpha_rho(0.5),
        beta_rho(0.5),
        max_extinction_risk(1.0),
        cost_floor(1.0),
        exact_check_limit(200000ul)
    {}
};

struct CslaObservation
{
    size_t trials;
    size_t successes;
    size_t retained;
    size_t event_transitions;

    CslaObservation() :
        trials(0ul), successes(0ul), retained(0ul), event_transitions(0ul)
    {}
};

class CslaHistory
{
public:
    CslaHistory() : observations_(), generation_(0ul) {}

    void reset(const size_t levels)
    {
        observations_.assign(levels, CslaObservation());
        ++generation_;
    }

    void observe(const size_t level,
                 const size_t trials,
                 const size_t successes,
                 const size_t retained,
                 const size_t eventTransitions)
    {
        if (level >= observations_.size())
            observations_.resize(level + 1ul);
        CslaObservation& item(observations_[level]);
        item.trials += trials;
        item.successes += successes;
        item.retained += retained;
        item.event_transitions += eventTransitions;
    }

    const std::vector<CslaObservation>& observations() const
        { return observations_; }

    size_t generation() const { return generation_; }

private:
    std::vector<CslaObservation> observations_;
    size_t generation_;
};

struct CslaPlan
{
    bool valid;
    bool conservative;
    bool exact_checked;
    bool exact_match;
    std::string reason;
    std::vector<size_t> populations;
    std::vector<double> q;
    std::vector<double> rho;
    std::vector<double> cost;
    std::vector<size_t> lower;
    std::vector<size_t> upper;
    double budget;
    double predicted_cost;
    double variance_term;
    double extinction_term;
    double objective;
    double lambda;
    bool risk_constraint_satisfied;
    double risk_target;
    size_t frozen_levels;

    CslaPlan() :
        valid(false), conservative(false), exact_checked(false),
        exact_match(false), reason(), populations(), q(), rho(), cost(),
        lower(), upper(), budget(0.0), predicted_cost(0.0),
        variance_term(0.0), extinction_term(0.0), objective(0.0),
        lambda(0.0), risk_constraint_satisfied(false), risk_target(0.0),
        frozen_levels(0ul)
    {}
};

inline double csla_clamp_probability(const double value)
{
    return std::max(1.0e-12, std::min(1.0 - 1.0e-12, value));
}

inline double csla_smoothed_probability(const size_t count,
                                        const size_t trials,
                                        const double alpha,
                                        const double beta)
{
    return csla_clamp_probability(
        (static_cast<double>(count) + alpha) /
        (static_cast<double>(trials) + alpha + beta));
}

inline double csla_unit_cost(const CslaObservation& observation,
                             const double floor)
{
    const double safeFloor = std::max(1.0e-12, floor);
    if (observation.trials == 0ul)
        return safeFloor;
    const double mean = static_cast<double>(observation.event_transitions) /
                        static_cast<double>(observation.trials);
    return std::isfinite(mean) ? std::max(safeFloor, mean) : safeFloor;
}

inline double csla_variance_term(const std::vector<size_t>& populations,
                                 const std::vector<double>& q)
{
    double value = 0.0;
    for (size_t level = 0ul; level < populations.size(); ++level)
        value += (1.0 - q[level]) /
                 (static_cast<double>(populations[level]) * q[level]);
    return value;
}

inline double csla_extinction_raw(const std::vector<size_t>& populations,
                                  const std::vector<double>& rho)
{
    double value = 0.0;
    for (size_t level = 0ul; level < populations.size(); ++level)
        value += std::pow(1.0 - rho[level],
                          static_cast<double>(populations[level]));
    return value;
}

inline double csla_population_cost(const std::vector<size_t>& populations,
                                   const std::vector<double>& cost)
{
    double value = 0.0;
    for (size_t level = 0ul; level < populations.size(); ++level)
        value += static_cast<double>(populations[level]) * cost[level];
    return value;
}

inline double csla_objective(const std::vector<size_t>& populations,
                             const std::vector<double>& q,
                             const std::vector<double>& rho,
                             const double lambda)
{
    // The primary planning objective is the delta-method approximation to
    // the relative variance of the final product estimator.  Extinction is
    // enforced separately as a feasibility constraint; it is deliberately
    // not added as a tunable penalty term.
    (void)rho;
    (void)lambda;
    return csla_variance_term(populations, q);
}

inline bool csla_risk_constraints_satisfied(
    const std::vector<size_t>& populations,
    const std::vector<double>& rho,
    const double target)
{
    if (populations.size() != rho.size() ||
        !(target >= 0.0) || !std::isfinite(target))
        return false;
    for (size_t level = 0ul; level < populations.size(); ++level)
        if (std::pow(1.0 - rho[level],
                     static_cast<double>(populations[level])) >
            target + 1.0e-12)
            return false;
    return true;
}

inline double csla_marginal_gain(const size_t population,
                                 const double q,
                                 const double rho,
                                 const double lambda)
{
    const double n = static_cast<double>(population);
    const double varianceGain = (1.0 - q) / (q * n * (n + 1.0));
    const double extinctionGain = lambda * rho * std::pow(1.0 - rho, n);
    return varianceGain + extinctionGain;
}

inline bool csla_lexicographically_less(const std::vector<size_t>& lhs,
                                        const std::vector<size_t>& rhs)
{
    return std::lexicographical_compare(lhs.begin(), lhs.end(),
                                        rhs.begin(), rhs.end());
}

inline CslaPlan csla_plan_levels(
    const std::vector<CslaObservation>& observations,
    const std::vector<size_t>& fixedPopulations,
    const std::vector<size_t>& previousPopulations,
    const double budget,
    const CslaConfig& config)
{
    CslaPlan plan;
    plan.budget = std::max(0.0, budget);
    const size_t levels = observations.size();
    if (levels == 0ul || fixedPopulations.size() != levels ||
        previousPopulations.size() != levels) {
        plan.reason = "invalid_level_inputs";
        return plan;
    }
    if (config.min_population == 0ul ||
        config.max_population < config.min_population ||
        config.min_samples == 0ul ||
        config.max_batch_change < 0.0 || config.max_batch_change > 1.0 ||
        config.alpha_q <= 0.0 || config.beta_q <= 0.0 ||
        config.alpha_rho <= 0.0 || config.beta_rho <= 0.0 ||
        config.max_extinction_risk < 0.0 ||
        config.max_extinction_risk > 1.0 ||
        !std::isfinite(config.max_extinction_risk) ||
        config.cost_floor <= 0.0 || !std::isfinite(plan.budget)) {
        plan.reason = "invalid_configuration";
        return plan;
    }

    plan.q.resize(levels);
    plan.rho.resize(levels);
    plan.cost.resize(levels);
    plan.lower.resize(levels);
    plan.upper.resize(levels);
    std::vector<size_t> reference(levels);
    size_t searchSpace = 1ul;
    bool searchSpaceKnown = true;
    double conservativeUnobservedCost = config.cost_floor;
    for (size_t level = 0ul; level < levels; ++level)
        if (observations[level].trials > 0ul)
            conservativeUnobservedCost = std::max(
                conservativeUnobservedCost,
                csla_unit_cost(observations[level], config.cost_floor));
    for (size_t level = 0ul; level < levels; ++level) {
        const CslaObservation& item(observations[level]);
        plan.q[level] = csla_smoothed_probability(
            item.successes, item.trials, config.alpha_q, config.beta_q);
        plan.rho[level] = csla_smoothed_probability(
            item.retained, item.trials, config.alpha_rho, config.beta_rho);
        plan.cost[level] = item.trials > 0ul
            ? csla_unit_cost(item, config.cost_floor)
            : conservativeUnobservedCost;

        const size_t previous = std::max(config.min_population,
            std::min(config.max_population, previousPopulations[level]));
        const size_t changeLower = static_cast<size_t>(std::ceil(
            static_cast<double>(previous) * (1.0 - config.max_batch_change)));
        const size_t changeUpper = static_cast<size_t>(std::floor(
            static_cast<double>(previous) * (1.0 + config.max_batch_change)));
        plan.lower[level] = std::max(config.min_population, changeLower);
        plan.upper[level] = std::min(config.max_population, changeUpper);
        if (item.trials < config.min_samples) {
            plan.lower[level] = previous;
            plan.upper[level] = previous;
            ++plan.frozen_levels;
        }
        if (plan.lower[level] > plan.upper[level]) {
            plan.reason = "bounds_infeasible";
            return plan;
        }
        reference[level] = std::max(plan.lower[level],
            std::min(plan.upper[level], fixedPopulations[level]));
        const size_t width = plan.upper[level] - plan.lower[level] + 1ul;
        if (searchSpaceKnown &&
            (width == 0ul || searchSpace > config.exact_check_limit / width))
            searchSpaceKnown = false;
        else if (searchSpaceKnown)
            searchSpace *= width;
    }

    plan.lambda = 0.0;
    plan.risk_target = config.max_extinction_risk;

    for (size_t level = 0ul; level < levels; ++level) {
        while (plan.lower[level] < plan.upper[level] &&
               std::pow(1.0 - plan.rho[level],
                        static_cast<double>(plan.lower[level])) >
                   plan.risk_target + 1.0e-12)
            ++plan.lower[level];
        if (std::pow(1.0 - plan.rho[level],
                     static_cast<double>(plan.lower[level])) >
            plan.risk_target + 1.0e-12) {
            plan.reason = "risk_constraint_infeasible";
            return plan;
        }
    }

    std::vector<size_t> greedy(plan.lower);
    double greedyCost = csla_population_cost(greedy, plan.cost);
    if (greedyCost > plan.budget + 1.0e-9) {
        plan.reason = "budget_infeasible";
        return plan;
    }
    while (true) {
        size_t selected = levels;
        double bestGainPerCost = -1.0;
        for (size_t level = 0ul; level < levels; ++level) {
            if (greedy[level] >= plan.upper[level] ||
                greedyCost + plan.cost[level] > plan.budget + 1.0e-9)
                continue;
            const double gainPerCost = csla_marginal_gain(
                greedy[level], plan.q[level], plan.rho[level], plan.lambda) /
                plan.cost[level];
            if (gainPerCost > bestGainPerCost + 1.0e-15 ||
                (std::fabs(gainPerCost - bestGainPerCost) <= 1.0e-15 &&
                 level < selected)) {
                selected = level;
                bestGainPerCost = gainPerCost;
            }
        }
        if (selected == levels)
            break;
        ++greedy[selected];
        greedyCost += plan.cost[selected];
    }

    std::vector<size_t> chosen(greedy);
    double chosenCost = greedyCost;
    double chosenObjective = csla_objective(
        chosen, plan.q, plan.rho, plan.lambda);
    if (searchSpaceKnown && searchSpace <= config.exact_check_limit) {
        plan.exact_checked = true;
        std::vector<size_t> candidate(plan.lower);
        std::vector<size_t> exactBest;
        double exactCost = 0.0;
        double exactObjective = std::numeric_limits<double>::infinity();
        std::function<void(size_t,double)> enumerate =
            [&](const size_t level, const double partialCost) {
                if (partialCost > plan.budget + 1.0e-9)
                    return;
                if (level == levels) {
                    const double value = csla_objective(
                        candidate, plan.q, plan.rho, plan.lambda);
                    if (!csla_risk_constraints_satisfied(
                            candidate, plan.rho, plan.risk_target))
                        return;
                    if (value < exactObjective - 1.0e-15 ||
                        (std::fabs(value - exactObjective) <= 1.0e-15 &&
                         (partialCost < exactCost - 1.0e-9 ||
                          (std::fabs(partialCost - exactCost) <= 1.0e-9 &&
                           (exactBest.empty() || csla_lexicographically_less(
                               candidate, exactBest)))))) {
                        exactBest = candidate;
                        exactCost = partialCost;
                        exactObjective = value;
                    }
                    return;
                }
                for (size_t n = plan.lower[level]; n <= plan.upper[level]; ++n) {
                    candidate[level] = n;
                    enumerate(level + 1ul,
                              partialCost + static_cast<double>(n) *
                                            plan.cost[level]);
                    if (n == plan.upper[level])
                        break;
                }
            };
        enumerate(0ul, 0.0);
        if (exactBest.empty()) {
            plan.reason = "exact_solver_infeasible";
            return plan;
        }
        plan.exact_match = greedy == exactBest;
        chosen = exactBest;
        chosenCost = exactCost;
        chosenObjective = exactObjective;
    }

    plan.risk_constraint_satisfied =
        csla_risk_constraints_satisfied(chosen, plan.rho, plan.risk_target);
    plan.valid = plan.risk_constraint_satisfied;
    plan.conservative = plan.frozen_levels == levels;
    plan.reason = plan.conservative ? "all_levels_frozen" : "none";
    plan.populations = chosen;
    plan.predicted_cost = chosenCost;
    plan.variance_term = csla_variance_term(chosen, plan.q);
    plan.extinction_term = csla_extinction_raw(chosen, plan.rho);
    plan.objective = chosenObjective;
    return plan;
}

} // namespace fig

#endif // CSLALEVELALLOCATION_H
