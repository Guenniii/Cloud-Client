#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../config_share_files.hpp"
#include "../../dependencies/imgui/imgui_internal.h"
#include "../../utils/config/sharing.hpp"
#include "../module_registry.hpp"
#include <algorithm>
#include <array>
#include <cstdio>

namespace Menu::Pages {
using namespace Menu::Widgets;
namespace {
char new_config_name[65]="";
char import_name[65]="Imported config";
std::array<char,ConfigShareCode::MaxCode+1> code{};
ConfigShareCode::Document document;
ConfigSharing::Options import_options;
std::string share_status;
bool preview_ready=false,sharing=false;
// Overwrite/delete need a second click within 3 s on the same button.
std::string confirm_action;double confirm_until=0;
bool Confirmed(const std::string& action){
 const double now=ImGui::GetTime();
 if(confirm_action==action&&now<confirm_until){confirm_action.clear();return true;}
 confirm_action=action;confirm_until=now+3;return false;
}
bool Pending(const std::string& action){return confirm_action==action&&ImGui::GetTime()<confirm_until;}
void SetCode(std::string_view text){std::snprintf(code.data(),code.size(),"%.*s",int(text.size()),text.data());preview_ready=false;share_status.clear();}
void Text(const char* value,bool muted=false){
 ImGui::PushStyleColor(ImGuiCol_Text,muted?Palette::Muted():Palette::Text());
 ImGui::PushTextWrapPos(0);ImGui::TextWrapped("%s",value);ImGui::PopTextWrapPos();ImGui::PopStyleColor();
}
bool Button(const char* id,const char* label,float width,float alpha,bool primary=false){
 return DrawRoundedButton(ImGui::GetWindowDrawList(),id,label,ImVec2(width,32),
  primary?Palette::Accent():Palette::Soft(),primary?Theme::Current().hover:Palette::Border(),primary?Theme::Current().onAccent:Palette::Accent(),alpha,8);
}
void Saved(ModuleData* modules,int count,float width,float alpha){
 ImGui::SetNextItemWidth((std::min)(300.0f,width-108));
 ImGui::InputTextWithHint("##new_config","Profile name",new_config_name,sizeof(new_config_name));
 ImGui::SameLine(0,12);
 const auto& existing=Config::GetList();
 const bool overwrite=std::find(existing.begin(),existing.end(),std::string(new_config_name))!=existing.end();
 const std::string saveAction="save:"+std::string(new_config_name);
 if(Button("##save_config",Pending(saveAction)?"Sure?":"Save",90,alpha,true)&&(!overwrite||Confirmed(saveAction)))
  Config::Save(new_config_name,modules,count);
 ImGui::Spacing();
 Text(Config::profile_status.empty()?"Load a saved profile before sharing it. Sharing exports your current settings.":Config::profile_status.c_str(),true);
 ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
 const auto list=Config::GetList();
 if(list.empty())Text("No profiles yet. Enter a name and save your current settings.",true);
 for(size_t i=0;i<list.size();++i){
  ImGui::PushID(int(i));auto* draw=ImGui::GetWindowDrawList();const auto pos=ImGui::GetCursorScreenPos();
  const float height=(std::max)(38.0f,ImGui::GetFontSize()+20);
  draw->AddRectFilled(pos,ImVec2(pos.x+width,pos.y+height),ColA(Palette::Background(),alpha),8);
  const auto label=list[i]+(Config::active_profile==list[i]?"  *":"");
  draw->PushClipRect(pos,ImVec2(pos.x+width-148,pos.y+height),true);
  draw->AddText(ImVec2(pos.x+12,pos.y+(height-ImGui::GetFontSize())*.5f),ColA(Palette::Text(),alpha),label.c_str());draw->PopClipRect();
  ImGui::SetCursorScreenPos(ImVec2(pos.x+width-136,pos.y+3));
  if(Button("##load","Load",58,alpha)){
   // Keys missing from the profile must not keep the previous profile's values.
   for(int m=0;m<count;++m)Registry::ResetModule(m);
   Config::Load(list[i],modules,count);
  }
  ImGui::SetCursorScreenPos(ImVec2(pos.x+width-70,pos.y+3));
  const std::string deleteAction="delete:"+list[i];
  if(Button("##delete",Pending(deleteAction)?"Sure?":"Delete",62,alpha)&&Confirmed(deleteAction))Config::Delete(list[i]);
  ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+height+8));ImGui::PopID();
 }
}
void Sharing(ModuleData* modules,int count,float width,float alpha){
 Text("Share current settings as a code or .ccfg file. Login data, HWID, friend names and seeds are excluded.",true);
 ImGui::Spacing();const float buttonWidth=(width-24)/4;
 if(Button("##copy_config","Copy code",buttonWidth,alpha)){
  auto text=ConfigSharing::Export(modules,count);
  if(text.empty())share_status="Could not encode the current settings.";
  else {SetCode(text);ImGui::SetClipboardText(text.c_str());share_status="Current settings copied. Load a saved profile first to share its values.";}
 }
 ImGui::SameLine(0,8);
 if(Button("##export_config_file","Export file",buttonWidth,alpha)){
  auto text=ConfigSharing::Export(modules,count);auto path=Menu::ConfigShareFiles::Pick(true,share_status);
  if(!path.empty()&&Menu::ConfigShareFiles::Write(path,text,share_status))share_status="Shared config exported. Private data was excluded.";
 }
 ImGui::SameLine(0,8);
 if(Button("##paste_config","Paste code",buttonWidth,alpha)){
  if(const char* clipboard=ImGui::GetClipboardText()){
   size_t size=0;while(size<=ConfigShareCode::MaxCode&&clipboard[size])++size;
   if(size>ConfigShareCode::MaxCode)share_status="Clipboard content is too large.";else SetCode(std::string_view(clipboard,size));
  }
 }
 ImGui::SameLine(0,8);
 if(Button("##open_config_file","Open file",buttonWidth,alpha)){
  auto path=Menu::ConfigShareFiles::Pick(false,share_status);std::string text;
  if(!path.empty()&&Menu::ConfigShareFiles::Read(path,text,share_status))SetCode(text);
 }
 ImGui::Spacing();
 if(ImGui::InputTextMultiline("##shared_config",code.data(),code.size(),ImVec2(width,76)))preview_ready=false;
 ImGui::Spacing();
 if(Button("##preview_config","Preview import",154,alpha,true))preview_ready=ConfigShareCode::Decode(code.data(),document,share_status);
 if(!share_status.empty()){ImGui::Spacing();Text(share_status.c_str(),true);}
 if(!preview_ready)return;
 ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
 const auto review=ConfigSharing::Inspect(document,modules,count);
 Text("Choose what to import");
 auto* draw=ImGui::GetWindowDrawList();
 for(int i=0;i<3;++i){
  auto pos=ImGui::GetCursorScreenPos();
  DrawCheckboxRow(draw,pos,width,i==0?"Include hotkeys":i==1?"Include theme":"Enable modules from config",i==0?&import_options.hotkeys:i==1?&import_options.theme:&import_options.enabled,alpha);
  ImGui::SetCursorScreenPos(ImVec2(pos.x,pos.y+34));
 }
 Text(import_options.enabled?"Module states are taken from the config. The OreSim seed is cleared.":"All modules stay off after import. The OreSim seed is cleared.",true);
 Text("A backup is saved first. Undo restores settings; saved files remain.",true);
 ImGui::Spacing();ImGui::SetNextItemWidth((std::min)(300.0f,width-184));
 ImGui::InputTextWithHint("##import_profile_name","New profile name",import_name,sizeof(import_name));
 ImGui::SameLine(0,12);ImGui::BeginDisabled(review.modules==0);
 if(Button("##apply_shared_config","Create profile",164,alpha,true)){
  if(ConfigSharing::Apply(document,import_options,import_name,modules,count,share_status))preview_ready=false;
 }
 ImGui::EndDisabled();
 if(!preview_ready){Text(share_status.c_str(),true);return;}
 ImGui::Spacing();int changed=0;
 for(const auto& c:review.changes)if(c.kind!=ConfigSharing::Kind::Alias && ConfigSharing::After(c,import_options)!=c.before)++changed;
 ImGui::Text("%d supported modules | %d changed fields | %d skipped entries",review.modules,changed,int(review.skipped.size()));
 if(!document.theme.empty()){
  auto theme=ThemeShareCode::Decode(document.theme);
  ImGui::TextWrapped("Theme: %s (%s)",theme->preset.c_str(),!import_options.theme?"kept current":review.themeSupported?"included":"unavailable; kept current");
 }
 if(import_options.hotkeys)Text("Hotkey conflicts remain visible on module cards and in Settings.",true);
 if(ImGui::CollapsingHeader("Changes",ImGuiTreeNodeFlags_DefaultOpen)){
  if(ImGui::BeginTable("##import_changes",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)){
   ImGui::TableSetupColumn("Module");ImGui::TableSetupColumn("Setting");ImGui::TableSetupColumn("Current");ImGui::TableSetupColumn("Imported");ImGui::TableHeadersRow();
   for(const auto& c:review.changes){if(c.kind==ConfigSharing::Kind::Alias)continue;
    ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(modules[c.module].name);
    ImGui::TableNextColumn();ImGui::TextWrapped("%s",c.field.c_str());
    ImGui::TableNextColumn();ImGui::Text("%d",c.before);ImGui::TableNextColumn();ImGui::Text("%d",ConfigSharing::After(c,import_options));
   }
   ImGui::EndTable();
  }
 }
 if(!review.skipped.empty()&&ImGui::CollapsingHeader("Skipped entries"))for(const auto& entry:review.skipped)ImGui::BulletText("%s",entry.c_str());
}
}
void DrawConfigPage(ImDrawList* draw,ImVec2 area_min,float area_w,float area_h,float alpha,ModuleData* modules,int moduleCount){
 draw->AddRectFilled(area_min,ImVec2(area_min.x+area_w,area_min.y+area_h),ColA(Palette::Card(),alpha),12);
 draw->AddRect(area_min,ImVec2(area_min.x+area_w,area_min.y+area_h),ColA(Palette::Border(),alpha),12);
 draw->AddText(ImVec2(area_min.x+20,area_min.y+18),ColA(Palette::Text(),alpha),"Profiles");
 draw->AddText(ImVec2(area_min.x+20,area_min.y+40),ColA(Palette::Muted(),alpha),sharing?"Share and review module settings.":"Modules, hotkeys, theme and manual OreSim seed.");
 ImGui::SetCursorScreenPos(ImVec2(area_min.x+area_w-202,area_min.y+16));
 if(Button("##profiles_saved","Saved",82,alpha,!sharing))sharing=false;
 ImGui::SetCursorScreenPos(ImVec2(area_min.x+area_w-110,area_min.y+16));
 if(Button("##profiles_share","Sharing",90,alpha,sharing))sharing=true;
 ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,area_min.y+76));
 ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,8));
 ImGui::BeginChild("##profile_content",ImVec2(area_w-40,(std::max)(1.0f,area_h-92)),false,ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_NoBackground);
 const float width=ImGui::GetContentRegionAvail().x;
 if(sharing)Sharing(modules,moduleCount,width,alpha);else Saved(modules,moduleCount,width,alpha);
 ImGui::Dummy(ImVec2(1,8));ImGui::EndChild();ImGui::PopStyleVar(2);
}
}
