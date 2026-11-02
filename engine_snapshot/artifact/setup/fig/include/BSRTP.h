// Bayesian Sequential Risk-balanced Trajectory Planning (BSRTP).
#ifndef BSRTP_H
#define BSRTP_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fig
{

struct BsrtpObservation
{
    double trials;
    double successes;
    double retained;
    double rare_only_successes;
    double event_transitions;
    double visits;

    BsrtpObservation() :
        trials(0.0), successes(0.0), retained(0.0),
        rare_only_successes(0.0), event_transitions(0.0), visits(0.0)
    {}
};

struct BsrtpConfig
{
    double prior_alpha;
    double prior_beta;
    size_t min_population;
    size_t max_population;
    size_t reference_population;
    double budget_multiplier;
    double risk_relaxation;
    double max_change_share;
    double access_floor;
    double cost_floor;
    double history_decay;
    size_t replan_interval;

    BsrtpConfig() :
        prior_alpha(0.5), prior_beta(0.5),
        min_population(12ul), max_population(360ul),
        reference_population(288ul), budget_multiplier(1.0),
        risk_relaxation(0.05), max_change_share(0.20),
        access_floor(0.05), cost_floor(1.0), history_decay(1.0),
        replan_interval(1ul)
    {}
};

struct BsrtpPlan
{
    bool valid;
    bool cost_compressed;
    std::string reason;
    std::vector<size_t> stage1_populations;
    std::vector<size_t> populations;
    std::vector<size_t> lower;
    std::vector<size_t> upper;
    std::vector<double> alpha;
    std::vector<double> beta;
    std::vector<double> pool_alpha;
    std::vector<double> pool_beta;
    std::vector<bool> risk_active;
    std::vector<double> access;
    std::vector<double> unit_cost;
    std::vector<double> cost;
    std::vector<double> final_risk;
    std::vector<double> final_pool_risk;
    double budget;
    double stage1_cost;
    double predicted_cost;
    double optimal_worst_risk;
    double final_worst_risk;

    BsrtpPlan() :
        valid(false), cost_compressed(false), reason("invalid"),
        stage1_populations(), populations(), lower(), upper(),
        alpha(), beta(), pool_alpha(), pool_beta(), risk_active(),
        access(), unit_cost(), cost(), final_risk(), final_pool_risk(),
        budget(0.0), stage1_cost(0.0), predicted_cost(0.0),
        optimal_worst_risk(1.0), final_worst_risk(1.0)
    {}
};

inline void
validate_bsrtp_config(const BsrtpConfig& config)
{
    if (!(config.prior_alpha > 0.0) || !(config.prior_beta > 0.0) ||
        !std::isfinite(config.prior_alpha) ||
        !std::isfinite(config.prior_beta))
        throw std::invalid_argument("BSRTP prior parameters must be positive");
    if (config.min_population == 0ul ||
        config.max_population < config.min_population ||
        config.reference_population < config.min_population ||
        config.reference_population > config.max_population)
        throw std::invalid_argument("invalid BSRTP population bounds");
    if (!(config.budget_multiplier > 0.0) ||
        !std::isfinite(config.budget_multiplier))
        throw std::invalid_argument("BSRTP budget multiplier must be positive");
    if (config.risk_relaxation < 0.0 ||
        !std::isfinite(config.risk_relaxation))
        throw std::invalid_argument("BSRTP risk relaxation must be non-negative");
    if (config.max_change_share < 0.0 ||
        config.max_change_share > 1.0 ||
        !std::isfinite(config.max_change_share))
        throw std::invalid_argument("BSRTP change share must lie in [0,1]");
    if (!(config.access_floor > 0.0) || config.access_floor > 1.0 ||
        !std::isfinite(config.access_floor) ||
        !(config.cost_floor > 0.0) || !std::isfinite(config.cost_floor))
        throw std::invalid_argument("invalid BSRTP cost floors");
    if (!(config.history_decay > 0.0) || config.history_decay > 1.0 ||
        !std::isfinite(config.history_decay) || config.replan_interval == 0ul)
        throw std::invalid_argument("invalid BSRTP sequential configuration");
}

inline double
bsrtp_posterior_extinction(const double alpha, const double beta,
                           const size_t population)
{
    if (!(alpha > 0.0) || !(beta > 0.0) ||
        !std::isfinite(alpha) || !std::isfinite(beta))
        return std::numeric_limits<double>::quiet_NaN();
    double logRisk = 0.0;
    for (size_t i = 0ul; i < population; ++i)
        logRisk += std::log(beta + static_cast<double>(i)) -
                   std::log(alpha + beta + static_cast<double>(i));
    const double risk = std::exp(logRisk);
    return std::min(1.0, std::max(0.0, risk));
}

class BsrtpHistory
{
private:
    std::vector<BsrtpObservation> observations_;

public:
    explicit BsrtpHistory(const size_t levels = 0ul) :
        observations_(levels)
    {}

    void reset(const size_t levels = 0ul)
    {
        observations_.assign(levels, BsrtpObservation());
    }

    void observe(const size_t level, const size_t trials,
                 const size_t successes, const size_t retained,
                 const size_t rareOnlySuccesses,
                 const size_t eventTransitions)
    {
        if (successes > trials || retained > trials ||
            rareOnlySuccesses > successes)
            throw std::invalid_argument("invalid BSRTP stage counts");
        if (observations_.size() <= level)
            observations_.resize(level + 1ul);
        BsrtpObservation& item(observations_[level]);
        item.trials += static_cast<double>(trials);
        item.successes += static_cast<double>(successes);
        item.retained += static_cast<double>(retained);
        item.rare_only_successes += static_cast<double>(rareOnlySuccesses);
        item.event_transitions += static_cast<double>(eventTransitions);
        item.visits += 1.0;
    }

    void decay(const double factor)
    {
        if (!(factor > 0.0) || factor > 1.0 || !std::isfinite(factor))
            throw std::invalid_argument("BSRTP decay must lie in (0,1]");
        for (size_t level = 0ul; level < observations_.size(); ++level) {
            observations_[level].trials *= factor;
            observations_[level].successes *= factor;
            observations_[level].retained *= factor;
            observations_[level].rare_only_successes *= factor;
            observations_[level].event_transitions *= factor;
            observations_[level].visits *= factor;
        }
    }

    const std::vector<BsrtpObservation>& observations() const noexcept
    {
        return observations_;
    }
};

inline double
bsrtp_plan_cost(const std::vector<size_t>& populations,
                const std::vector<double>& cost)
{
    if (populations.size() != cost.size())
        return std::numeric_limits<double>::infinity();
    double result = 0.0;
    for (size_t level = 0ul; level < populations.size(); ++level)
        result += cost[level] * static_cast<double>(populations[level]);
    return result;
}

inline std::vector<size_t>
bsrtp_minimum_for_risk(const std::vector<double>& alpha,
                       const std::vector<double>& beta,
                       const std::vector<size_t>& lower,
                       const std::vector<size_t>& upper,
                       const std::vector<bool>& active,
                       const double target,
                       bool& feasible)
{
    const size_t levels = alpha.size();
    std::vector<size_t> result(levels, 0ul);
    feasible = true;
    for (size_t level = 0ul; level < levels; ++level) {
        result[level] = lower[level];
        if (!active[level])
            continue;
        while (result[level] < upper[level] &&
               bsrtp_posterior_extinction(
                   alpha[level], beta[level], result[level]) >
                   target + 1.0e-15)
            ++result[level];
        if (bsrtp_posterior_extinction(
                alpha[level], beta[level], result[level]) >
            target + 1.0e-15) {
            feasible = false;
            return result;
        }
    }
    return result;
}

inline BsrtpPlan
bsrtp_plan(const std::vector<BsrtpObservation>& observations,
           const std::vector<size_t>& previous,
           const std::vector<size_t>& referencePopulations,
           const BsrtpConfig& config)
{
    validate_bsrtp_config(config);
    BsrtpPlan plan;
    const size_t levels = observations.size();
    if (levels == 0ul) {
        plan.reason = "no_levels";
        return plan;
    }
    if (!previous.empty() && previous.size() != levels) {
        plan.reason = "previous_size_mismatch";
        return plan;
    }
    if (!referencePopulations.empty() &&
        referencePopulations.size() != levels) {
        plan.reason = "reference_size_mismatch";
        return plan;
    }

    plan.alpha.resize(levels);
    plan.beta.resize(levels);
    plan.pool_alpha.resize(levels);
    plan.pool_beta.resize(levels);
    plan.risk_active.resize(levels, false);
    plan.access.resize(levels);
    plan.unit_cost.resize(levels);
    plan.cost.resize(levels);
    plan.lower.resize(levels, config.min_population);
    plan.upper.resize(levels, config.max_population);
    double maxVisits = 0.0;
    double conservativeUnitCost = config.cost_floor;
    for (size_t level = 0ul; level < levels; ++level) {
        const BsrtpObservation& item(observations[level]);
        if (item.trials < 0.0 || item.successes < 0.0 ||
            item.successes > item.trials + 1.0e-9 ||
            item.retained < 0.0 ||
            item.retained > item.trials + 1.0e-9 ||
            item.rare_only_successes < 0.0 ||
            item.rare_only_successes > item.successes + 1.0e-9 ||
            item.event_transitions < 0.0 || item.visits < 0.0) {
            plan.reason = "invalid_observation";
            return plan;
        }
        maxVisits = std::max(maxVisits, item.visits);
        if (item.trials > 0.0)
            conservativeUnitCost = std::max(
                conservativeUnitCost, item.event_transitions / item.trials);
    }

    for (size_t level = 0ul; level < levels; ++level) {
        const BsrtpObservation& item(observations[level]);
        plan.alpha[level] = config.prior_alpha + item.successes;
        plan.beta[level] = config.prior_beta + item.trials - item.successes;
        plan.pool_alpha[level] = config.prior_alpha + item.retained;
        plan.pool_beta[level] = config.prior_beta + item.trials - item.retained;
        plan.risk_active[level] = item.trials > 0.0;
        plan.access[level] = maxVisits > 0.0
            ? std::max(config.access_floor, item.visits / maxVisits)
            : 1.0;
        plan.unit_cost[level] = item.trials > 0.0
            ? std::max(config.cost_floor,
                       item.event_transitions / item.trials)
            : conservativeUnitCost;
        plan.cost[level] = plan.access[level] * plan.unit_cost[level];
        if (!plan.risk_active[level]) {
            const size_t neutral = referencePopulations.empty()
                ? config.reference_population
                : referencePopulations[level];
            plan.lower[level] = std::max(
                config.min_population, std::min(config.max_population, neutral));
            plan.upper[level] = plan.lower[level];
        } else if (!previous.empty()) {
            const double value = static_cast<double>(previous[level]);
            const size_t changeLower = static_cast<size_t>(std::ceil(
                value * (1.0 - config.max_change_share) - 1.0e-12));
            const size_t changeUpper = static_cast<size_t>(std::floor(
                value * (1.0 + config.max_change_share) + 1.0e-12));
            plan.lower[level] = std::max(config.min_population, changeLower);
            plan.upper[level] = std::min(config.max_population,
                                         std::max(changeLower, changeUpper));
        }
        if (plan.lower[level] > plan.upper[level]) {
            plan.reason = "empty_bounds";
            return plan;
        }
    }

    std::vector<size_t> reference = referencePopulations.empty()
        ? std::vector<size_t>(levels, config.reference_population)
        : referencePopulations;
    for (size_t level = 0ul; level < levels; ++level) {
        reference[level] = std::max(
            config.min_population,
            std::min(config.max_population, reference[level]));
    }
    plan.budget = config.budget_multiplier *
                  bsrtp_plan_cost(reference, plan.cost);
    const double minimumCost = bsrtp_plan_cost(plan.lower, plan.cost);
    if (minimumCost > plan.budget + 1.0e-9) {
        plan.reason = "budget_infeasible";
        return plan;
    }

    std::vector<double> candidates;
    candidates.reserve(levels * (config.max_population + 1ul));
    for (size_t level = 0ul; level < levels; ++level) {
        if (!plan.risk_active[level])
            continue;
        for (size_t n = plan.lower[level]; n <= plan.upper[level]; ++n)
            candidates.push_back(bsrtp_posterior_extinction(
                plan.alpha[level], plan.beta[level], n));
    }
    if (candidates.empty()) {
        plan.reason = "no_observed_levels";
        return plan;
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());

    bool found = false;
    for (size_t i = 0ul; i < candidates.size(); ++i) {
        bool riskFeasible = false;
        const std::vector<size_t> candidate = bsrtp_minimum_for_risk(
            plan.alpha, plan.beta, plan.lower, plan.upper, plan.risk_active,
            candidates[i], riskFeasible);
        if (!riskFeasible)
            continue;
        const double cost = bsrtp_plan_cost(candidate, plan.cost);
        if (cost <= plan.budget + 1.0e-9) {
            plan.optimal_worst_risk = candidates[i];
            plan.stage1_populations = candidate;
            plan.stage1_cost = cost;
            found = true;
            break;
        }
    }
    if (!found) {
        plan.reason = "no_feasible_risk";
        return plan;
    }

    const double relaxedTarget = std::min(
        1.0, (1.0 + config.risk_relaxation) * plan.optimal_worst_risk);
    bool relaxedFeasible = false;
    plan.populations = bsrtp_minimum_for_risk(
        plan.alpha, plan.beta, plan.lower, plan.upper,
        plan.risk_active, relaxedTarget, relaxedFeasible);
    if (!relaxedFeasible)
        plan.populations = plan.stage1_populations;
    plan.predicted_cost = bsrtp_plan_cost(plan.populations, plan.cost);
    plan.final_risk.resize(levels, 1.0);
    plan.final_pool_risk.resize(levels, 1.0);
    plan.final_worst_risk = 0.0;
    for (size_t level = 0ul; level < levels; ++level) {
        plan.final_pool_risk[level] = bsrtp_posterior_extinction(
            plan.pool_alpha[level], plan.pool_beta[level],
            plan.populations[level]);
        if (plan.risk_active[level]) {
            plan.final_risk[level] = bsrtp_posterior_extinction(
                plan.alpha[level], plan.beta[level], plan.populations[level]);
            plan.final_worst_risk = std::max(
                plan.final_worst_risk, plan.final_risk[level]);
        }
    }
    plan.cost_compressed = plan.predicted_cost < plan.stage1_cost - 1.0e-9;
    plan.valid = plan.predicted_cost <= plan.budget + 1.0e-9 &&
        plan.final_worst_risk <= relaxedTarget + 1.0e-12;
    plan.reason = plan.valid
        ? (plan.cost_compressed ? "risk_relaxed_cost_compression" : "optimal")
        : "postcondition_failed";
    return plan;
}

inline BsrtpPlan
bsrtp_plan(const std::vector<BsrtpObservation>& observations,
           const std::vector<size_t>& previous,
           const BsrtpConfig& config)
{
    return bsrtp_plan(observations, previous, {}, config);
}

} // namespace fig

#endif // BSRTP_H
