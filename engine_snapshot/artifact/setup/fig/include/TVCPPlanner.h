#ifndef TVCPPLANNER_H
#define TVCPPLANNER_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace fig {

enum TvcpStopReason {
    TVCP_GROWTH_CAP,
    TVCP_CONSERVATIVE_GAIN_GATE,
    TVCP_ALL_UPPER_BOUNDS
};

struct TvcpPlan
{
    bool valid;
    std::vector<size_t> population;
    double predicted_variance;
    size_t additions;
    double lambda_threshold;
    TvcpStopReason stop_reason;

    TvcpPlan() : valid(false), population(),
        predicted_variance(std::numeric_limits<double>::infinity()),
        additions(0ul), lambda_threshold(0.0),
        stop_reason(TVCP_GROWTH_CAP) {}
};

// Exact greedy solution of PDF equations (24)--(29).  Event cost is
// intentionally absent: it is a reported outcome, never an allocation input.
inline TvcpPlan tvcp_greedy_plan(
    const std::vector<double>& a_mean,
    const std::vector<double>& a_safe,
    const std::vector<size_t>& current,
    const std::vector<size_t>& safe_lower,
    const std::vector<size_t>& upper,
    size_t growth_cap,
    double eta)
{
    const size_t levels = a_mean.size();
    if (levels == 0ul || a_safe.size() != levels || current.size() != levels ||
        safe_lower.size() != levels || upper.size() != levels ||
        !std::isfinite(eta) || eta < 0.0)
        throw std::invalid_argument("invalid TVCP planner dimensions or eta");
    TvcpPlan plan;
    plan.population.resize(levels);
    for (size_t level = 0ul; level < levels; ++level) {
        if (!std::isfinite(a_mean[level]) || a_mean[level] < 0.0 ||
            !std::isfinite(a_safe[level]) || a_safe[level] < 0.0 ||
            current[level] == 0ul || safe_lower[level] == 0ul ||
            upper[level] < safe_lower[level] || current[level] > upper[level])
            throw std::invalid_argument("invalid TVCP coefficient or population bound");
        plan.population[level] = std::max(current[level], safe_lower[level]);
        plan.additions += plan.population[level] - current[level];
        plan.lambda_threshold += a_mean[level] /
            static_cast<double>(current[level]);
    }
    if (plan.additions > growth_cap)
        throw std::invalid_argument("TVCP safety lower bounds exceed growth cap");
    plan.lambda_threshold *= eta;
    while (plan.additions < growth_cap) {
        size_t best = levels;
        double best_gain = -1.0;
        for (size_t level = 0ul; level < levels; ++level) {
            const size_t n = plan.population[level];
            if (n >= upper[level]) continue;
            const double denominator = static_cast<double>(n) *
                                       static_cast<double>(n + 1ul);
            const double safe_gain = a_safe[level] / denominator;
            if (safe_gain <= plan.lambda_threshold) continue;
            const double gain = a_mean[level] / denominator;
            if (gain > best_gain) { best = level; best_gain = gain; }
        }
        if (best == levels) {
            plan.stop_reason = TVCP_CONSERVATIVE_GAIN_GATE;
            break;
        }
        ++plan.population[best];
        ++plan.additions;
    }
    bool all_upper = true;
    plan.predicted_variance = 0.0;
    for (size_t level = 0ul; level < levels; ++level) {
        plan.predicted_variance += a_mean[level] /
            static_cast<double>(plan.population[level]);
        all_upper = all_upper && plan.population[level] >= upper[level];
    }
    if (plan.additions < growth_cap && all_upper)
        plan.stop_reason = TVCP_ALL_UPPER_BOUNDS;
    plan.valid = true;
    return plan;
}

} // namespace fig
#endif
