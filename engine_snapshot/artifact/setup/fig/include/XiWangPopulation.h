// Explicit per-level population plan for the pure XiWang controller.
#ifndef XIWANGPOPULATION_H
#define XIWANGPOPULATION_H

#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fig
{

inline std::map<size_t,size_t>
bats_xiwang_plan()
{
    std::map<size_t,size_t> plan;
    const char* raw = std::getenv("BATS_XIWANG_PLAN");
    if (nullptr == raw || raw[0] == '\0')
        return plan;
    std::string text(raw);
    for (auto& ch: text)
        if (ch == ';') ch = ',';
    std::istringstream input(text);
    std::string token;
    while (std::getline(input, token, ',')) {
        const size_t colon = token.find(':');
        if (token.empty() || colon == std::string::npos ||
            token.find(':', colon + 1ul) != std::string::npos)
            throw std::invalid_argument(
                "BATS_XIWANG_PLAN must be level:population pairs");
        try {
            const size_t level = static_cast<size_t>(
                std::stoull(token.substr(0ul, colon)));
            const size_t population = static_cast<size_t>(
                std::stoull(token.substr(colon + 1ul)));
            if (population < 24ul || !plan.emplace(level, population).second)
                throw std::invalid_argument(
                    "BATS_XIWANG_PLAN populations must be >=24 and levels unique");
        } catch (const std::invalid_argument&) {
            throw;
        } catch (...) {
            throw std::invalid_argument(
                "BATS_XIWANG_PLAN must be level:population pairs");
        }
    }
    return plan;
}

inline size_t
bats_xiwang_population(const std::map<size_t,size_t>& plan,
                        const size_t level,
                        const size_t fallback)
{
    const auto found = plan.find(level);
    return found == plan.end() ? fallback : found->second;
}

} // namespace fig

#endif // XIWANGPOPULATION_H
