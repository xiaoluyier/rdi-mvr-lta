#ifndef KBRANCHFRONTIER_H
#define KBRANCHFRONTIER_H

#include <KBranchEstimator.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fig
{

struct KBranchFrontierParticle
{
    unsigned reached_level;
    double propagated_mass;
    std::string parent_id;
    std::string candidate_id;
    size_t offspring_ordinal;
    std::string factor_sequence;
    std::vector<unsigned> lineage_levels;
    size_t event_count;

    KBranchFrontierParticle(unsigned level = 0u,
                            double mass = 0.0,
                            const std::string& parent = "-",
                            const std::string& candidate = "-",
                            size_t ordinal = 0ul,
                            const std::string& factors = "-",
                            const std::vector<unsigned>& lineage = std::vector<unsigned>(),
                            size_t events = 0ul) :
        reached_level(level), propagated_mass(mass), parent_id(parent),
        candidate_id(candidate), offspring_ordinal(ordinal),
        factor_sequence(factors), lineage_levels(lineage), event_count(events)
    {
        if (!std::isfinite(propagated_mass) || propagated_mass < 0.0)
            throw std::invalid_argument("K-branch particle mass must be finite and non-negative");
    }

    // Backward-compatible constructor used by existing frontier tests and
    // callers that do not carry lineage diagnostics.
    KBranchFrontierParticle(unsigned level, double mass,
                            const std::string& parent,
                            const std::string& candidate,
                            size_t ordinal,
                            const std::string& factors,
                            size_t events) :
        KBranchFrontierParticle(level, mass, parent, candidate, ordinal,
                                factors, std::vector<unsigned>(), events) {}
};

class KBranchFrontier
{
    std::map<unsigned, std::vector<KBranchFrontierParticle> > levels_;

public:
    bool empty() const noexcept { return levels_.empty(); }
    size_t size() const noexcept
    {
        size_t result = 0ul;
        for (const auto& item: levels_) result += item.second.size();
        return result;
    }
    unsigned smallest_level() const
    {
        if (levels_.empty()) throw std::logic_error("K-branch frontier is empty");
        return levels_.begin()->first;
    }
    void push(const KBranchFrontierParticle& particle)
    {
        if (particle.propagated_mass > 0.0)
            levels_[particle.reached_level].push_back(particle);
    }
    std::pair<unsigned, std::vector<KBranchFrontierParticle> > pop_smallest()
    {
        if (levels_.empty()) throw std::logic_error("K-branch frontier is empty");
        auto it = levels_.begin();
        std::pair<unsigned, std::vector<KBranchFrontierParticle> > result(
            it->first, std::move(it->second));
        levels_.erase(it);
        return result;
    }
};

inline std::vector<size_t>
kbranch_allocate_continuations(const std::vector<double>& parent_masses,
                               size_t total_children)
{
    std::vector<size_t> counts(parent_masses.size(), 0ul);
    std::vector<size_t> positive;
    double total_mass = 0.0;
    for (size_t i = 0ul; i < parent_masses.size(); ++i) {
        if (!std::isfinite(parent_masses[i]) || parent_masses[i] < 0.0)
            throw std::invalid_argument("invalid K-branch parent mass");
        if (parent_masses[i] > 0.0) {
            positive.push_back(i);
            total_mass += parent_masses[i];
        }
    }
    if (positive.empty()) return counts;
    if (total_children < positive.size())
        throw std::invalid_argument("K-branch effort cannot preserve every positive parent");
    for (const size_t i: positive) counts[i] = 1ul;
    for (size_t child = positive.size(); child < total_children; ++child) {
        size_t best = positive.front();
        double best_score = parent_masses[best] / static_cast<double>(counts[best] + 1ul);
        for (const size_t i: positive) {
            const double score = parent_masses[i] / static_cast<double>(counts[i] + 1ul);
            if (score > best_score) { best = i; best_score = score; }
        }
        counts[best]++;
    }
    return counts;
}

struct KBranchFrameCandidate
{
    size_t candidate_index;
    unsigned reached_level;
    double parent_mass;
    double local_child_mass;
    double candidate_mass;
    bool direct_top;
    bool terminal_dead;
    std::string parent_id;
    std::string candidate_id;
    std::string factor_sequence;
    std::vector<unsigned> lineage_levels;
    size_t event_count;

    KBranchFrameCandidate(size_t index = 0ul,
                          unsigned level = 0u,
                          double mass = 0.0,
                          bool top = false,
                          bool dead = false,
                          const std::string& parent = "-",
                          const std::string& candidate = "-",
                          const std::string& factors = "-",
                          const std::vector<unsigned>& lineage = std::vector<unsigned>(),
                          size_t events = 0ul) :
        candidate_index(index), reached_level(level), parent_mass(mass),
        local_child_mass(1.0), candidate_mass(mass),
        direct_top(top), terminal_dead(dead), parent_id(parent),
        candidate_id(candidate), factor_sequence(factors), lineage_levels(lineage), event_count(events) {}

    KBranchFrameCandidate(size_t index, unsigned level, double mass,
                          bool top, bool dead,
                          const std::string& parent,
                          const std::string& candidate,
                          const std::string& factors,
                          size_t events) :
        KBranchFrameCandidate(index, level, mass, top, dead, parent, candidate,
                              factors, std::vector<unsigned>(), events) {}

    KBranchFrameCandidate(size_t index,
                          unsigned level,
                          double parentMass,
                          double localChildMass,
                          double candidateMass,
                          bool top,
                          bool dead,
                          const std::string& parent,
                          const std::string& candidate,
                          const std::string& factors,
                          const std::vector<unsigned>& lineage,
                          size_t events) :
        candidate_index(index), reached_level(level), parent_mass(parentMass),
        local_child_mass(localChildMass), candidate_mass(candidateMass),
        direct_top(top), terminal_dead(dead), parent_id(parent),
        candidate_id(candidate), factor_sequence(factors), lineage_levels(lineage), event_count(events) {}
};

struct KBranchFrameRetained
{
    size_t candidate_index;
    unsigned reached_level;
    double propagated_mass;
    double inclusion_probability;
    std::string parent_id;
    std::string candidate_id;
    std::string factor_sequence;
    std::vector<unsigned> lineage_levels;
    size_t event_count;
};

struct KBranchFrameResult
{
    std::vector<KBranchFrameRetained> retained;
    double direct_top_mass = 0.0;
    size_t positive_active_count = 0ul;
    size_t effective_k = 0ul;
    double active_input_mass = 0.0;
    double retained_output_mass = 0.0;
};

inline KBranchFrameResult
kbranch_compress_frame(const std::vector<KBranchFrameCandidate>& candidates,
                       size_t requested_k,
                       std::mt19937_64& rng)
{
    if (requested_k == 0ul)
        throw std::invalid_argument("K-branch requested K must be positive");
    KBranchFrameResult result;
    std::vector<BranchFkCandidate> active;
    std::vector<size_t> active_to_frame;
    for (size_t i = 0ul; i < candidates.size(); ++i) {
        const auto& candidate = candidates[i];
        if (!std::isfinite(candidate.candidate_mass) || candidate.candidate_mass < 0.0)
            throw std::invalid_argument("invalid K-branch candidate mass");
        if (!std::isfinite(candidate.parent_mass) || candidate.parent_mass < 0.0 ||
            !std::isfinite(candidate.local_child_mass) || candidate.local_child_mass < 0.0)
            throw std::invalid_argument("invalid K-branch mass decomposition");
        const double expected_mass = candidate.parent_mass * candidate.local_child_mass;
        if (std::fabs(candidate.candidate_mass - expected_mass) >
            1e-12 * std::max(1.0, std::fabs(expected_mass)))
            throw std::invalid_argument("K-branch candidate mass decomposition mismatch");
        if (candidate.direct_top) {
            result.direct_top_mass += candidate.candidate_mass;
            continue;
        }
        if (candidate.terminal_dead || candidate.candidate_mass == 0.0)
            continue;
        BranchFkCandidate estimator_candidate(
            0ul, active.size(), candidate.reached_level,
            candidate.candidate_mass, false, candidate.event_count,
            candidate.factor_sequence);
        estimator_candidate.parent_mass = candidate.parent_mass;
        estimator_candidate.local_child_mass = candidate.local_child_mass;
        estimator_candidate.candidate_mass = candidate.candidate_mass;
        active.push_back(estimator_candidate);
        active_to_frame.push_back(i);
        result.active_input_mass += candidate.candidate_mass;
    }
    result.positive_active_count = active.size();
    result.effective_k = std::min(requested_k, active.size());
    if (active.empty()) return result;
    const auto selected = branch_fk_pairwise_pivotal(active, requested_k, rng);
    result.retained.reserve(selected.size());
    for (const auto& sample: selected) {
        if (sample.candidate_index >= active_to_frame.size())
            throw std::runtime_error("K-branch sample index is out of range");
        const auto& source = candidates[active_to_frame[sample.candidate_index]];
        KBranchFrameRetained retained;
        retained.candidate_index = source.candidate_index;
        retained.reached_level = source.reached_level;
        retained.propagated_mass = sample.horvitz_thompson_weight;
        retained.inclusion_probability = sample.inclusion_probability;
        retained.parent_id = source.parent_id;
        retained.candidate_id = source.candidate_id;
        retained.factor_sequence = source.factor_sequence;
        retained.lineage_levels = source.lineage_levels;
        retained.event_count = source.event_count;
        result.retained_output_mass += retained.propagated_mass;
        result.retained.push_back(retained);
    }
    return result;
}

} // namespace fig

#endif
