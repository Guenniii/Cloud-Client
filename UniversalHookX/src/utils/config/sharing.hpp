#pragma once
#include "config.hpp"
#include "share_code.hpp"
#include "../theme/sharing.hpp"
#include "../../menu/history.hpp"
#include <algorithm>
#include <cctype>
#include <Windows.h>
namespace ConfigSharing {
struct Options {bool hotkeys=false,theme=true,enabled=false;};
enum class Kind {Enabled,Hotkey,Mode,Favorite,Setting,Alias};
struct Change {int module,setting,before,value;Kind kind;std::string field;};
struct Review {std::vector<Change> changes;std::vector<std::string> skipped;int modules=0;bool themeSupported=false;};
inline ConfigShareCode::Document Capture(ModuleData* modules,int count){
 ConfigShareCode::Document doc;doc.theme=ThemeSharing::Export();
 for(int i=0;i<count;++i){const auto& m=modules[i];
  doc.records.push_back({m.name,"enabled",m.enabled->load()});
  doc.records.push_back({m.name,"hotkey",m.hotkey});
  doc.records.push_back({m.name,"hotkey_mode",int(m.hotkey_mode)});
  doc.records.push_back({m.name,"favorite",m.favorite});
  for(const auto& s:m.settings_list){
   if(s.type==SettingType::Checkbox && s.b_val)doc.records.push_back({m.name,s.name,int(s.b_val->load())});
   else if(s.i_val)doc.records.push_back({m.name,s.name,s.i_val->load()});
  }
 }
 return doc;
}
inline std::string Export(ModuleData* modules,int count){return ConfigShareCode::Encode(Capture(modules,count));}
inline Review Inspect(const ConfigShareCode::Document& doc,ModuleData* modules,int count){
 Review result;std::set<int> seen;
 if(!doc.theme.empty())if(auto theme=ThemeShareCode::Decode(doc.theme)){
  for(int i=0;i<Theme::Count;++i)result.themeSupported|=theme->preset==Theme::presets[i].name;
  if(!result.themeSupported)result.skipped.push_back("Theme preset unavailable; current theme will be kept.");
 }
 for(const auto& r:doc.records){
  const auto module=Config::CurrentModuleName(r.module,r.field);
  int index=-1;for(int i=0;i<count;++i)if(module==modules[i].name){index=i;break;}
  if(index<0){result.skipped.push_back(r.module+" / "+r.field+": unknown module");continue;}
  auto& m=modules[index];Change change{index,-1,0,r.value,Kind::Setting,SettingsText::English(r.field.c_str())};bool valid=true,found=true;
  if(r.field=="enabled"){change.kind=Kind::Enabled;change.before=m.enabled->load();valid=r.value==0||r.value==1;}
  else if(r.field=="favorite"){change.kind=Kind::Favorite;change.before=m.favorite;valid=r.value==0||r.value==1;}
  else if(r.field=="hotkey"){change.kind=Kind::Hotkey;change.before=m.hotkey;valid=r.value>=0&&r.value<=255;}
  else if(r.field=="hotkey_mode"){change.kind=Kind::Mode;change.before=int(m.hotkey_mode);valid=r.value>=0&&r.value<=1;}
  else {found=false;for(int j=0;j<int(m.settings_list.size());++j){auto& setting=m.settings_list[j];if(r.field!=setting.name)continue;
   found=true;change.setting=j;
   if(setting.type==SettingType::Checkbox && setting.b_val){change.before=setting.b_val->load();valid=r.value==0||r.value==1;if(setting.b_val==m.enabled)change.kind=Kind::Alias;}
   else if(setting.i_val){change.before=setting.i_val->load();valid=setting.type==SettingType::ColorRGB ? r.value>=0&&r.value<=0xFFFFFF : r.value>=setting.i_min&&r.value<=setting.i_max;}
   else valid=false;
   break;
  }}
  if(!found||!valid){result.skipped.push_back(r.module+" / "+r.field+(found?": invalid value (kept current)":": unknown setting"));continue;}
  seen.insert(index);result.changes.push_back(std::move(change));
 }
 result.modules=int(seen.size());return result;
}
inline int After(const Change& c,const Options& options){
 if((c.kind==Kind::Hotkey||c.kind==Kind::Mode)&&!options.hotkeys)return c.before;
 if(c.kind==Kind::Enabled)return options.enabled?c.value:0;
 if(c.kind==Kind::Alias)return c.before;
 return c.value;
}
inline bool NewName(const std::string& name,std::string& error){
 if(name.empty()||name.size()>64||name=="."||name==".."||name.find_first_of("/\\:*?\"<>|\r\n\t")!=name.npos||name.back()=='.'||name.back()==' '){error="Choose a valid profile name (1-64 characters).";return false;}
 auto lower=[](std::string text){for(char& c:text)c=char(std::tolower(static_cast<unsigned char>(c)));return text;};
 Config::RefreshList();for(const auto& existing:Config::GetList())if(lower(existing)==lower(name)){error="That profile already exists. Choose a new name.";return false;}
 return true;
}
inline bool Apply(const ConfigShareCode::Document& doc,const Options& options,const std::string& name,ModuleData* modules,int count,std::string& status){
 if(!NewName(name,status))return false;
 const auto review=Inspect(doc,modules,count);
 if(review.modules==0){status="No supported module settings found. Nothing was changed.";return false;}
 const auto previous=Menu::History::Capture();const auto previousProfile=Config::active_profile;
 static unsigned sequence=0;
 const auto backup="before-import-"+std::to_string(GetTickCount64())+"-"+std::to_string(++sequence);
 if(!Config::SaveNewProfile(backup,modules,count,true)){status="Could not create the backup. Import cancelled. "+Config::profile_status;Config::active_profile=previousProfile;return false;}
 for(int i=0;i<count;++i)*modules[i].enabled=false;
 for(const auto& c:review.changes){auto& m=modules[c.module];
  if(c.kind==Kind::Hotkey){if(options.hotkeys)m.hotkey=c.value;}
  else if(c.kind==Kind::Mode){if(options.hotkeys)m.hotkey_mode=static_cast<Input::HotkeyMode>(c.value);}
  else if(c.kind==Kind::Favorite)m.favorite=c.value!=0;
  else if(c.kind==Kind::Setting){auto& setting=m.settings_list[c.setting];if(setting.type==SettingType::Checkbox)*setting.b_val=c.value!=0;else *setting.i_val=c.value;}
 }
 // Only explicit status records may enable modules; aliased checkbox settings never bypass the opt-in.
 for(int i=0;i<count;++i)*modules[i].enabled=false;
 if(options.enabled)for(const auto& c:review.changes)if(c.kind==Kind::Enabled)*modules[c.module].enabled=c.value!=0;
 if(options.theme&&review.themeSupported){std::string ignored;ThemeSharing::Import(doc.theme,ignored);}
 OreSim::SetProfileSeed(false,0);
 Config::capturing_menu_key=false;Config::capturing_module_key=-1;
 if(!Config::SaveNewProfile(name,modules,count,false)){
  const auto failure=Config::profile_status;Menu::History::Restore(previous);Config::active_profile=previousProfile;
  Config::profile_status="Import rolled back. "+failure;status=Config::profile_status;return false;
 }
 status="Created "+name+". Backup: "+backup+". "+std::to_string(review.skipped.size())+" entries skipped.";
 Config::profile_status=status;Theme::ApplyStyle();return true;
}
}
