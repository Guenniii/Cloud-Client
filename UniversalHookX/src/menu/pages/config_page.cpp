#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../dependencies/imgui/imgui_internal.h"
#include <cstdio>
#include "../../utils/config/config.hpp"

namespace Menu::Pages {
using namespace Menu::Widgets;

static char new_config_name[65] = "";

void DrawConfigPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha, ModuleData* modules, int moduleCount) {
    ImVec2 area_max = ImVec2(area_min.x + area_w, area_min.y + area_h);

    draw_list->AddRectFilled(area_min, area_max, ColA(Menu::Palette::Card(), alpha), 12.0f);
    draw_list->AddRect(area_min, area_max, ColA(Menu::Palette::Border(), alpha), 12.0f, 0, 1.0f);

    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 18), ColA(Menu::Palette::Text(), alpha), "Profiles");
    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 40), ColA(Menu::Palette::Muted(), alpha), "Modules, hotkeys, theme and manual OreSim seed.");

    // --- Neuer Config-Name + Save-Button ---
    ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20, area_min.y + 70));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ColV(Menu::Palette::Background(), alpha));
    ImGui::PushStyleColor(ImGuiCol_Text, ColV(Menu::Palette::Text(), alpha));
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##new_config", "Profile name", new_config_name, sizeof(new_config_name));
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0, 12);
    if (DrawRoundedButton(draw_list, "##save_config", "Save", ImVec2(90.0f, 32.0f),
                          Menu::Palette::Accent(), Theme::Current().hover, Theme::Current().onAccent,
                          alpha, 8.0f, true)) {
        std::string name = new_config_name;
        if (!name.empty( ))
            Config::Save(name, modules, moduleCount);
    }

    draw_list->AddLine(ImVec2(area_min.x + 20, area_min.y + 106), ImVec2(area_max.x - 20, area_min.y + 106), ColA(Menu::Palette::Border(), alpha));
    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 118), ColA(Menu::Palette::Muted(), alpha), Config::profile_status.empty() ? "Saved profiles" : Config::profile_status.c_str());

    // --- Scrollbare Liste der gespeicherten Configs ---
    ImGui::SetCursorScreenPos(ImVec2(area_min.x + 16, area_min.y + 142));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("##config_list", ImVec2(area_w - 32.0f, area_h - 158.0f), false, ImGuiWindowFlags_NoScrollbar);

    const std::vector<std::string> config_list = Config::GetList( );
    if (config_list.empty( )) {
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(Menu::Palette::Muted(), alpha));
        ImGui::TextWrapped("No profiles yet. Enter a name above and save your current settings.");
        ImGui::PopStyleColor( );
    }

    for (size_t i = 0; i < config_list.size( ); i++) {
        ImGui::PushID((int)i);

        ImVec2 row_p = ImGui::GetCursorScreenPos( );
        float row_w = area_w - 32.0f;
        float row_h = 34.0f;

        draw_list->AddRectFilled(row_p, ImVec2(row_p.x + row_w, row_p.y + row_h), ColA(Menu::Palette::Background(), alpha), 8.0f);
        draw_list->AddText(ImVec2(row_p.x + 12, row_p.y + 9), ColA(Menu::Palette::Text(), alpha), (config_list[i] + (Config::active_profile==config_list[i] ? "  *" : "")).c_str());

        float load_w = 52.0f, del_w = 58.0f, btn_h = 24.0f, btn_gap = 8.0f;
        ImVec2 load_pos = ImVec2(row_p.x + row_w - load_w - del_w - btn_gap - 12.0f, row_p.y + (row_h - btn_h) * 0.5f);
        ImVec2 del_pos = ImVec2(load_pos.x + load_w + btn_gap, load_pos.y);

        ImGui::SetCursorScreenPos(load_pos);
        if (DrawRoundedButton(draw_list, "##load_btn", "Load", ImVec2(load_w, btn_h),
                              Menu::Palette::Soft(), Theme::Current().border, Menu::Palette::Accent(), alpha, 7.0f))
            Config::Load(config_list[i], modules, moduleCount);

        ImGui::SetCursorScreenPos(del_pos);
        if (DrawRoundedButton(draw_list, "##delete_btn", "Delete", ImVec2(del_w, btn_h),
                              IM_COL32(255, 225, 225, 255), IM_COL32(255, 205, 205, 255), IM_COL32(200, 50, 50, 255), alpha, 7.0f))
            Config::Delete(config_list[i]);

        ImGui::SetCursorScreenPos(ImVec2(row_p.x, row_p.y + row_h + 8.0f));
        ImGui::PopID( );
    }

    ImGui::EndChild( );
    ImGui::PopStyleColor( );
}
}
