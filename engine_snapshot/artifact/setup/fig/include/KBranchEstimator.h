#ifndef KBRANCHESTIMATOR_H
#define KBRANCHESTIMATOR_H

#include <BranchFkObservation.h>
#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <vector>

namespace fig {

inline std::vector<double> branch_fk_capped_pps_probabilities(
    const std::vector<double>& masses, size_t requested_k)
{
    if (requested_k == 0ul) throw std::invalid_argument("branch FK requested K must be positive");
    std::vector<double> pi(masses.size(),0.0); std::vector<size_t> pos;
    for(size_t i=0;i<masses.size();++i){
        if(!std::isfinite(masses[i])||masses[i]<0.0) throw std::invalid_argument("invalid candidate mass");
        if(masses[i]>0.0) pos.push_back(i);
    }
    const size_t k=std::min(requested_k,pos.size());
    if(k==0) return pi;
    if(k==pos.size()){for(auto i:pos)pi[i]=1.0;return pi;}
    double total=0.0; for(auto i:pos)total+=masses[i];
    std::vector<size_t> active=pos;double remaining=static_cast<double>(k);
    while(!active.empty()){
        double active_total=0.0;for(auto i:active)active_total+=masses[i];
        const double scale=remaining/active_total;size_t certain=active.size();
        for(size_t j=0;j<active.size();++j)if(scale*masses[active[j]]>=1.0-1e-12&&(certain==active.size()||masses[active[j]]>masses[active[certain]]))certain=j;
        if(certain==active.size()){for(auto i:active)pi[i]=scale*masses[i];break;}
        pi[active[certain]]=1.0;active.erase(active.begin()+certain);remaining-=1.0;if(remaining<=1e-12)break;
    }
    double sum=0;for(double p:pi)sum+=p;if(std::fabs(sum-k)>1e-9)throw std::runtime_error("PPS closure failure");
    return pi;
}

inline std::vector<BranchFkSample> branch_fk_pairwise_pivotal(
    const std::vector<BranchFkCandidate>& candidates,size_t requested_k,std::mt19937_64& rng)
{
    std::vector<double> masses;for(const auto& c:candidates){branch_fk_validate_candidate_mass(c);masses.push_back(c.candidate_mass);}
    const auto pi=branch_fk_capped_pps_probabilities(masses,requested_k);auto p=pi;
    std::uniform_real_distribution<double> U(0.0,1.0);
    while(true){std::vector<size_t> f;for(size_t i=0;i<p.size();++i)if(p[i]>1e-12&&p[i]<1-1e-12)f.push_back(i);if(f.size()<2)break;size_t a=f[f.size()-2],b=f.back();double x=p[a],y=p[b],s=x+y,u=U(rng);if(s<1.0){if(u<x/s){p[a]=s;p[b]=0;}else{p[a]=0;p[b]=s;}}else{double denom=2-s,prob=denom<=1e-12?1.0:(1-y)/denom;if(u<prob){p[a]=1;p[b]=s-1;}else{p[a]=s-1;p[b]=1;}}if(p[a]<1e-12)p[a]=0;if(p[a]>1-1e-12)p[a]=1;if(p[b]<1e-12)p[b]=0;if(p[b]>1-1e-12)p[b]=1;}
    for(size_t i=0;i<p.size();++i)if(p[i]>0&&p[i]<1)p[i]=(U(rng)<p[i]?1.0:0.0);
    std::vector<BranchFkSample> out;for(size_t i=0;i<p.size();++i)if(p[i]>0.5){BranchFkSample s;s.parent_index=candidates[i].parent_index;s.candidate_index=candidates[i].candidate_index;s.inclusion_probability=pi[i];s.horvitz_thompson_weight=candidates[i].candidate_mass/pi[i];if(!std::isfinite(s.horvitz_thompson_weight))throw std::overflow_error("HT weight overflow");out.push_back(s);}
    size_t positive=0;for(double m:masses)positive+=m>0;const size_t k=std::min(requested_k,positive);if(out.size()!=k)throw std::runtime_error("pivotal sample size mismatch");return out;
}

}
#endif
