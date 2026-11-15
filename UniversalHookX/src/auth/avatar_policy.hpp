#pragma once
#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>
namespace AvatarPolicy {
inline constexpr size_t MaxBytes=512*1024;
inline std::string EncodeName(std::string_view name) {
    constexpr char hex[]="0123456789ABCDEF";std::string encoded;
    for(unsigned char c:name) {
        if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~')encoded+=c;
        else {encoded+='%';encoded+=hex[c>>4];encoded+=hex[c&15];}
    }
    return encoded;
}
inline bool ValidImage(std::span<const unsigned char> bytes) {
    constexpr std::array<unsigned char,8> signature={137,80,78,71,13,10,26,10};
    constexpr std::array<unsigned char,4> size={0,0,1,0};
    return bytes.size()>=33 && bytes.size()<=MaxBytes && std::equal(signature.begin(),signature.end(),bytes.begin())
        && bytes[8]==0 && bytes[9]==0 && bytes[10]==0 && bytes[11]==13
        && bytes[12]=='I' && bytes[13]=='H' && bytes[14]=='D' && bytes[15]=='R'
        && std::equal(size.begin(),size.end(),bytes.begin()+16) && std::equal(size.begin(),size.end(),bytes.begin()+20)
        && bytes[24]==8 && bytes[25]==6 && bytes[26]==0 && bytes[27]==0 && bytes[28]==0;
}
}
