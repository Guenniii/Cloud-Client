#pragma once
#include <atomic>
#include "../dependencies/imgui/imgui.h"

namespace Menu::Widgets {
    ImU32 ColA(ImU32 col, float mult);
    ImVec4 ColV(ImU32 col, float mult);
    void DrawGlowCircle(ImDrawList* draw_list, ImVec2 center, float radius, ImU32 color, float alpha, int layers = 8);
    void DrawGlowRect(ImDrawList* draw_list, ImVec2 min, ImVec2 max, float rounding, ImU32 color, float alpha, int layers = 8);
    bool CustomToggle(const char* label, bool* v, float alpha = 1.0f, float width = 44.0f, float height = 24.0f);
    bool DrawRoundedButton(ImDrawList* draw_list, const char* str_id, const char* label,
                              ImVec2 size, ImU32 bg_col, ImU32 bg_hover_col, ImU32 text_col,
                              float alpha, float rounding = 8.0f, bool glow = false);
    void DrawCheckboxRow(ImDrawList* draw_list, ImVec2 pos, float width, const char* label, bool* v, float alpha);
    void DrawSliderRow(ImDrawList* draw_list, ImVec2 pos, float width, const char* label, int* v, int vmin, int vmax, const char* suffix, float alpha);
}

namespace Menu::Widgets {
    bool CustomToggle(const char*, std::atomic<bool>*, float alpha = 1.0f, float width = 44.0f, float height = 24.0f);
    void DrawCheckboxRow(ImDrawList*, ImVec2, float, const char*, std::atomic<bool>*, float);
    void DrawSliderRow(ImDrawList*, ImVec2, float, const char*, std::atomic<int>*, int, int, const char*, float);
}
