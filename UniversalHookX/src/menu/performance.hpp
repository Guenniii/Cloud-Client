#pragma once
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>

namespace Menu::Performance {
enum Section { Client, OreSim, PlayerESP, Count };
struct Summary { double average=0, peak=0; };
struct Samples {
    std::array<double,120> values{};
    std::size_t size=0, next=0;
    void Add(double milliseconds) {
        if(!std::isfinite(milliseconds) || milliseconds<0) return;
        values[next]=milliseconds;
        next=(next+1)%values.size();
        if(size<values.size()) ++size;
    }
    Summary Read() const {
        Summary result;
        for(std::size_t i=0;i<size;++i) {
            result.average+=values[i];
            if(values[i]>result.peak) result.peak=values[i];
        }
        if(size) result.average/=size;
        return result;
    }
};
// Accessed only from the serialized render path, including the Settings page.
inline std::array<Samples,Count> samples;
inline void Reset() { samples={}; }
class Scope {
    Section section;
    std::chrono::steady_clock::time_point start=std::chrono::steady_clock::now();
public:
    explicit Scope(Section value):section(value) {}
    ~Scope() {
        samples[section].Add(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
