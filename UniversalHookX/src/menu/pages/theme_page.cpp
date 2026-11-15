#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../utils/theme/sharing.hpp"
#include <cstdio>
#include <algorithm>
#include "../../dependencies/imgui/imgui_internal.h"

namespace Menu::Pages {
void DrawThemePage(ImDrawList*, ImVec2 area_min, float area_w, float area_h, float alpha) {
    ImGui::SetCursorScreenPos(area_min);
    ImGui::BeginChild("ThemeContent", ImVec2(area_w, area_h), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoBackground);
    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    draw->AddText(origin, Widgets::ColA(Palette::Muted(), alpha), "Click a preview to apply. Saved with your profile; also used by ESP.");
    static char share_code[512]={};
    static std::string share_status;
    const float share_y=origin.y+34;
    const float share_input_w=(std::max)(100.0f,width-220.0f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x,share_y));ImGui::SetNextItemWidth(share_input_w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,8.0f);ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(10,8));
    ImGui::InputTextWithHint("##theme_share_code","Paste a theme code...",share_code,sizeof(share_code));
    ImGui::PopStyleVar(2);
    ImGui::SetCursorScreenPos(ImVec2(origin.x+share_input_w+10,share_y));
    if(Widgets::DrawRoundedButton(draw,"##theme_export","Copy code",ImVec2(94,32),Palette::Soft(),Palette::Border(),Palette::Accent(),alpha,8)) {
        auto code=ThemeSharing::Export();
        std::snprintf(share_code,sizeof(share_code),"%s",code.c_str());
        ImGui::SetClipboardText(code.c_str());share_status="Theme code copied. Only appearance settings are included.";
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x+share_input_w+114,share_y));
    if(Widgets::DrawRoundedButton(draw,"##theme_import","Import",ImVec2(94,32),Palette::Soft(),Palette::Border(),Palette::Accent(),alpha,8)) ThemeSharing::Import(share_code,share_status);
    draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(origin.x,share_y+40),Widgets::ColA(Palette::Muted(),alpha),share_status.empty()?"Share your design, or paste a code and import it. Undo restores the previous appearance.":share_status.c_str(),nullptr,width);
    const float controls_w = (std::max)(240.0f, width - 292.0f);
    const float panel_y = origin.y + 116.0f;
    draw->AddRectFilled(ImVec2(origin.x,panel_y),ImVec2(origin.x+width,panel_y+312.0f),Widgets::ColA(Palette::Card(),alpha),12);
    draw->AddRect(ImVec2(origin.x,panel_y),ImVec2(origin.x+width,panel_y+312.0f),Widgets::ColA(Palette::Border(),alpha),12);
    Widgets::DrawCheckboxRow(draw, ImVec2(origin.x+16,panel_y+16),controls_w,"Custom accent",&Theme::custom_accent_enabled,alpha);
    const int old_rgb=Theme::custom_accent_rgb.load();
    Widgets::DrawColorRow(draw,ImVec2(origin.x+16,panel_y+54),controls_w,"Accent color",&Theme::custom_accent_rgb,alpha);
    if(Theme::custom_accent_rgb!=old_rgb) Theme::custom_accent_enabled=true;
    Widgets::DrawCheckboxRow(draw,ImVec2(origin.x+16,panel_y+96),controls_w,"Custom background",&Theme::custom_background_enabled,alpha);
    const int old_background=Theme::custom_background_rgb.load();
    Widgets::DrawColorRow(draw,ImVec2(origin.x+16,panel_y+134),controls_w,"Background color",&Theme::custom_background_rgb,alpha);
    if(Theme::custom_background_rgb!=old_background) Theme::custom_background_enabled=true;
    Widgets::DrawSliderRow(draw,ImVec2(origin.x+16,panel_y+180),controls_w,"Menu glow",&Theme::menu_glow_percent,0,200,"%",alpha);
    Widgets::DrawSliderRow(draw,ImVec2(origin.x+16,panel_y+238),controls_w,"ESP / effects glow",&Theme::esp_glow_percent,0,200,"%",alpha);
    const ImVec2 preview(origin.x+controls_w+40,panel_y+14), preview_end(origin.x+width-16,panel_y+296);
    const int preview_first=draw->VtxBuffer.Size;
    draw->AddRectFilled(preview,preview_end,Widgets::ColA(Palette::Background(),alpha),10);
    if(Theme::gradient_enabled) ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw,preview_first,draw->VtxBuffer.Size,preview,ImVec2(preview.x,preview_end.y),Palette::Background(),Theme::GradientEnd());
    draw->AddText(ImVec2(preview.x+12,preview.y+10),Widgets::ColA(Palette::Text(),alpha),"Live preview");
    const ImVec2 toggle(preview.x+18,preview.y+45);
    Widgets::DrawGlowRect(draw,toggle,ImVec2(toggle.x+42,toggle.y+22),11,Palette::Accent(),alpha,12);
    draw->AddRectFilled(toggle,ImVec2(toggle.x+42,toggle.y+22),Widgets::ColA(Palette::Accent(),alpha),11);
    draw->AddCircleFilled(ImVec2(toggle.x+31,toggle.y+11),8,Widgets::ColA(Theme::Current().onAccent,alpha));
    draw->AddText(ImVec2(toggle.x+56,toggle.y+4),Widgets::ColA(Palette::Label(),alpha),"Menu");
    const ImVec2 box(preview.x+20,preview.y+90),box_end(preview.x+58,preview.y+164);
    Widgets::DrawGlowRect(draw,box,box_end,3,Palette::Accent(),alpha,12,true);
    draw->AddRect(box,box_end,Widgets::ColA(Palette::Accent(),alpha),3,0,2);
    draw->AddText(ImVec2(box_end.x+14,box.y+27),Widgets::ColA(Palette::Label(),alpha),"ESP");
    ImGui::SetCursorScreenPos(ImVec2(preview.x+10,preview_end.y-30));
    if(Widgets::DrawRoundedButton(draw,"##reset_appearance","Reset appearance",ImVec2(preview_end.x-preview.x-20,24),Palette::Soft(),Palette::Border(),Palette::Accent(),alpha,6)) {
        Theme::gradient_enabled=false;Theme::animated_accent=false;Theme::player_colors_enabled=false;
        Theme::secondary_rgb=0xF459D5;Theme::gradient_strength=18;Theme::animation_period=8;
        Theme::friend_rgb=0x4BDBB7;Theme::enemy_rgb=0xF4597B;
        Theme::custom_accent_enabled=false;Theme::custom_accent_rgb=0x6F42FF;
        Theme::custom_background_enabled=false;Theme::custom_background_rgb=0xF8F9FC;
        Theme::menu_glow_percent=100;Theme::esp_glow_percent=100;
    }
    static bool studio_open=false;
    const float studio_y=origin.y+446;
    ImGui::SetCursorScreenPos(ImVec2(origin.x,studio_y));
    if(Widgets::DrawRoundedButton(draw,"##studio_section",studio_open?"Theme Studio  ^":"Theme Studio  -  gradients, animation and player colors",ImVec2(width,36),Palette::Card(),Palette::Border(),Palette::Accent(),alpha,10))studio_open=!studio_open;
    const float studio_h=studio_open?320.0f:36.0f;
    if(studio_open) {
        const float column_w=(width-48)*.5f;
        const float x=origin.x+16,x2=origin.x+32+column_w,y=studio_y+52;
        Widgets::DrawCheckboxRow(draw,ImVec2(x,y),column_w,"Background gradient",&Theme::gradient_enabled,alpha);
        Widgets::DrawColorRow(draw,ImVec2(x,y+38),column_w,"Secondary color",&Theme::secondary_rgb,alpha);
        Widgets::DrawSliderRow(draw,ImVec2(x,y+80),column_w,"Gradient strength",&Theme::gradient_strength,0,50,"%",alpha);
        Widgets::DrawCheckboxRow(draw,ImVec2(x,y+138),column_w,"Animated accent",&Theme::animated_accent,alpha);
        Widgets::DrawSliderRow(draw,ImVec2(x,y+176),column_w,"Animation period",&Theme::animation_period,2,20," s",alpha);
        Widgets::DrawCheckboxRow(draw,ImVec2(x2,y),column_w,"Custom player colors",&Theme::player_colors_enabled,alpha);
        const int old_friend=Theme::friend_rgb.load(),old_enemy=Theme::enemy_rgb.load();
        Widgets::DrawColorRow(draw,ImVec2(x2,y+38),column_w,"Friends",&Theme::friend_rgb,alpha);
        Widgets::DrawColorRow(draw,ImVec2(x2,y+80),column_w,"Opponents",&Theme::enemy_rgb,alpha);
        if(old_friend!=Theme::friend_rgb || old_enemy!=Theme::enemy_rgb)Theme::player_colors_enabled=true;
        draw->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(x2,y+132),Widgets::ColA(Palette::Muted(),alpha),"Player colors apply to Player ESP and Echo Trail. Secondary color controls the gradient and animated accent.",nullptr,column_w);
        const ImVec2 a(x2+8,y+204),b(x2+column_w-20,y+204);
        draw->AddLine(a,ImVec2(a.x+56,a.y),Widgets::ColA(Theme::PlayerColor(true,0x4BDBB7),alpha),3);
        draw->AddLine(ImVec2(b.x-56,b.y),b,Widgets::ColA(Theme::PlayerColor(false,0),alpha),3);
        draw->AddText(ImVec2(a.x,y+224),Widgets::ColA(Palette::Label(),alpha),"Friend");
        draw->AddText(ImVec2(b.x-56,y+224),Widgets::ColA(Palette::Label(),alpha),"Opponent");
    }
    const float grid_y=studio_y+studio_h+14;
    const int columns = width >= 690.0f ? 3 : width >= 440.0f ? 2 : 1;
    const float gap = 14.0f;
    const float tile_w = (width - gap * (columns - 1)) / columns;
    const float tile_h = 146.0f;
    for (int i=0; i<Theme::Count; ++i) {
        const auto& theme = Theme::selected == i ? Theme::Current() : Theme::presets[i];
        const ImVec2 pos(origin.x+(i%columns)*(tile_w+gap), grid_y+(i/columns)*(tile_h+gap));
        const ImVec2 end(pos.x+tile_w,pos.y+tile_h);
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::InvisibleButton("##theme", ImVec2(tile_w,tile_h))) {
            Theme::selected=i;
            Theme::ApplyStyle();
        }
        const bool selected=Theme::selected==i;
        draw->AddRectFilled(pos,end,Widgets::ColA(theme.background,alpha),12);
        draw->AddRect(pos,end,Widgets::ColA(selected?theme.accent:theme.border,alpha),12,0,selected?2.0f:1.0f);
        draw->AddText(ImVec2(pos.x+14,pos.y+13),Widgets::ColA(theme.text,alpha),theme.name);
        const ImVec2 mini(pos.x+14,pos.y+43),mini_end(end.x-14,pos.y+91);
        draw->AddRectFilled(mini,mini_end,Widgets::ColA(theme.card,alpha),8);
        draw->AddRect(mini,mini_end,Widgets::ColA(theme.border,alpha),8);
        draw->AddText(ImVec2(mini.x+10,mini.y+8),Widgets::ColA(theme.text,alpha),"Player ESP");
        draw->AddLine(ImVec2(mini.x+10,mini.y+33),ImVec2(mini_end.x-54,mini.y+33),Widgets::ColA(theme.soft,alpha),3);
        draw->AddLine(ImVec2(mini.x+10,mini.y+33),ImVec2(mini.x+46,mini.y+33),Widgets::ColA(theme.accent,alpha),3);
        draw->AddRectFilled(ImVec2(mini_end.x-38,mini.y+16),ImVec2(mini_end.x-10,mini.y+32),Widgets::ColA(theme.accent,alpha),8);
        draw->AddCircleFilled(ImVec2(mini_end.x-18,mini.y+24),5,Widgets::ColA(theme.onAccent,alpha));
        const ImU32 swatches[]={theme.accent,theme.soft,theme.card,theme.text};
        for(int c=0;c<4;++c) draw->AddCircleFilled(ImVec2(pos.x+20+c*22,pos.y+119),6,Widgets::ColA(swatches[c],alpha));
        if(selected) draw->AddText(ImVec2(end.x-14-ImGui::CalcTextSize("ACTIVE").x,pos.y+112),Widgets::ColA(theme.accent,alpha),"ACTIVE");
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x,grid_y+((Theme::Count+columns-1)/columns)*(tile_h+gap)));
    ImGui::Dummy(ImVec2(1,1));
    ImGui::EndChild();
}
}
