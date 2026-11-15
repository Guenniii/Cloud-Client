#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace ThemeShareCode {
struct Design {std::string preset;std::array<unsigned,14> values;bool operator==(const Design&)const=default;};
inline std::uint32_t Checksum(std::string_view text) {
 std::uint32_t hash=2166136261u;for(unsigned char c:text){hash^=c;hash*=16777619u;}return hash;
}
inline bool Valid(const Design& design) {
 if(design.preset.empty() || design.preset.size()>48 || design.preset.find_first_of("|\r\n\t")!=std::string::npos)return false;
 const auto& v=design.values;
 for(int i:{0,2,6,9,11})if(v[i]>1)return false;
 for(int i:{1,3,7,12,13})if(v[i]>0xFFFFFF)return false;
 return v[4]<=200 && v[5]<=200 && v[8]<=50 && v[10]>=2 && v[10]<=20;
}
inline std::string Encode(const Design& design) {
 if(!Valid(design))return {};
 std::string code="CC-THEME1|"+design.preset;
 for(int i=0;i<14;++i){
  char value[16];if(i==1||i==3||i==7||i==12||i==13)std::snprintf(value,sizeof(value),"%06X",design.values[i]);
  else std::snprintf(value,sizeof(value),"%u",design.values[i]);
  code+='|';code+=value;
 }
 char checksum[12];std::snprintf(checksum,sizeof(checksum),"|%08X",Checksum(code));return code+checksum;
}
inline std::optional<Design> Decode(std::string_view code) {
 if(code.size()>512)return {};
 const auto begin=code.find_first_not_of(" \t\r\n"),end=code.find_last_not_of(" \t\r\n");
 if(begin==std::string_view::npos)return {};
 code=code.substr(begin,end-begin+1);
 std::vector<std::string_view> parts;size_t at=0;
 while(true){const auto separator=code.find('|',at);parts.push_back(code.substr(at,separator==code.npos?code.size()-at:separator-at));if(separator==code.npos)break;at=separator+1;if(parts.size()>17)return {};}
 if(parts.size()!=17 || parts[0]!="CC-THEME1" || parts[16].size()!=8)return {};
 auto number=[](std::string_view text,unsigned& value,int base){auto parsed=std::from_chars(text.data(),text.data()+text.size(),value,base);return !text.empty() && parsed.ec==std::errc{} && parsed.ptr==text.data()+text.size();};
 unsigned checksum=0;if(!number(parts[16],checksum,16) || checksum!=Checksum(code.substr(0,code.rfind('|'))))return {};
 Design design;design.preset=std::string(parts[1]);
 for(int i=0;i<14;++i){const bool color=i==1||i==3||i==7||i==12||i==13;if(color && parts[i+2].size()!=6)return {};if(!number(parts[i+2],design.values[i],color?16:10))return {};}
 return Valid(design)?std::optional<Design>(design):std::nullopt;
}
}
