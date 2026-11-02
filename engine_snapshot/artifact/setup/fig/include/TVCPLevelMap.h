#ifndef TVCPLEVELMAP_H
#define TVCPLEVELMAP_H
#include <map>
#include <stdexcept>
#include <vector>

namespace fig {
inline size_t tvcp_population_for_level(const std::map<unsigned,size_t>& plan,unsigned reached_level)
{
    const auto found=plan.find(reached_level);
    if(found==plan.end()) throw std::out_of_range("TVCP missing reached level");
    return found->second;
}

inline void tvcp_validate_level_coverage(const std::map<unsigned,size_t>& plan,
    unsigned initial_level,unsigned maximum_level)
{
    if(initial_level>maximum_level)
        throw std::invalid_argument("TVCP initial level exceeds maximum level");
    for(unsigned level=initial_level;;++level){
        if(plan.count(level)!=1ul) throw std::invalid_argument("TVCP plan missing accessible level");
        if(level==maximum_level) break;
    }
    for(const auto& item:plan)
        if(item.first<initial_level||item.first>maximum_level||item.second==0ul)
            throw std::invalid_argument("TVCP plan has out-of-range/zero entry");
}
} // namespace fig
#endif
