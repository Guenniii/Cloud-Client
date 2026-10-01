#pragma once
#include "../dependencies/imgui/imgui.h"

struct ModuleData;

namespace Menu::Pages {
    void DrawConfigPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha, ModuleData* modules, int moduleCount);
    void DrawSettingsPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha);
    void DrawSeedCrackerPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha);
}
