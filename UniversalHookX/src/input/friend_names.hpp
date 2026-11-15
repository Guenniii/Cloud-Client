#pragma once
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace FriendNames {
inline bool Valid(std::string_view name) {
    if(name.empty() || name.size()>16)return false;
    for(unsigned char c:name)if(!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='_'))return false;
    return true;
}
inline std::string Lower(std::string_view value) {
    std::string result(value);for(char& c:result)if(c>='A' && c<='Z')c+=32;return result;
}
inline bool Contains(const std::vector<std::string>& names,std::string_view value) {
    const auto lower=Lower(value);return std::any_of(names.begin(),names.end(),[&](const auto& name){return Lower(name)==lower;});
}
inline std::optional<std::vector<std::string>> Parse(std::string_view text) {
    std::vector<std::string> names;std::string part;
    auto finish=[&](){if(part.empty())return true;if(!Valid(part))return false;
        if(!Contains(names,part)){if(names.size()>=128)return false;names.push_back(part);}part.clear();return true;};
    for(char c:text) {if(c==',' || c==';' || c==' ' || c=='\n' || c=='\r' || c=='\t'){if(!finish())return {};}else part+=c;}
    if(!finish())return {};return names;
}
inline std::string Join(const std::vector<std::string>& names) {
    std::string text;for(const auto& name:names){if(!text.empty())text+=", ";text+=name;}return text;
}
}
