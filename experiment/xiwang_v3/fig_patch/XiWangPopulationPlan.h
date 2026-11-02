#ifndef XIWANG_POPULATION_PLAN_H
#define XIWANG_POPULATION_PLAN_H

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fig {

inline bool xiwang_diagnostics_enabled()
{
    const char* raw = std::getenv("BATS_XIWANG_DIAGNOSTICS");
    if (raw == nullptr)
        return false;
    const std::string value(raw);
    return !value.empty() && value != "0" && value != "false" && value != "FALSE";
}

inline size_t xiwang_diagnostic_root_limit()
{
    const char* raw = std::getenv("BATS_XIWANG_DIAGNOSTIC_ROOTS");
    if (raw == nullptr || raw[0] == '\0')
        return 256u;
    if (raw[0] == '-')
        throw std::runtime_error("invalid XiWang diagnostic root limit");
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(raw, &end, 10);
    if (errno == ERANGE || end == nullptr || end[0] != '\0' || value == 0u ||
        value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
        throw std::runtime_error("invalid XiWang diagnostic root limit");
    return static_cast<size_t>(value);
}

class XiWangPopulationPlan {
public:
    static XiWangPopulationPlan from_environment()
    {
        const char* raw = std::getenv("BATS_XIWANG_PLAN");
        if (raw == nullptr || raw[0] == '\0')
            return XiWangPopulationPlan();
        XiWangPopulationPlan result;
        result.enabled_ = true;
        std::string text(raw);
        for (std::string::iterator it = text.begin(); it != text.end(); ++it)
            if (*it == ';') *it = ',';
        std::istringstream input(text);
        std::string token;
        while (std::getline(input, token, ',')) {
            const std::string::size_type colon = token.find(':');
            if (token.empty() || colon == std::string::npos ||
                token.find(':', colon + 1u) != std::string::npos)
                throw std::runtime_error(
                    "BATS_XIWANG_PLAN must contain level:population pairs");
            const size_t level = parse_nonnegative(token.substr(0u, colon), "level");
            const size_t population = parse_nonnegative(token.substr(colon + 1u), "population");
            if (population == 0u)
                throw std::runtime_error("XiWang population must be positive");
            if (!result.values_.insert(std::make_pair(level, population)).second)
                throw std::runtime_error("duplicate XiWang level");
        }
        if (result.values_.empty())
            throw std::runtime_error("empty XiWang plan");
        return result;
    }

    bool enabled() const { return enabled_; }

    size_t population(const size_t level, const size_t legacy) const
    {
        if (!enabled_)
            return legacy;
        const std::map<size_t,size_t>::const_iterator found = values_.find(level);
        return found == values_.end() ? legacy : found->second;
    }

private:
    XiWangPopulationPlan() : enabled_(false), values_() {}

    static size_t parse_nonnegative(const std::string& text, const char* field)
    {
        if (text.empty() || text[0] == '-')
            throw std::runtime_error(std::string("invalid XiWang ") + field);
        char* end = nullptr;
        errno = 0;
        const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
        if (errno == ERANGE || end == nullptr || end[0] != '\0' ||
            value > static_cast<unsigned long long>(std::numeric_limits<size_t>::max()))
            throw std::runtime_error(std::string("invalid XiWang ") + field);
        return static_cast<size_t>(value);
    }

    bool enabled_;
    std::map<size_t,size_t> values_;
};

} // namespace fig

#endif
