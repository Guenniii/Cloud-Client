#pragma once
#include "../../dependencies/imgui/imgui.h"
#include <algorithm>
namespace Theme {
struct Palette {
 const char* name;
 ImU32 background,card,border,text,muted,label,accent,hover,soft,toggle,onAccent,success;
};
inline int selected=0;
inline const Palette presets[]={
 {"Pearl Violet",IM_COL32(248,249,252,255),IM_COL32(255,255,255,255),IM_COL32(230,230,242,255),IM_COL32(25,25,43,255),IM_COL32(116,118,139,255),IM_COL32(105,105,137,255),IM_COL32(111,66,255,255),IM_COL32(133,94,255,255),IM_COL32(235,230,255,255),IM_COL32(218,219,231,255),IM_COL32(255,255,255,255),IM_COL32(35,172,107,255)},
 {"Ocean Mint",IM_COL32(13,23,34,255),IM_COL32(22,36,49,255),IM_COL32(42,62,77,255),IM_COL32(231,245,247,255),IM_COL32(151,176,185,255),IM_COL32(183,210,216,255),IM_COL32(75,218,183,255),IM_COL32(113,235,206,255),IM_COL32(30,65,66,255),IM_COL32(61,81,96,255),IM_COL32(12,36,34,255),IM_COL32(93,220,153,255)},
 {"Rose Dusk",IM_COL32(25,22,32,255),IM_COL32(38,32,45,255),IM_COL32(65,52,70,255),IM_COL32(248,236,244,255),IM_COL32(183,162,181,255),IM_COL32(211,188,207,255),IM_COL32(242,151,181,255),IM_COL32(255,181,203,255),IM_COL32(76,44,64,255),IM_COL32(85,65,88,255),IM_COL32(47,23,37,255),IM_COL32(123,217,172,255)}
};
inline const Palette& Current(){return presets[std::clamp(selected,0,2)];}
inline ImU32 SkinTint(ImU32 color) {
 auto src=ImGui::ColorConvertU32ToFloat4(color),accent=ImGui::ColorConvertU32ToFloat4(Current().accent);
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
