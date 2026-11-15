#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
namespace EchoTrailModel {
struct Sample { double x,y,z,time; };
inline double DistanceSquared(const Sample& a,const Sample& b) {
    return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z);
}
inline void Prune(std::deque<Sample>& samples,double now,double lifetime) {
    while(!samples.empty() && now-samples.front().time>lifetime) samples.pop_front();
}
inline void Append(std::deque<Sample>& samples,Sample sample,double lifetime) {
    if(!std::isfinite(sample.x)||!std::isfinite(sample.y)||!std::isfinite(sample.z)||!std::isfinite(sample.time)) return;
    if(!samples.empty() && (sample.time<samples.back().time || DistanceSquared(sample,samples.back())>64)) samples.clear();
    Prune(samples,sample.time,lifetime);
    if(!samples.empty() && (sample.time-samples.back().time<.05 || DistanceSquared(sample,samples.back())<.0009)) return;
    samples.push_back(sample);
    if(samples.size()>128) samples.pop_front();
}
inline float Opacity(double now,double time,double lifetime) {
    return static_cast<float>(std::pow(std::clamp(1-(now-time)/lifetime,0.0,1.0),1.8));
}
}
