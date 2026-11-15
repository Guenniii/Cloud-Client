#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <charconv>
#include <string_view>
namespace ColorRGB {
inline bool ParseHex(std::string_view text,int& result) {
    if(!text.empty() && text.front()=='#')text.remove_prefix(1);
    if(text.size()!=6)return false;
    int value=0;auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,16);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<0 || value>0xFFFFFF)return false;
    result=value;return true;
}
constexpr bool Valid(int rgb) {return rgb>=0 && rgb<=0xFFFFFF;}
inline std::array<float,3> Unpack(int rgb) {return {((rgb>>16)&255)/255.f,((rgb>>8)&255)/255.f,(rgb&255)/255.f};}
inline int Pack(const std::array<float,3>& channels) {
    int rgb=0;
    for(float value:channels) {
        if(!std::isfinite(value))value=0;
        rgb=(rgb<<8)|static_cast<int>(std::clamp(value,0.f,1.f)*255.f+.5f);
    }
    return rgb;
}
}
