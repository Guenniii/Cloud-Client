#pragma once
#include "../../dependencies/imgui/imgui.h"
#include <algorithm>
#include <atomic>
#include <cmath>
namespace Theme {
struct Palette {
 const char* name;
 ImU32 background,card,border,text,muted,label,accent,hover,soft,toggle,onAccent,success;
};
inline int selected=0;
inline std::atomic<bool> custom_accent_enabled{false};
inline std::atomic<int> custom_accent_rgb{0x6F42FF};
inline std::atomic<bool> custom_background_enabled{false};
inline std::atomic<int> custom_background_rgb{0xF8F9FC};
inline std::atomic<int> menu_glow_percent{100}, esp_glow_percent{100};
inline std::atomic<bool> gradient_enabled{false}, animated_accent{false}, player_colors_enabled{false};
inline std::atomic<int> secondary_rgb{0xF459D5}, gradient_strength{18}, animation_period{8};
inline std::atomic<int> friend_rgb{0x4BDBB7}, enemy_rgb{0xF4597B};
inline ImU32 FromRgb(int rgb){return IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);}
inline ImU32 Blend(ImU32 a,ImU32 b,float t){
 auto x=ImGui::ColorConvertU32ToFloat4(a),y=ImGui::ColorConvertU32ToFloat4(b);
 return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x*(1-t)+y.x*t,x.y*(1-t)+y.y*t,x.z*(1-t)+y.z*t,1));
}
inline float MenuGlow(){return std::clamp(menu_glow_percent.load(),0,200)/100.0f;}
inline float EspGlow(){return std::clamp(esp_glow_percent.load(),0,200)/100.0f;}
inline const Palette presets[]={
 {"Pearl Violet",IM_COL32(248,249,252,255),IM_COL32(255,255,255,255),IM_COL32(230,230,242,255),IM_COL32(25,25,43,255),IM_COL32(116,118,139,255),IM_COL32(105,105,137,255),IM_COL32(111,66,255,255),IM_COL32(133,94,255,255),IM_COL32(235,230,255,255),IM_COL32(218,219,231,255),IM_COL32(255,255,255,255),IM_COL32(35,172,107,255)},
 {"Ocean Mint",IM_COL32(13,23,34,255),IM_COL32(22,36,49,255),IM_COL32(42,62,77,255),IM_COL32(231,245,247,255),IM_COL32(151,176,185,255),IM_COL32(183,210,216,255),IM_COL32(75,218,183,255),IM_COL32(113,235,206,255),IM_COL32(30,65,66,255),IM_COL32(61,81,96,255),IM_COL32(12,36,34,255),IM_COL32(93,220,153,255)},
 {"Rose Dusk",IM_COL32(25,22,32,255),IM_COL32(38,32,45,255),IM_COL32(65,52,70,255),IM_COL32(248,236,244,255),IM_COL32(183,162,181,255),IM_COL32(211,188,207,255),IM_COL32(242,151,181,255),IM_COL32(255,181,203,255),IM_COL32(76,44,64,255),IM_COL32(85,65,88,255),IM_COL32(47,23,37,255),IM_COL32(123,217,172,255)},
 {"Arctic Blue",IM_COL32(244,248,253,255),IM_COL32(255,255,255,255),IM_COL32(218,229,244,255),IM_COL32(29,43,66,255),IM_COL32(109,127,153,255),IM_COL32(82,106,141,255),IM_COL32(52,112,223,255),IM_COL32(83,140,246,255),IM_COL32(229,239,255,255),IM_COL32(203,217,237,255),IM_COL32(255,255,255,255),IM_COL32(34,156,119,255)},
 {"Midnight Indigo",IM_COL32(17,19,33,255),IM_COL32(25,29,48,255),IM_COL32(47,53,78,255),IM_COL32(236,239,255,255),IM_COL32(154,164,193,255),IM_COL32(190,198,227,255),IM_COL32(149,135,255,255),IM_COL32(176,164,255,255),IM_COL32(48,44,77,255),IM_COL32(67,73,100,255),IM_COL32(22,21,41,255),IM_COL32(109,217,171,255)},
 {"Amber Graphite",IM_COL32(24,25,28,255),IM_COL32(35,36,40,255),IM_COL32(61,62,67,255),IM_COL32(246,243,233,255),IM_COL32(173,172,161,255),IM_COL32(214,205,182,255),IM_COL32(240,184,82,255),IM_COL32(255,207,126,255),IM_COL32(66,52,32,255),IM_COL32(78,77,71,255),IM_COL32(37,29,16,255),IM_COL32(134,213,157,255)},
 {"Sage Mist",IM_COL32(244,247,243,255),IM_COL32(255,255,253,255),IM_COL32(222,229,219,255),IM_COL32(35,49,40,255),IM_COL32(116,133,121,255),IM_COL32(84,109,91,255),IM_COL32(64,132,105,255),IM_COL32(88,158,128,255),IM_COL32(225,239,228,255),IM_COL32(207,220,210,255),IM_COL32(255,255,255,255),IM_COL32(55,155,106,255)},
 {"Neon Orchid",IM_COL32(23,13,35,255),IM_COL32(38,22,53,255),IM_COL32(79,44,98,255),IM_COL32(255,240,255,255),IM_COL32(191,156,201,255),IM_COL32(216,179,237,255),IM_COL32(244,89,213,255),IM_COL32(255,137,229,255),IM_COL32(77,32,85,255),IM_COL32(90,56,104,255),IM_COL32(35,12,42,255),IM_COL32(92,237,184,255)},
 {"Tropical Cyan",IM_COL32(8,28,34,255),IM_COL32(14,43,49,255),IM_COL32(35,79,87,255),IM_COL32(231,255,247,255),IM_COL32(140,187,184,255),IM_COL32(214,229,153,255),IM_COL32(33,229,210,255),IM_COL32(111,255,231,255),IM_COL32(20,73,73,255),IM_COL32(42,94,99,255),IM_COL32(5,42,42,255),IM_COL32(200,237,103,255)},
 {"Sunset Coral",IM_COL32(36,19,35,255),IM_COL32(54,29,44,255),IM_COL32(99,54,67,255),IM_COL32(255,240,227,255),IM_COL32(203,164,165,255),IM_COL32(243,185,134,255),IM_COL32(255,119,98,255),IM_COL32(255,163,129,255),IM_COL32(99,47,58,255),IM_COL32(114,65,76,255),IM_COL32(52,20,24,255),IM_COL32(143,229,159,255)},
 {"Electric Lime",IM_COL32(18,25,18,255),IM_COL32(28,39,26,255),IM_COL32(57,79,44,255),IM_COL32(245,255,227,255),IM_COL32(167,188,146,255),IM_COL32(199,213,163,255),IM_COL32(182,244,62,255),IM_COL32(212,255,113,255),IM_COL32(55,74,27,255),IM_COL32(72,92,53,255),IM_COL32(24,38,9,255),IM_COL32(58,221,178,255)},
 {"Cotton Candy",IM_COL32(255,241,249,255),IM_COL32(255,250,253,255),IM_COL32(237,204,226,255),IM_COL32(62,32,74,255),IM_COL32(139,102,143,255),IM_COL32(121,66,135,255),IM_COL32(193,50,172,255),IM_COL32(217,90,198,255),IM_COL32(238,220,250,255),IM_COL32(222,197,229,255),IM_COL32(255,255,255,255),IM_COL32(15,142,144,255)},
 {"Cherry Volt",IM_COL32(29,12,22,255),IM_COL32(45,21,34,255),IM_COL32(84,40,59,255),IM_COL32(255,239,244,255),IM_COL32(195,151,171,255),IM_COL32(229,176,199,255),IM_COL32(255,73,123,255),IM_COL32(255,128,159,255),IM_COL32(84,29,52,255),IM_COL32(104,52,71,255),IM_COL32(43,9,24,255),IM_COL32(88,225,206,255)}
};
inline constexpr int Count = sizeof(presets) / sizeof(presets[0]);
inline const Palette& Current(){
 const auto& base=presets[std::clamp(selected,0,Count-1)];
 if(!custom_accent_enabled.load() && !custom_background_enabled.load() && !animated_accent.load()) return base;
 thread_local Palette custom;
 custom=base;
 auto rgbColor=[](int rgb){return IM_COL32((rgb>>16)&255,(rgb>>8)&255,rgb&255,255);};
 auto blend=[](ImU32 a,ImU32 b,float amount){
  auto x=ImGui::ColorConvertU32ToFloat4(a),y=ImGui::ColorConvertU32ToFloat4(b);
  return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x*(1-amount)+y.x*amount,x.y*(1-amount)+y.y*amount,x.z*(1-amount)+y.z*amount,1));
 };
 auto luminance=[](ImU32 color){
  auto c=ImGui::ColorConvertU32ToFloat4(color);
  auto linear=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
  return .2126f*linear(c.x)+.7152f*linear(c.y)+.0722f*linear(c.z);
 };
 if(custom_background_enabled.load()) {
  custom.background=rgbColor(custom_background_rgb.load());
  const bool light=luminance(custom.background)>.179f;
  custom.text=light ? IM_COL32(18,19,28,255) : IM_COL32(245,247,255,255);
  custom.card=blend(custom.background,IM_COL32(255,255,255,255),light?.32f:.07f);
  custom.border=blend(custom.card,custom.text,.20f);
  custom.muted=blend(custom.card,custom.text,.72f);
  custom.label=blend(custom.card,custom.text,.88f);
  custom.toggle=blend(custom.card,custom.text,.25f);
 }
 if(custom_accent_enabled.load()) custom.accent=rgbColor(custom_accent_rgb.load());
 if(animated_accent.load() && ImGui::GetCurrentContext()) {
  const double phase=ImGui::GetTime()/std::clamp(animation_period.load(),2,20);
  const float amount=static_cast<float>((1-std::cos(phase*6.283185307179586))*.5);
  custom.accent=blend(custom.accent,FromRgb(secondary_rgb.load()),amount);
 }
 custom.soft=blend(custom.card,custom.accent,.16f);
 custom.hover=blend(IM_COL32(255,255,255,255),custom.accent,.78f);
 custom.onAccent=luminance(custom.accent)>.179f ? IM_COL32(10,10,15,255) : IM_COL32(255,255,255,255);
 return custom;
}
inline ImU32 GradientEnd(){return Blend(Current().background,FromRgb(secondary_rgb.load()),std::clamp(gradient_strength.load(),0,50)/100.0f);}
inline ImU32 PlayerColor(bool friendPlayer,int fallbackFriend){
 if(player_colors_enabled.load()) return FromRgb(friendPlayer ? friend_rgb.load() : enemy_rgb.load());
 return friendPlayer ? FromRgb(fallbackFriend) : Current().accent;
}
inline ImU32 SkinTint(ImU32 color) {
 auto src=ImGui::ColorConvertU32ToFloat4(color),accent=ImGui::ColorConvertU32ToFloat4(PlayerColor(false,0));
 src.x=src.x*.88f+accent.x*.12f;src.y=src.y*.88f+accent.y*.12f;src.z=src.z*.88f+accent.z*.12f;
 return ImGui::ColorConvertFloat4ToU32(src);
}
inline void ApplyStyle() {
 auto& c=ImGui::GetStyle().Colors;const auto& p=Current();
 auto set=[&](ImGuiCol id,ImU32 color){c[id]=ImGui::ColorConvertU32ToFloat4(color);};
 set(ImGuiCol_Text,p.text);set(ImGuiCol_TextDisabled,p.muted);set(ImGuiCol_WindowBg,p.background);
 set(ImGuiCol_ChildBg,p.card);set(ImGuiCol_PopupBg,p.card);set(ImGuiCol_Border,p.border);
 set(ImGuiCol_FrameBg,p.background);set(ImGuiCol_FrameBgHovered,p.soft);set(ImGuiCol_FrameBgActive,p.soft);
 set(ImGuiCol_Button,p.soft);set(ImGuiCol_ButtonHovered,p.border);set(ImGuiCol_ButtonActive,p.soft);
 set(ImGuiCol_Header,p.soft);set(ImGuiCol_HeaderHovered,p.border);set(ImGuiCol_HeaderActive,p.soft);
 set(ImGuiCol_CheckMark,p.accent);set(ImGuiCol_SliderGrab,p.accent);set(ImGuiCol_SliderGrabActive,p.hover);
 set(ImGuiCol_Separator,p.border);set(ImGuiCol_TextSelectedBg,p.soft);set(ImGuiCol_NavHighlight,p.accent);
}
}
