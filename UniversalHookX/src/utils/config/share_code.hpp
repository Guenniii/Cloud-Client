#pragma once
#include "../theme/share_code.hpp"
#include <charconv>
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace ConfigShareCode {
inline constexpr size_t MaxPayload=128*1024,MaxCode=180*1024;
struct Record {std::string module,field;int value;bool operator==(const Record&)const=default;};
struct Document {std::vector<Record> records;std::string theme;bool operator==(const Document&)const=default;};
inline bool Name(std::string_view text){
 if(text.empty()||text.size()>128)return false;
 for(unsigned char c:text)if(c<32||c==127)return false;
 return true;
}
inline std::string Base64(std::string_view bytes){
 constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
 std::string result;unsigned value=0;int bits=0;
 for(unsigned char c:bytes){value=(value<<8)|c;bits+=8;while(bits>=6){bits-=6;result+=alphabet[(value>>bits)&63];}}
 if(bits)result+=alphabet[(value<<(6-bits))&63];return result;
}
inline bool Unbase64(std::string_view text,std::string& bytes){
 if(text.size()%4==1)return false;unsigned value=0;int bits=0;
 for(char c:text){int n=c>='A'&&c<='Z'?c-'A':c>='a'&&c<='z'?c-'a'+26:c>='0'&&c<='9'?c-'0'+52:c=='-'?62:c=='_'?63:-1;
  if(n<0)return false;value=(value<<6)|unsigned(n);bits+=6;
  if(bits>=8){bits-=8;bytes+=char((value>>bits)&255);if(bytes.size()>MaxPayload)return false;}
 }
 return !bits || (value&((1u<<bits)-1))==0;
}
inline std::vector<std::string_view> Split(std::string_view text,char delimiter){
 std::vector<std::string_view> parts;size_t at=0;
 while(true){auto next=text.find(delimiter,at);parts.push_back(text.substr(at,next==text.npos?text.size()-at:next-at));if(next==text.npos)break;at=next+1;if(parts.size()>4096)break;}
 return parts;
}
inline std::string Encode(const Document& doc){
 if(doc.records.empty()||doc.records.size()>2048||(!doc.theme.empty()&&!ThemeShareCode::Decode(doc.theme)))return {};
 std::string payload="client-config\t1\n";
 if(!doc.theme.empty())payload+="theme\t"+doc.theme+"\n";
 std::set<std::pair<std::string,std::string>> keys;
 for(const auto& r:doc.records){if(!Name(r.module)||!Name(r.field)||!keys.emplace(r.module,r.field).second)return {};
  payload+="module\t"+r.module+"\t"+r.field+"\t"+std::to_string(r.value)+"\n";
 }
 if(payload.size()>MaxPayload)return {};
 char hash[12];std::snprintf(hash,sizeof(hash),".%08X",ThemeShareCode::Checksum(payload));
 return "CC-CONFIG1."+Base64(payload)+hash;
}
inline bool Decode(std::string_view code,Document& doc,std::string& error){
 auto fail=[&](const char* message){error=message;return false;};
 if(code.size()>MaxCode)return fail("Config code is too large.");
 auto first=code.find_first_not_of(" \t\r\n"),last=code.find_last_not_of(" \t\r\n");
 if(first==code.npos)return fail("Paste a config code or open a .ccfg file first.");
 code=code.substr(first,last-first+1);
 if(!code.starts_with("CC-CONFIG1."))return fail("Unsupported config format or version.");
 auto dot=code.rfind('.');if(dot<=11||code.size()-dot-1!=8)return fail("Incomplete config code.");
 unsigned checksum=0;auto hash=code.substr(dot+1);auto parsed=std::from_chars(hash.data(),hash.data()+hash.size(),checksum,16);
 std::string payload;
 if(parsed.ec!=std::errc{}||parsed.ptr!=hash.data()+hash.size()||!Unbase64(code.substr(11,dot-11),payload)||checksum!=ThemeShareCode::Checksum(payload))return fail("Config code is damaged. Copy it again.");
 auto lines=Split(payload,'\n');if(lines.empty()||lines[0]!="client-config\t1")return fail("Unsupported config payload.");
 Document candidate;std::set<std::pair<std::string,std::string>> keys;bool hasTheme=false;
 for(size_t i=1;i<lines.size();++i){if(lines[i].empty()&&i==lines.size()-1)continue;
  auto fields=Split(lines[i],'\t');
  if(fields.size()==2&&fields[0]=="theme"&&!hasTheme){
   if(!ThemeShareCode::Decode(fields[1]))return fail("The included theme code is invalid.");
   candidate.theme=fields[1];hasTheme=true;
  }else if(fields.size()==4&&fields[0]=="module"&&Name(fields[1])&&Name(fields[2])){
   int value=0;auto number=std::from_chars(fields[3].data(),fields[3].data()+fields[3].size(),value);
   if(fields[3].empty()||number.ec!=std::errc{}||number.ptr!=fields[3].data()+fields[3].size())return fail("A setting value is invalid.");
   if(candidate.records.size()>=2048||!keys.emplace(fields[1],fields[2]).second)return fail("Duplicate or excessive config entries.");
   candidate.records.push_back({std::string(fields[1]),std::string(fields[2]),value});
  }else return fail("Unexpected config data. Nothing was changed.");
 }
 if(candidate.records.empty())return fail("This config contains no module settings.");
 doc=std::move(candidate);error.clear();return true;
}
}
