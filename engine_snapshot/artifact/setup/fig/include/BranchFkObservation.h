//==============================================================================
// Opt-in branch-preserving diagnostics for SFE experiments.
// This header contains no global state and does not change the formal estimator.
//==============================================================================

#ifndef BRANCHFKOBSERVATION_H
#define BRANCHFKOBSERVATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fig
{

inline double
branch_fk_diagnostic_offset(const std::string& invocation_id,
                            size_t level_step)
{
    unsigned long long hash = 1469598103934665603ull;
    for (const unsigned char ch: invocation_id) {
        hash ^= static_cast<unsigned long long>(ch);
        hash *= 1099511628211ull;
    }
    hash ^= static_cast<unsigned long long>(level_step);
    hash *= 1099511628211ull;
    const unsigned long long bucket = hash % 1000003ull;
    return (static_cast<double>(bucket) + 0.5) / 1000003.0;
}

struct BranchFkObservationConfig
{
    bool enabled;
    size_t keep_k;

    BranchFkObservationConfig() : enabled(false), keep_k(2ul) {}
};

inline bool
branch_fk_config_valid(const BranchFkObservationConfig& config)
{
    return !config.enabled || config.keep_k > 0ul;
}

struct BranchFkCandidate
{
    std::string invocation_id;
    size_t batch_step;
    std::string parent_id;
    size_t parent_index;
    size_t candidate_index;
    unsigned reached_level;
    double weight;
    // Explicit mass decomposition for the production estimator.  `weight`
    // remains the legacy diagnostic alias; production code must validate and
    // use candidate_mass == parent_mass * local_child_mass.
    double parent_mass;
    double local_child_mass;
    double candidate_mass;
    bool rare;
    bool jump;
    size_t event_count;
    std::string factor_sequence;
    std::string path_signature;

    BranchFkCandidate(size_t parent = 0ul,
                      size_t candidate = 0ul,
                      unsigned reached = 0u,
                      double mass = 0.0,
                      bool reached_rare = false,
                      size_t events = 0ul,
                      const std::string& factors = "-") :
        invocation_id(), batch_step(0ul), parent_id(),
        parent_index(parent), candidate_index(candidate),
        reached_level(reached), weight(mass), parent_mass(mass),
        local_child_mass(1.0), candidate_mass(mass), rare(reached_rare),
        jump(false), event_count(events), factor_sequence(factors),
        path_signature("-") {}

    BranchFkCandidate(const std::string& invocation,
                      size_t step,
                      const std::string& parent_identifier,
                      size_t parent,
                      size_t candidate,
                      unsigned reached,
                      double mass,
                      bool reached_rare,
                      bool jumped,
                      size_t events,
                      const std::string& factors,
                      const std::string& signature) :
        invocation_id(invocation), batch_step(step), parent_id(parent_identifier),
        parent_index(parent), candidate_index(candidate),
        reached_level(reached), weight(mass), parent_mass(mass),
        local_child_mass(1.0), candidate_mass(mass), rare(reached_rare), jump(jumped),
        event_count(events), factor_sequence(factors), path_signature(signature) {}

    std::string candidate_id() const
    {
        return invocation_id + ":" + std::to_string(batch_step) + ":" +
               std::to_string(candidate_index);
    }

    std::string lineage_signature() const
    {
        return parent_id + ">" + candidate_id() + "@" +
               std::to_string(reached_level) + "#" + path_signature;
    }
};

inline void
branch_fk_validate_candidate_mass(const BranchFkCandidate& c,
                                  const double tolerance = 1e-12)
{
    if (!std::isfinite(c.parent_mass) || c.parent_mass < 0.0 ||
        !std::isfinite(c.local_child_mass) || c.local_child_mass < 0.0 ||
        !std::isfinite(c.candidate_mass) || c.candidate_mass < 0.0)
        throw std::invalid_argument("branch FK candidate mass must be finite and non-negative");
    const double expected = c.parent_mass * c.local_child_mass;
    const double scale = std::max(1.0, std::fabs(expected));
    if (std::fabs(c.candidate_mass - expected) > tolerance * scale)
        throw std::invalid_argument("branch FK candidate mass decomposition mismatch");
}

struct BranchFkSample
{
    size_t parent_index;
    size_t candidate_index;
    double inclusion_probability;
    double horvitz_thompson_weight;
};

struct BranchFkPreservedRecord
{
    std::string candidate_id;
    std::string parent_id;
    std::string lineage_signature;
    bool snapshot_required;

    BranchFkPreservedRecord() : candidate_id(), parent_id(),
        lineage_signature(), snapshot_required(true) {}
};

struct BranchFkLineagePoolEntry
{
    std::string candidate_id;
    unsigned reached_level;

    BranchFkLineagePoolEntry(const std::string& id = "-",
                             unsigned reached = 0u) :
        candidate_id(id), reached_level(reached) {}
};

struct BranchFkLevelSummary
{
    size_t candidate_count;
    size_t modal_count;
    size_t rare_count;
    size_t event_count;
    double candidate_weight;
    double modal_weight;
    double discarded_weight;
    double weight_closure_error;

    BranchFkLevelSummary() : candidate_count(0ul), modal_count(0ul),
        rare_count(0ul), event_count(0ul), candidate_weight(0.0),
        modal_weight(0.0), discarded_weight(0.0),
        weight_closure_error(0.0) {}
};

inline BranchFkLevelSummary
branch_fk_summarise(const std::vector<BranchFkCandidate>& candidates,
                    unsigned modal_level)
{
    BranchFkLevelSummary summary;
    summary.candidate_count = candidates.size();
    for (const auto& candidate: candidates) {
        summary.event_count += candidate.event_count;
        summary.candidate_weight += candidate.weight;
        summary.rare_count += candidate.rare ? 1ul : 0ul;
        if (candidate.reached_level == modal_level) {
            ++summary.modal_count;
            summary.modal_weight += candidate.weight;
        } else {
            summary.discarded_weight += candidate.weight;
        }
    }
    summary.weight_closure_error = std::fabs(
        summary.candidate_weight - summary.modal_weight -
        summary.discarded_weight);
    return summary;
}

// Fixed-offset systematic PPS. The caller supplies offset in [0,1), so this
// helper consumes no simulator RNG and is reproducible independently of FIG.
inline std::vector<BranchFkSample>
branch_fk_systematic_pps(const std::vector<BranchFkCandidate>& candidates,
                         size_t keep_k,
                         double offset_fraction)
{
    std::vector<BranchFkSample> selected;
    if (candidates.empty() || keep_k == 0ul)
        return selected;
    keep_k = std::min(keep_k, candidates.size());
    double total = 0.0;
    for (const auto& candidate: candidates)
        if (std::isfinite(candidate.weight) && candidate.weight > 0.0)
            total += candidate.weight;
    if (!(total > 0.0))
        return selected;

    offset_fraction -= std::floor(offset_fraction);
    std::vector<bool> used(candidates.size(), false);
    double remaining_total = total;
    for (size_t draw = 0ul; draw < keep_k; ++draw) {
        const double fraction = std::fmod(
            offset_fraction + static_cast<double>(draw) * 0.6180339887498949,
            1.0);
        const double target = fraction * remaining_total;
        size_t index = candidates.size();
        double cumulative = 0.0;
        for (size_t item = 0ul; item < candidates.size(); ++item) {
            if (used[item])
                continue;
            cumulative += std::max(0.0, candidates[item].weight);
            if (index == candidates.size() && cumulative > target)
                index = item;
        }
        if (index == candidates.size())
            for (size_t item = candidates.size(); item-- > 0ul;)
                if (!used[item]) { index = item; break; }
        if (index == candidates.size())
            break;
        used[index] = true;
        const double probability = std::min(
            1.0, static_cast<double>(keep_k) *
                 std::max(0.0, candidates[index].weight) / total);
        BranchFkSample sample;
        sample.parent_index = candidates[index].parent_index;
        sample.candidate_index = candidates[index].candidate_index;
        sample.inclusion_probability = probability;
        sample.horvitz_thompson_weight = probability > 0.0
            ? candidates[index].weight / probability : 0.0;
        selected.push_back(sample);
        remaining_total -= std::max(0.0, candidates[index].weight);
        if (!(remaining_total > 0.0))
            break;
    }
    return selected;
}

inline std::vector<BranchFkPreservedRecord>
branch_fk_preserved_records(const std::vector<BranchFkCandidate>& candidates,
                            const std::vector<BranchFkSample>& samples)
{
    std::vector<BranchFkPreservedRecord> records;
    records.reserve(samples.size());
    for (const auto& sample: samples) {
        if (sample.candidate_index >= candidates.size())
            continue;
        const auto& candidate = candidates[sample.candidate_index];
        BranchFkPreservedRecord record;
        record.candidate_id = candidate.candidate_id();
        record.parent_id = candidate.parent_id;
        record.lineage_signature = candidate.lineage_signature();
        records.push_back(record);
    }
    return records;
}

inline std::vector<BranchFkLineagePoolEntry>
branch_fk_filter_modal_lineage(
    const std::vector<BranchFkLineagePoolEntry>& entries,
    unsigned modal_level)
{
    std::vector<BranchFkLineagePoolEntry> retained;
    retained.reserve(entries.size());
    for (auto it = entries.rbegin(); it != entries.rend(); ++it)
        if (it->reached_level == modal_level)
            retained.push_back(*it);
    return retained;
}

inline bool
branch_fk_lineage_pool_aligned(
    const std::vector<BranchFkLineagePoolEntry>& entries,
    size_t formal_pool_size,
    std::string& reason)
{
    if (entries.size() != formal_pool_size) {
        reason = "lineage_pool_size_mismatch";
        return false;
    }
    reason = "ok";
    return true;
}

} // namespace fig

#endif // BRANCHFKOBSERVATION_H
