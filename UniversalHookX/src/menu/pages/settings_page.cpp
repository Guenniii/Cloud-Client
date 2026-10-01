#include "../pages.hpp"
#include "../performance.hpp"
#include "../diagnostics.hpp"
#include "../module_registry.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../dependencies/imgui/imgui_internal.h"
#include <cstdio>
#include "../../base.hpp"
#include "../../utils/config/config.hpp"
#include "../../modules/settings.hpp"
#include "../key_names.hpp"
#include "../../input/key_mapping.hpp"

namespace Menu::Pages {
using namespace Menu::Widgets;

void DrawSettingsPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha) {
    ImVec2 area_max = ImVec2(area_min.x + area_w, area_min.y + area_h);

    draw_list->AddRectFilled(area_min, area_max, ColA(Menu::Palette::Card(), alpha), 12.0f);
    draw_list->AddRect(area_min, area_max, ColA(Menu::Palette::Border(), alpha), 12.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(area_min);
    ImGui::BeginChild("SettingsContent",ImVec2(area_w,area_h),false,ImGuiWindowFlags_NoScrollbar);
    area_min=ImGui::GetCursorScreenPos();
    area_max=ImVec2(area_min.x+area_w,area_min.y+area_h);
    draw_list=ImGui::GetWindowDrawList();
    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 18), ColA(Menu::Palette::Text(), alpha), "Settings");
    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 40), ColA(Menu::Palette::Muted(), alpha), "General options for the menu itself.");

    float row_w = area_w - 40.0f;
    float row_y = area_min.y + 76.0f;

    // --- Menu Key ---
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 3), ColA(Menu::Palette::Label(), alpha), "Menu Key");
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 20), ColA(Menu::Palette::Muted(), alpha), "Key used to open/close this menu.");

    std::string mk_label = Config::capturing_menu_key ? "..." : GetKeyName(Config::menu_keybind);
    if (mk_label.empty( ))
        mk_label = "Set Key";
    ImVec2 mk_size = ImGui::CalcTextSize(mk_label.c_str( ));
    float badge_w = mk_size.x + 18.0f, badge_h = 24.0f;
    ImVec2 badge_min = ImVec2(area_min.x + 20 + row_w - badge_w, row_y - 2.0f);
    ImVec2 badge_max = ImVec2(badge_min.x + badge_w, badge_min.y + badge_h);

    draw_list->AddRectFilled(badge_min, badge_max, ColA(Config::capturing_menu_key ? Menu::Palette::Soft() : Menu::Palette::Background(), alpha), 6.0f);
    if (Config::capturing_menu_key)
        draw_list->AddRect(badge_min, badge_max, ColA(Menu::Palette::Accent(), alpha), 6.0f, 0, 1.5f);
    draw_list->AddText(ImVec2(badge_min.x + 9, badge_min.y + 4), ColA(Config::capturing_menu_key ? Menu::Palette::Accent() : Menu::Palette::Text(), alpha), mk_label.c_str( ));

    ImGui::SetCursorScreenPos(badge_min);
    if (ImGui::InvisibleButton("##menu_key_badge", ImVec2(badge_w, badge_h)))
        { Config::capturing_module_key=-1; Config::capturing_menu_key = !Config::capturing_menu_key; }

    draw_list->AddLine(ImVec2(area_min.x + 20, row_y + 44), ImVec2(area_max.x - 20, row_y + 44), ColA(Menu::Palette::Border(), alpha));

    // --- Partikel-Toggle ---
    row_y += 64.0f;
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 3), ColA(Menu::Palette::Label(), alpha), "Background Particles");
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 20), ColA(Menu::Palette::Muted(), alpha), "Toggle the floating particle effect.");

    float tw = 38.0f, th = 21.0f;
    ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20 + row_w - tw, row_y + 2.0f));
    CustomToggle("##settings_particles", &Config::particles_enabled, alpha, tw, th);

    row_y += 68.0f;
    draw_list->AddLine(ImVec2(area_min.x+20,row_y-8), ImVec2(area_max.x-20,row_y-8), ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,row_y), ColA(Menu::Palette::Label(),alpha), "Color Theme");
    const float themeWidth=(row_w-20.0f)/3.0f;
    for(int t=0;t<3;++t) {
        const auto& palette=Theme::presets[t];
        ImGui::PushID(t);
        ImVec2 pos(area_min.x+20+t*(themeWidth+10),row_y+26);
        ImVec2 end(pos.x+themeWidth,pos.y+66);
        ImGui::SetCursorScreenPos(pos);
        if(ImGui::InvisibleButton("##theme",ImVec2(themeWidth,66))) Theme::selected=t;
        bool active=Theme::selected==t;
        draw_list->AddRectFilled(pos,end,ColA(palette.background,alpha),9);
        draw_list->AddRect(pos,end,ColA(active?palette.accent:palette.border,alpha),9,0,active?2.0f:1.0f);
        draw_list->AddText(ImVec2(pos.x+12,pos.y+10),ColA(palette.text,alpha),palette.name);
        for(int swatch=0;swatch<3;++swatch) {
            ImU32 color=swatch==0?palette.accent:swatch==1?palette.soft:palette.card;
            draw_list->AddCircleFilled(ImVec2(pos.x+18+swatch*21,pos.y+45),6,ColA(color,alpha));
        }
        if(active) draw_list->AddCircleFilled(ImVec2(end.x-14,pos.y+45),3,ColA(palette.accent,alpha));
        ImGui::PopID();
    }

    const float unloadY=row_y+112.0f;
    draw_list->AddLine(ImVec2(area_min.x+20,unloadY-10),ImVec2(area_min.x+area_w-20,unloadY-10),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,unloadY),ColA(Menu::Palette::Label(),alpha),"Unload Client");
    draw_list->AddText(ImVec2(area_min.x+20,unloadY+23),ColA(Menu::Palette::Muted(),alpha),"Hotkey: Ende");
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+area_w-170,unloadY));
    if(DrawRoundedButton(draw_list,"##unload_client","Unload DLL",ImVec2(150,36),
        Menu::Palette::Soft(),Theme::Current().border,Menu::Palette::Accent(),alpha,8)) Base::Unload();
    float hudY=unloadY+70;
    draw_list->AddLine(ImVec2(area_min.x+20,hudY),ImVec2(area_max.x-20,hudY),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,hudY+16),ColA(Menu::Palette::Label(),alpha),"Status HUD");
    draw_list->AddText(ImVec2(area_min.x+20,hudY+39),ColA(Menu::Palette::Muted(),alpha),"Drag its header while the menu is open. Position is saved in profiles.");
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-160,hudY+10));
    if(DrawRoundedButton(draw_list,"##reset_hud","Reset",ImVec2(80,28),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6)) {
        Config::hud_x=20; Config::hud_y=20;
    }
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-58,hudY+14));
    CustomToggle("##hud_enabled",&Config::hud_enabled,alpha,38,21);
    float hotkeyY=hudY+76;
    draw_list->AddLine(ImVec2(area_min.x+20,hotkeyY),ImVec2(area_max.x-20,hotkeyY),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,hotkeyY+16),ColA(Menu::Palette::Text(),alpha),"Module Hotkeys");
    draw_list->AddText(ImVec2(area_min.x+20,hotkeyY+38),ColA(Menu::Palette::Muted(),alpha),"Esc: cancel | Backspace: clear | End: reserved for unload");
    hotkeyY+=68;
    auto modules=Registry::GetModules();
    for (int i=0;i<static_cast<int>(modules.size());++i) {
        auto& module=modules[i];
        ImGui::PushID(i);
        bool conflict=Registry::HasConflict(module.hotkey,i);
        draw_list->AddText(ImVec2(area_min.x+20,hotkeyY+8),ColA(Menu::Palette::Label(),alpha),module.name);
        if (conflict) draw_list->AddText(ImVec2(area_min.x+20,hotkeyY+29),ColA(Menu::Palette::Accent(),alpha),"Key conflict");
        float x=area_max.x-304;
        auto button=[&](const char* id,const char* label,float width) {
            ImGui::SetCursorScreenPos(ImVec2(x,hotkeyY));
            bool clicked=DrawRoundedButton(draw_list,id,label,ImVec2(width,30),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6);
            x+=width+8;
            return clicked;
        };
        std::string label=Config::capturing_module_key==i ? "Press key..." : module.hotkey ? GetKeyName(module.hotkey) : "Set key";
        if (button("key",label.c_str(),104)) {
            Config::capturing_menu_key=false;
            Config::capturing_module_key=Config::capturing_module_key==i ? -1 : i;
        }
        if (module.enabled==&ShortWindCharge_Enabled || module.enabled==&AutoPearlCatch_Enabled) {
            button("mode","Action",80);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable the module, then press its key once per action. Holding does not repeat.");
        } else {
            if (button("mode",module.hotkey_mode==Input::HotkeyMode::Toggle ? "Toggle" : "Hold",80))
                module.hotkey_mode=module.hotkey_mode==Input::HotkeyMode::Toggle ? Input::HotkeyMode::Hold : Input::HotkeyMode::Toggle;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hold temporarily enables the module; release restores its previous state.");
        }
        if (button("clear","Clear",64)) { module.hotkey=0; if(Config::capturing_module_key==i) Config::capturing_module_key=-1; }
        hotkeyY+=56;
        ImGui::PopID();
    }
    draw_list->AddLine(ImVec2(area_min.x+20,hotkeyY),ImVec2(area_max.x-20,hotkeyY),ColA(Menu::Palette::Border(),alpha));
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,hotkeyY+16));
    ImGui::TextUnformatted("Performance / Diagnostics");
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,hotkeyY+42));
    ImGui::TextDisabled("CPU preparation only; excludes GPU and background cracking.");
    const char* labels[]={"Client total", "OreSim", "Player ESP"};
    for(int i=0;i<Performance::Count;++i) {
        const auto stats=Performance::samples[i].Read();
        ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,hotkeyY+68+i*24));
        ImGui::Text("%s: %.3f ms avg / %.3f ms peak",labels[i],stats.average,stats.peak);
    }
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,hotkeyY+146));
    ImGui::TextDisabled("Last %d frames. Total includes OreSim and ESP.",static_cast<int>(Performance::samples[0].size));
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-288,hotkeyY+170));
    if(DrawRoundedButton(draw_list,"##export_diagnostics","Export report",ImVec2(156,28),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6)) Diagnostics::Export();
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-120,hotkeyY+170));
    if(DrawRoundedButton(draw_list,"##reset_perf","Reset",ImVec2(100,28),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6)) Performance::Reset();
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,hotkeyY+210));
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row_w);
    ImGui::TextWrapped("%s",Diagnostics::status.empty() ? "Export saves a local text report with timings and module settings." : Diagnostics::status.c_str());
    ImGui::PopTextWrapPos();
    if(!Diagnostics::lastPath.empty()) {
        if(ImGui::SmallButton("Copy report path")) ImGui::SetClipboardText(Diagnostics::lastPath.c_str());
    }

    ImGui::Dummy(ImVec2(1,1));
    ImGui::EndChild();

}
}
