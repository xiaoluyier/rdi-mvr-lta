#ifndef TVCPNESTEDESTIMATOR_H
#define TVCPNESTEDESTIMATOR_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace fig {

struct TvcpNestedEstimate
{
    size_t population;
    size_t outer_replicates;
    size_t inner_replicates;
    double between_outer_variance;
    double downstream_noise_variance;
    double nu_raw;
    double nu;
    double a_raw;
    double a;
};

inline double tvcp_sample_variance(const std::vector<double>& values)
{
    if (values.size() < 2ul)
        throw std::invalid_argument("TVCP sample variance needs two values");
    double mean = 0.0;
    for (const double value: values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("TVCP nested return must be finite");
        mean += value;
    }
    mean /= static_cast<double>(values.size());
    double sum = 0.0;
    for (const double value: values) {
        const double delta = value - mean;
        sum += delta * delta;
    }
    return sum / static_cast<double>(values.size() - 1ul);
}

inline TvcpNestedEstimate
tvcp_nested_estimate(size_t population,
                     const std::vector<std::vector<double> >& values)
{
    if (population == 0ul || values.size() < 2ul || values.front().size() < 2ul)
        throw std::invalid_argument("TVCP nested design needs positive n, R>=2, Q>=2");
    const size_t q = values.front().size();
    std::vector<double> means;
    means.reserve(values.size());
    double within = 0.0;
    for (const auto& row: values) {
        if (row.size() != q)
            throw std::invalid_argument("TVCP nested design must be rectangular");
        double mean = 0.0;
        for (const double value: row) {
            if (!std::isfinite(value))
                throw std::invalid_argument("TVCP nested return must be finite");
            mean += value;
        }
        means.push_back(mean / static_cast<double>(q));
        within += tvcp_sample_variance(row);
    }
    within /= static_cast<double>(values.size());
    const double between = tvcp_sample_variance(means);
    const double downstream = within / static_cast<double>(q);
    const double raw = between - downstream;
    const double clipped = std::max(0.0, raw);
    TvcpNestedEstimate result;
    result.population = population;
    result.outer_replicates = values.size();
    result.inner_replicates = q;
    result.between_outer_variance = between;
    result.downstream_noise_variance = downstream;
    result.nu_raw = raw;
    result.nu = clipped;
    result.a_raw = static_cast<double>(population) * raw;
    result.a = static_cast<double>(population) * clipped;
    return result;
}

} // namespace fig
#endif
