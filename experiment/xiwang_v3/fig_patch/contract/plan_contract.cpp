#include <cassert>
#include <cstdlib>
#include <stdexcept>
#include "XiWangPopulationPlan.h"

static void set_env(const char* name, const char* value)
{
#ifdef _WIN32
    if (_putenv_s(name, value) != 0)
        throw std::runtime_error("putenv failed");
#else
    if (value[0] == '\0') {
        if (unsetenv(name) != 0)
            throw std::runtime_error("unsetenv failed");
    } else if (setenv(name, value, 1) != 0) {
        throw std::runtime_error("setenv failed");
    }
#endif
}

int main()
{
    set_env("BATS_XIWANG_PLAN", "0:24,1:31");
    const fig::XiWangPopulationPlan plan =
        fig::XiWangPopulationPlan::from_environment();
    assert(plan.enabled());
    assert(plan.population(0u, 99u) == 24u);
    assert(plan.population(2u, 99u) == 99u);
    set_env("BATS_XIWANG_PLAN", "3:7");
    const fig::XiWangPopulationPlan small =
        fig::XiWangPopulationPlan::from_environment();
    assert(small.population(3u, 99u) == 7u);
    assert(small.population(4u, 99u) == 99u);
    set_env("BATS_XIWANG_DIAGNOSTICS", "1");
    set_env("BATS_XIWANG_DIAGNOSTIC_ROOTS", "256");
    assert(fig::xiwang_diagnostics_enabled());
    assert(fig::xiwang_diagnostic_root_limit() == 256u);
    return 0;
}
