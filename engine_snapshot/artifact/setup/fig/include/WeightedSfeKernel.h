//==============================================================================
//
//  WeightedSfeKernel.h
//
//  Stage-0 local-mass accounting for modal Fixed Effort splitting.
//
//==============================================================================

#ifndef WEIGHTEDSFEKERNEL_H
#define WEIGHTEDSFEKERNEL_H

#include <cmath>
#include <cstddef>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>


namespace fig
{

typedef size_t WeightedSfeLevel;


inline double
weighted_sfe_stage0_modal_tolerance()
{
    return 1e-12;
}


inline bool
weighted_sfe_stage0_eligible(const bool thresholdBuild,
                             const std::string& poolMode,
                             const bool oracleDiagnosticRequested)
{
    return !thresholdBuild &&
           poolMode == "modal" &&
           !oracleDiagnosticRequested;
}

struct WeightedModalDecision
{
    bool valid;
    WeightedSfeLevel level;
    double modal_mass;
    bool tolerance_tie;
    double largest_mass;
    double second_largest_mass;
    double mass_gap;

    WeightedModalDecision() :
        valid(false),
        level(0ul),
        modal_mass(0.0),
        tolerance_tie(false),
        largest_mass(0.0),
        second_largest_mass(0.0),
        mass_gap(0.0)
    {}
};


inline double
uniform_local_child_mass(const size_t childCount)
{
    if (childCount == 0ul)
        throw std::invalid_argument(
            "weighted SFE child count must be positive");
    return 1.0 / static_cast<double>(childCount);
}


inline double
normalised_local_mass(const double mass, const double totalMass)
{
    if (!std::isfinite(mass) || mass < 0.0)
        throw std::invalid_argument(
            "weighted SFE mass must be finite and non-negative");
    if (!std::isfinite(totalMass) || totalMass <= 0.0)
        throw std::invalid_argument(
            "weighted SFE normalisation mass must be finite and positive");
    return mass / totalMass;
}


class WeightedSfeMassAccumulator
{
public:
    typedef std::map<WeightedSfeLevel,double> ReachedMass;

private:
    double totalMass_;
    double advancedMass_;
    double rareOnlyMass_;
    double successMass_;
    ReachedMass reachedMass_;

public:
    WeightedSfeMassAccumulator() :
        totalMass_(0.0),
        advancedMass_(0.0),
        rareOnlyMass_(0.0),
        successMass_(0.0),
        reachedMass_()
    {}

    void observe(const double mass,
                 const bool reachedUpper,
                 const bool reachedRare,
                 const WeightedSfeLevel reachedLevel)
    {
        if (!std::isfinite(mass) || mass < 0.0)
            throw std::invalid_argument(
                "weighted SFE observation mass must be finite and non-negative");

        totalMass_ += mass;
        if (reachedUpper) {
            advancedMass_ += mass;
            reachedMass_[reachedLevel] += mass;
        }
        if (reachedRare && !reachedUpper)
            rareOnlyMass_ += mass;
        if (reachedUpper || reachedRare)
            successMass_ += mass;
    }

    WeightedModalDecision modal(const double tolerance) const
    {
        if (!std::isfinite(tolerance) || tolerance < 0.0)
            throw std::invalid_argument(
                "weighted SFE modal tolerance must be finite and non-negative");

        WeightedModalDecision result;
        if (reachedMass_.empty())
            return result;

        double largest = -1.0;
        double secondLargest = -1.0;
        for (const auto& item: reachedMass_) {
            const double mass = item.second;
            if (mass > largest) {
                secondLargest = largest;
                largest = mass;
            } else if (mass > secondLargest) {
                secondLargest = mass;
            }
        }

        // ReachedMass is ordered by level. Select the first (smallest) level
        // whose mass is within tolerance of the numerical maximum.
        auto best = reachedMass_.begin();
        while (best != reachedMass_.end() &&
               largest - best->second > tolerance)
            ++best;

        result.valid = true;
        result.level = best->first;
        result.modal_mass = best->second;
        result.largest_mass = largest;
        result.second_largest_mass = secondLargest < 0.0 ? 0.0 : secondLargest;
        result.mass_gap = secondLargest < 0.0 ? largest : largest - secondLargest;
        result.tolerance_tie = reachedMass_.size() > 1ul &&
                               result.mass_gap <= tolerance;
        return result;
    }

    double total_mass() const noexcept { return totalMass_; }
    double advanced_mass() const noexcept { return advancedMass_; }
    double rare_only_mass() const noexcept { return rareOnlyMass_; }
    double success_mass() const noexcept { return successMass_; }
    const ReachedMass& reached_mass() const noexcept { return reachedMass_; }
};

} // namespace fig

#endif // WEIGHTEDSFEKERNEL_H
