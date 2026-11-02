// Adaptive level-wise particle planning for Fixed Effort splitting.
#ifndef ADAPTIVELEVELPOPULATION_H
#define ADAPTIVELEVELPOPULATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fig
{

struct AdaptiveLevelPopulationConfig
{
    size_t min_population;
    size_t max_population;
    double max_extinction_risk;
    double prior_alpha;
    double prior_beta;
    size_t reference_population;

    AdaptiveLevelPopulationConfig() :
        min_population(24ul),
        max_population(360ul),
        max_extinction_risk(0.0025),
        prior_alpha(0.5),
        prior_beta(0.5),
        reference_population(288ul)
    {}
};

inline void
validate_adaptive_level_population_config(
    const AdaptiveLevelPopulationConfig& config)
{
    if (config.min_population == 0ul)
        throw std::invalid_argument(
            "adaptive minimum population must be positive");
    if (config.max_population < config.min_population)
        throw std::invalid_argument(
            "adaptive maximum population must not be below the minimum");
    if (config.max_extinction_risk <= 0.0 ||
        config.max_extinction_risk >= 1.0)
        throw std::invalid_argument(
            "adaptive extinction risk must lie in (0,1)");
    if (config.prior_alpha <= 0.0 || config.prior_beta <= 0.0)
        throw std::invalid_argument(
            "adaptive smoothing parameters must be positive");
    if (config.reference_population < config.min_population)
        throw std::invalid_argument(
            "adaptive reference population must not be below the minimum");
}

inline double
adaptive_level_retention_probability(
    const size_t retained,
    const size_t trials,
    const AdaptiveLevelPopulationConfig& config)
{
    validate_adaptive_level_population_config(config);
    if (retained > trials)
        throw std::invalid_argument(
            "adaptive retained count must not exceed the trial count");
    if (trials == 0ul)
        return 0.0;
    return (static_cast<double>(retained) + config.prior_alpha) /
           (static_cast<double>(trials) +
            config.prior_alpha + config.prior_beta);
}

inline size_t
adaptive_level_population_from_counts(
    const size_t retained,
    const size_t trials,
    const AdaptiveLevelPopulationConfig& config)
{
    const double probability =
        adaptive_level_retention_probability(retained, trials, config);
    if (probability <= 0.0)
        return config.max_population;
    if (probability >= 1.0)
        return config.min_population;

    const double raw = std::ceil(
        std::log(config.max_extinction_risk) /
        std::log1p(-probability));
    if (!std::isfinite(raw) ||
        raw >= static_cast<double>(config.max_population))
        return config.max_population;
    if (raw <= static_cast<double>(config.min_population))
        return config.min_population;
    return static_cast<size_t>(raw);
}

class AdaptiveLevelPopulationPlan
{
private:
    AdaptiveLevelPopulationConfig config_;
    std::vector<size_t> trials_;
    std::vector<size_t> retained_;
    std::vector<size_t> successes_;
    std::vector<size_t> rareOnlySuccesses_;
    std::vector<size_t> eventTransitions_;
    std::vector<double> wallSeconds_;
    std::vector<size_t> populations_;
    double totalReferenceBudget_;
    double calibrationEventTransitions_;
    double productionBudgetPerRun_;
    double predictedProductionCost_;
    size_t unobservedFallbackLevels_;
    std::string plannerMode_;
    std::string fallbackReason_;
    bool finalized_;

public:
    explicit AdaptiveLevelPopulationPlan(
        const AdaptiveLevelPopulationConfig& config =
            AdaptiveLevelPopulationConfig()) :
        config_(config),
        trials_(),
        retained_(),
        successes_(),
        rareOnlySuccesses_(),
        eventTransitions_(),
        wallSeconds_(),
        populations_(),
        totalReferenceBudget_(0.0),
        calibrationEventTransitions_(0.0),
        productionBudgetPerRun_(0.0),
        predictedProductionCost_(0.0),
        unobservedFallbackLevels_(0ul),
        plannerMode_(),
        fallbackReason_(),
        finalized_(false)
    {
        validate_adaptive_level_population_config(config_);
    }

    void reset()
    {
        trials_.clear();
        retained_.clear();
        successes_.clear();
        rareOnlySuccesses_.clear();
        eventTransitions_.clear();
        wallSeconds_.clear();
        populations_.clear();
        totalReferenceBudget_ = 0.0;
        calibrationEventTransitions_ = 0.0;
        productionBudgetPerRun_ = 0.0;
        predictedProductionCost_ = 0.0;
        unobservedFallbackLevels_ = 0ul;
        plannerMode_.clear();
        fallbackReason_.clear();
        finalized_ = false;
    }

    void observe(const size_t level,
                 const size_t trials,
                 const size_t retained,
                 const size_t eventTransitions,
                 const double wallSeconds)
    {
        observe_success(level, trials, retained, retained, 0ul,
                        eventTransitions, wallSeconds);
    }

    void observe_success(const size_t level,
                         const size_t trials,
                         const size_t retained,
                         const size_t successes,
                         const size_t rareOnlySuccesses,
                         const size_t eventTransitions,
                         const double wallSeconds)
    {
        if (finalized_)
            throw std::logic_error(
                "cannot add calibration observations after finalization");
        if (retained > trials)
            throw std::invalid_argument(
                "adaptive retained count must not exceed the trial count");
        if (successes > trials)
            throw std::invalid_argument(
                "adaptive success count must not exceed the trial count");
        if (rareOnlySuccesses > successes)
            throw std::invalid_argument(
                "adaptive rare-only success count must not exceed the success count");
        if (trials_.size() <= level) {
            trials_.resize(level + 1ul, 0ul);
            retained_.resize(level + 1ul, 0ul);
            successes_.resize(level + 1ul, 0ul);
            rareOnlySuccesses_.resize(level + 1ul, 0ul);
            eventTransitions_.resize(level + 1ul, 0ul);
            wallSeconds_.resize(level + 1ul, 0.0);
        }
        trials_[level] += trials;
        retained_[level] += retained;
        successes_[level] += successes;
        rareOnlySuccesses_[level] += rareOnlySuccesses;
        eventTransitions_[level] += eventTransitions;
        wallSeconds_[level] += wallSeconds;
    }

    void finalize_extinction(const size_t levelCount)
    {
        populations_.assign(levelCount, config_.max_population);
        if (trials_.size() < levelCount) {
            trials_.resize(levelCount, 0ul);
            retained_.resize(levelCount, 0ul);
            successes_.resize(levelCount, 0ul);
            rareOnlySuccesses_.resize(levelCount, 0ul);
            eventTransitions_.resize(levelCount, 0ul);
            wallSeconds_.resize(levelCount, 0.0);
        }
        calibrationEventTransitions_ = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            calibrationEventTransitions_ +=
                static_cast<double>(eventTransitions_[level]);
            populations_[level] = adaptive_level_population_from_counts(
                retained_[level], trials_[level], config_);
        }
        totalReferenceBudget_ = 0.0;
        productionBudgetPerRun_ = 0.0;
        predictedProductionCost_ = 0.0;
        unobservedFallbackLevels_ = 0ul;
        plannerMode_ = "retention_extinction";
        fallbackReason_ = "legacy_extinction";
        finalized_ = true;
    }

    void finalize_reference(const size_t levelCount,
                            const std::string& reason)
    {
        populations_.assign(levelCount, std::max(
            config_.min_population,
            std::min(config_.max_population,
                     config_.reference_population)));
        if (trials_.size() < levelCount) {
            trials_.resize(levelCount, 0ul);
            retained_.resize(levelCount, 0ul);
            successes_.resize(levelCount, 0ul);
            rareOnlySuccesses_.resize(levelCount, 0ul);
            eventTransitions_.resize(levelCount, 0ul);
            wallSeconds_.resize(levelCount, 0.0);
        }
        calibrationEventTransitions_ = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level)
            calibrationEventTransitions_ +=
                static_cast<double>(eventTransitions_[level]);
        totalReferenceBudget_ = 0.0;
        productionBudgetPerRun_ = 0.0;
        predictedProductionCost_ = 0.0;
        unobservedFallbackLevels_ = 0ul;
        plannerMode_ = "reference_only";
        fallbackReason_ = reason;
        finalized_ = true;
    }

    void finalize_success_extinction(const size_t levelCount)
    {
        populations_.assign(levelCount, config_.max_population);
        if (trials_.size() < levelCount) {
            trials_.resize(levelCount, 0ul);
            retained_.resize(levelCount, 0ul);
            successes_.resize(levelCount, 0ul);
            rareOnlySuccesses_.resize(levelCount, 0ul);
            eventTransitions_.resize(levelCount, 0ul);
            wallSeconds_.resize(levelCount, 0.0);
        }
        calibrationEventTransitions_ = 0.0;
        unobservedFallbackLevels_ = 0ul;
        for (size_t level = 0ul; level < levelCount; ++level) {
            calibrationEventTransitions_ +=
                static_cast<double>(eventTransitions_[level]);
            if (trials_[level] == 0ul) {
                populations_[level] = std::max(
                    config_.min_population,
                    std::min(config_.max_population,
                             config_.reference_population));
                ++unobservedFallbackLevels_;
            } else {
                populations_[level] = adaptive_level_population_from_counts(
                    successes_[level], trials_[level], config_);
            }
        }
        totalReferenceBudget_ = 0.0;
        productionBudgetPerRun_ = 0.0;
        predictedProductionCost_ = 0.0;
        plannerMode_ = "success_extinction";
        fallbackReason_ = unobservedFallbackLevels_ > 0ul
            ? "success_extinction_reference_fallback"
            : "success_extinction";
        finalized_ = true;
    }

    void finalize_budgeted(const size_t levelCount,
                           const size_t productionRuns)
    {
        populations_.assign(levelCount, config_.reference_population);
        unobservedFallbackLevels_ = 0ul;
        plannerMode_ = "budgeted";
        if (trials_.size() < levelCount) {
            trials_.resize(levelCount, 0ul);
            retained_.resize(levelCount, 0ul);
            successes_.resize(levelCount, 0ul);
            rareOnlySuccesses_.resize(levelCount, 0ul);
            eventTransitions_.resize(levelCount, 0ul);
            wallSeconds_.resize(levelCount, 0.0);
        }
        std::vector<double> q(levelCount, 0.0);
        std::vector<double> c(levelCount, 0.0);
        std::vector<double> a(levelCount, 0.0);
        std::vector<double> observedC;
        std::vector<double> observedQ;
        calibrationEventTransitions_ = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            calibrationEventTransitions_ +=
                static_cast<double>(eventTransitions_[level]);
            if (trials_[level] > 0ul) {
                q[level] = adaptive_level_retention_probability(
                    retained_[level], trials_[level], config_);
                c[level] = static_cast<double>(eventTransitions_[level]) /
                           static_cast<double>(trials_[level]);
                if (c[level] <= 0.0 || !std::isfinite(c[level]))
                    c[level] = 1.0;
                observedQ.push_back(q[level]);
                observedC.push_back(c[level]);
            }
        }
        if (observedQ.empty()) {
            fallbackReason_ = "no_valid_observation";
            totalReferenceBudget_ =
                static_cast<double>(config_.reference_population) *
                static_cast<double>(levelCount);
            productionBudgetPerRun_ = totalReferenceBudget_;
            predictedProductionCost_ = totalReferenceBudget_;
            finalized_ = true;
            return;
        }
        std::sort(observedC.begin(), observedC.end());
        const double defaultQ =
            *std::min_element(observedQ.begin(), observedQ.end());
        const double defaultC = observedC[observedC.size() / 2ul];
        double sumC = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            if (trials_[level] == 0ul) {
                q[level] = defaultQ;
                c[level] = defaultC;
            }
            q[level] = std::min(1.0 - 1e-12, std::max(1e-12, q[level]));
            c[level] = std::max(1e-12, c[level]);
            a[level] = (1.0 - q[level]) / q[level];
            sumC += c[level];
        }
        totalReferenceBudget_ =
            static_cast<double>(std::max<size_t>(1ul, productionRuns)) *
            static_cast<double>(config_.reference_population) * sumC;
        productionBudgetPerRun_ =
            (totalReferenceBudget_ - calibrationEventTransitions_) /
            static_cast<double>(std::max<size_t>(1ul, productionRuns));
        const double minCost =
            static_cast<double>(config_.min_population) * sumC;
        if (productionBudgetPerRun_ + 1e-9 < minCost) {
            throw std::runtime_error(
                "budget_infeasible: ALP calibration cost leaves less than the minimum per-level production budget");
        }

        auto clippedCost = [&](const double lambda) {
            double cost = 0.0;
            for (size_t level = 0ul; level < levelCount; ++level) {
                const double raw =
                    lambda * std::sqrt(a[level] / c[level]);
                const double n = std::min<double>(
                    static_cast<double>(config_.max_population),
                    std::max<double>(
                        static_cast<double>(config_.min_population), raw));
                cost += c[level] * n;
            }
            return cost;
        };
        double lo = 0.0;
        double hi = 1.0;
        while (clippedCost(hi) < productionBudgetPerRun_ &&
               hi < std::numeric_limits<double>::max() / 4.0)
            hi *= 2.0;
        for (int iter = 0; iter < 80; ++iter) {
            const double mid = 0.5 * (lo + hi);
            if (clippedCost(mid) <= productionBudgetPerRun_)
                lo = mid;
            else
                hi = mid;
        }
        predictedProductionCost_ = 0.0;
        for (size_t level = 0ul; level < levelCount; ++level) {
            const double raw = lo * std::sqrt(a[level] / c[level]);
            const double clipped = std::min<double>(
                static_cast<double>(config_.max_population),
                std::max<double>(
                    static_cast<double>(config_.min_population), raw));
            populations_[level] = static_cast<size_t>(std::floor(clipped));
            populations_[level] = std::max(config_.min_population,
                std::min(config_.max_population, populations_[level]));
            predictedProductionCost_ +=
                c[level] * static_cast<double>(populations_[level]);
        }
        while (predictedProductionCost_ > productionBudgetPerRun_ + 1e-9) {
            size_t best = levelCount;
            double bestLoss = std::numeric_limits<double>::infinity();
            for (size_t level = 0ul; level < levelCount; ++level) {
                if (populations_[level] <= config_.min_population)
                    continue;
                const double loss = a[level] /
                    (static_cast<double>(populations_[level]) *
                     static_cast<double>(populations_[level] - 1ul));
                if (loss < bestLoss) {
                    bestLoss = loss;
                    best = level;
                }
            }
            if (best == levelCount)
                break;
            populations_[best]--;
            predictedProductionCost_ -= c[best];
        }
        while (true) {
            size_t best = levelCount;
            double bestGainPerCost = 0.0;
            for (size_t level = 0ul; level < levelCount; ++level) {
                if (populations_[level] >= config_.max_population)
                    continue;
                if (predictedProductionCost_ + c[level] >
                    productionBudgetPerRun_ + 1e-9)
                    continue;
                const double n = static_cast<double>(populations_[level]);
                const double gain = a[level] / (n * (n + 1.0));
                const double gainPerCost = gain / c[level];
                if (gainPerCost > bestGainPerCost) {
                    bestGainPerCost = gainPerCost;
                    best = level;
                }
            }
            if (best == levelCount)
                break;
            populations_[best]++;
            predictedProductionCost_ += c[best];
        }
        finalized_ = true;
    }

    bool finalized() const noexcept
    {
        return finalized_;
    }

    size_t level_count() const noexcept
    {
        return populations_.size();
    }

    size_t trials(const size_t level) const
    {
        return level < trials_.size() ? trials_[level] : 0ul;
    }

    size_t retained(const size_t level) const
    {
        return level < retained_.size() ? retained_[level] : 0ul;
    }

    size_t successes(const size_t level) const
    {
        return level < successes_.size() ? successes_[level] : 0ul;
    }

    size_t rare_only_successes(const size_t level) const
    {
        return level < rareOnlySuccesses_.size()
               ? rareOnlySuccesses_[level] : 0ul;
    }

    size_t event_transitions(const size_t level) const
    {
        return level < eventTransitions_.size()
               ? eventTransitions_[level] : 0ul;
    }

    double wall_seconds(const size_t level) const
    {
        return level < wallSeconds_.size() ? wallSeconds_[level] : 0.0;
    }

    double probability(const size_t level) const
    {
        return adaptive_level_retention_probability(
            retained(level), trials(level), config_);
    }

    double probability_success(const size_t level) const
    {
        return adaptive_level_retention_probability(
            successes(level), trials(level), config_);
    }

    size_t population(const size_t level) const
    {
        if (!finalized_)
            throw std::logic_error(
                "adaptive population plan has not been finalized");
        return level < populations_.size()
               ? populations_[level]
               : config_.max_population;
    }

    const AdaptiveLevelPopulationConfig& config() const noexcept
    {
        return config_;
    }

    double total_reference_budget() const noexcept
    {
        return totalReferenceBudget_;
    }

    double calibration_event_transitions() const noexcept
    {
        return calibrationEventTransitions_;
    }

    double production_budget_per_run() const noexcept
    {
        return productionBudgetPerRun_;
    }

    double predicted_production_cost() const noexcept
    {
        return predictedProductionCost_;
    }

    size_t unobserved_fallback_levels() const noexcept
    {
        return unobservedFallbackLevels_;
    }

    const std::string& planner_mode() const noexcept
    {
        return plannerMode_;
    }

    const std::string& fallback_reason() const noexcept
    {
        return fallbackReason_;
    }
};

} // namespace fig

#endif // ADAPTIVELEVELPOPULATION_H
