#pragma once
#include <string>
#include <string_view>
#include <charconv>
#include <cstdio>
namespace OreSim {
inline char seedInput[64] = "";
inline long long manualSeed = 0;
inline bool hasManualSeed = false;
inline std::string seedError;
inline bool ParseSeed(std::string_view text, long long& result) {
    auto first=text.find_first_not_of(" \t\r\n");
    if(first==std::string_view::npos) return false;
    text=text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
    if(text.front()=='+') { text.remove_prefix(1); if(text.empty() || text.front()=='-' || text.front()=='+') return false; }
    auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    return parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size();
}
inline void SetProfileSeed(bool present, long long seed) {
    hasManualSeed=present;
    manualSeed=present ? seed : 0;
    seedError.clear();
    if(present) std::snprintf(seedInput,sizeof(seedInput),"%lld",seed);
    else seedInput[0]='\0';
}
}
