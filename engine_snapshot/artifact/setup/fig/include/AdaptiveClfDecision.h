#ifndef ADAPTIVECLFDECISION_H
#define ADAPTIVECLFDECISION_H

#include <cstddef>

namespace fig
{

struct BatsAdaptiveClfDecision
{
    double discard_rate;
    double overshoot_rate;
    double risk;
    bool activated;
    bool rare_only_forced;
};

inline BatsAdaptiveClfDecision
bats_adaptive_clf_decision(const std::size_t successCount,
                           const std::size_t modalCount,
                           const std::size_t overshootCount,
                           const std::size_t rareOnlySuccessCount,
                           const double threshold)
{
    const double discardRate = successCount > 0u
        ? static_cast<double>(successCount > modalCount
                              ? successCount - modalCount : 0u) / successCount
        : 0.0;
    const double overshootRate = successCount > 0u
        ? static_cast<double>(overshootCount) / successCount
        : 0.0;
    const double risk = 0.6 * discardRate + 0.4 * overshootRate;
    const bool rareOnlyForced = rareOnlySuccessCount > 0u;
    return {
        discardRate,
        overshootRate,
        risk,
        rareOnlyForced || risk >= threshold,
        rareOnlyForced,
    };
}

inline bool
bats_use_all_success(const bool allSuccessConfigured,
                     const bool adaptiveClfConfigured,
                     const BatsAdaptiveClfDecision& decision)
{
    return allSuccessConfigured
        || (adaptiveClfConfigured && decision.activated);
}

} // namespace fig

#endif // ADAPTIVECLFDECISION_H
