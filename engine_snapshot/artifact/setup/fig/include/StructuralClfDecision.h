#ifndef STRUCTURALCLFDECISION_H
#define STRUCTURALCLFDECISION_H

#include <cstddef>
#include <string>
#include <unordered_set>

namespace fig
{

struct BatsStructuralClfDecision
{
    std::size_t structural_signature_count;
    std::size_t modal_signature_count;
    std::size_t discarded_signature_count;
    std::size_t novel_discarded_signature_count;
    bool structural_opportunity;
    bool activated;
};

inline BatsStructuralClfDecision
bats_structural_clf_decision(const std::unordered_set<std::string>& allSignatures,
                             const std::unordered_set<std::string>& modalSignatures,
                             const std::unordered_set<std::string>& discardedSignatures)
{
    std::size_t novel = 0u;
    for (const auto& signature: discardedSignatures)
        if (modalSignatures.find(signature) == modalSignatures.end())
            novel++;
    return {
        allSignatures.size(),
        modalSignatures.size(),
        discardedSignatures.size(),
        novel,
        novel > 0u,
        novel > 0u,
    };
}

} // namespace fig

#endif // STRUCTURALCLFDECISION_H
