#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../dependencies/imgui/imgui_internal.h"
#include <cstdio>
#include "../../base.hpp"
#include "../../utils/config/config.hpp"
#include "../../utils/sdk/game_runtime.hpp"
#include "../../modules/settings.hpp"
#include "../key_names.hpp"

namespace Menu::Pages {
using namespace Menu::Widgets;

void DrawSettingsPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha) {
    ImVec2 area_max = ImVec2(area_min.x + area_w, area_min.y + area_h);

    draw_list->AddRectFilled(area_min, area_max, ColA(Menu::Palette::Card(), alpha), 12.0f);
    draw_list->AddRect(area_min, area_max, ColA(Menu::Palette::Border(), alpha), 12.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(area_min);
    ImGui::BeginChild("SettingsContent",ImVec2(area_w,area_h),false,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    area_min=ImGui::GetCursorScreenPos();
    area_w=ImGui::GetContentRegionAvail().x;
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

    // --- Hotkey toasts ---
    row_y += 64.0f;
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 3), ColA(Menu::Palette::Label(), alpha), "Hotkey toasts");
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 20), ColA(Menu::Palette::Muted(), alpha), "Show \"<module> on/off\" when a hotkey toggles a module.");
    ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20 + row_w - tw, row_y + 2.0f));
    CustomToggle("##settings_hotkey_toasts", &Config::hotkey_toasts, alpha, tw, th);

    // --- Menu style (this page is shared by all three menus; the change takes effect next frame) ---
    row_y += 64.0f;
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 3), ColA(Menu::Palette::Label(), alpha), "Menu style");
    {
        const char* style_names[3] = {"Classic", "Click GUI", "Orbit"};
        float seg_w[3], total_w = 4.0f;
        for (int i = 0; i < 3; ++i) { seg_w[i] = ImGui::CalcTextSize(style_names[i]).x + 18.0f; total_w += seg_w[i]; }
        const ImVec2 smin(area_min.x + 20 + row_w - total_w, row_y - 2.0f), smax(smin.x + total_w, smin.y + 28.0f);
        // narrow areas (Orbit at 80 %): drop the description instead of running under the control
        const char* style_desc = "Saved in your profile.";
        if (area_min.x + 20 + ImGui::CalcTextSize(style_desc).x + 10.0f < smin.x)
            draw_list->AddText(ImVec2(area_min.x + 20, row_y + 20), ColA(Menu::Palette::Muted(), alpha), style_desc);
        draw_list->AddRectFilled(smin, smax, ColA(Menu::Palette::Background(), alpha), 8.0f);
        draw_list->AddRect(smin, smax, ColA(Menu::Palette::Border(), alpha), 8.0f);
        float sx = smin.x + 2.0f;
        for (int i = 0; i < 3; ++i) {
            ImGui::SetCursorScreenPos(ImVec2(sx, smin.y + 2.0f));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##menu_style", ImVec2(seg_w[i], 24.0f))) Config::menu_style = i;
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool active = Config::menu_style == i;
            if (active || hovered)
                draw_list->AddRectFilled(ImVec2(sx, smin.y + 2.0f), ImVec2(sx + seg_w[i], smin.y + 26.0f), ColA(Menu::Palette::Soft(), alpha * (active ? 1.0f : 0.5f)), 6.0f);
            draw_list->AddText(ImVec2(sx + 9.0f, smin.y + 5.0f), ColA(active ? Menu::Palette::Accent() : Menu::Palette::Label(), alpha), style_names[i]);
            sx += seg_w[i];
        }
    }

    row_y += 64.0f;
    draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Interface size");
    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 22), ColA(Menu::Palette::Muted(), alpha), "Menu dimensions and text size are saved in your profile.");
    row_y += 46.0f;
    DrawSliderRow(draw_list, ImVec2(area_min.x + 20, row_y), row_w, "Menu size", &Config::menu_size_percent, 80, 120, "%", alpha);
    row_y += 54.0f;
    DrawSliderRow(draw_list, ImVec2(area_min.x + 20, row_y), row_w, "Text size", &Config::font_size_percent, 90, 115, "%", alpha);
    const float unloadY=row_y+76.0f;
    draw_list->AddLine(ImVec2(area_min.x+20,unloadY-10),ImVec2(area_min.x+area_w-20,unloadY-10),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,unloadY),ColA(Menu::Palette::Label(),alpha),"Unload Client");
    draw_list->AddText(ImVec2(area_min.x+20,unloadY+23),ColA(Menu::Palette::Muted(),alpha),"Hotkey: End");
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+area_w-170,unloadY));
    if(DrawRoundedButton(draw_list,"##unload_client","Unload DLL",ImVec2(150,36),
        Menu::Palette::Soft(),Theme::Current().border,Menu::Palette::Accent(),alpha,8)) Base::Unload();
    float hudY=unloadY+70;
    draw_list->AddLine(ImVec2(area_min.x+20,hudY),ImVec2(area_max.x-20,hudY),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,hudY+16),ColA(Menu::Palette::Label(),alpha),"Info HUD");
    draw_list->AddText(ImVec2(area_min.x+20,hudY+39),ColA(Menu::Palette::Muted(),alpha),"Drag its header while the menu is open. Position is saved in profiles.");
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-160,hudY+10));
    if(DrawRoundedButton(draw_list,"##reset_hud","Reset",ImVec2(80,28),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6)) {
        Config::hud_x=20; Config::hud_y=20;
    }
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-58,hudY+14));
    CustomToggle("##hud_enabled",&Config::hud_enabled,alpha,38,21);
    const float watermarkY=hudY+76;
    draw_list->AddLine(ImVec2(area_min.x+20,watermarkY),ImVec2(area_max.x-20,watermarkY),ColA(Menu::Palette::Border(),alpha));
    draw_list->AddText(ImVec2(area_min.x+20,watermarkY+16),ColA(Menu::Palette::Label(),alpha),"Logo watermark");
    draw_list->AddText(ImVec2(area_min.x+20,watermarkY+39),ColA(Menu::Palette::Muted(),alpha),"Logo only. Drag it while the menu is open.");
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-160,watermarkY+10));
    if(DrawRoundedButton(draw_list,"##reset_watermark","Reset",ImVec2(80,28),Menu::Palette::Soft(),Menu::Palette::Border(),Menu::Palette::Accent(),alpha,6)) {
        Config::watermark_x=-1;Config::watermark_y=20;
    }
    ImGui::SetCursorScreenPos(ImVec2(area_max.x-58,watermarkY+14));
    CustomToggle("##watermark_enabled",&Config::watermark_enabled,alpha,38,21);
    const float runtimeY=watermarkY+76;
    draw_list->AddLine(ImVec2(area_min.x+20,runtimeY),ImVec2(area_max.x-20,runtimeY),ColA(Menu::Palette::Border(),alpha));
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,runtimeY+16));
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+row_w);
    const auto runtime=GameRuntime::Info();
    ImGui::TextUnformatted("Runtime compatibility");
    ImGui::TextWrapped("Minecraft: %s | %s",runtime.version.c_str(),runtime.fabric?"Fabric":"Vanilla / other loader");
    ImGui::TextWrapped("Namespace: %s",runtime.nameSpace.c_str());
    ImGui::TextWrapped("Class loader: %s",runtime.loader.empty()?"Not resolved":runtime.loader.c_str());
    ImGui::TextDisabled("26.2 base / 26.3 compatibility overlay selected automatically.");
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(1,1));
    ImGui::EndChild();

}
}
