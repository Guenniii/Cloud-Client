#include "widgets.hpp"
#include "palette.hpp"
#include "../dependencies/imgui/imgui_internal.h"
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace Menu::Widgets {
ImU32 ColA(ImU32 col, float mult) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    c.w *= mult;
    return ImGui::ColorConvertFloat4ToU32(c);
}

ImVec4 ColV(ImU32 col, float mult) {
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    c.w *= mult;
    return c;
}

// Nested translucent shapes approximate a soft halo without textures or a shader pass.
void DrawGlowCircle(ImDrawList* draw_list, ImVec2 center, float radius, ImU32 color, float alpha, int layers) {
    if (!draw_list || radius <= 0.0f || alpha <= 0.0f) return;
    const ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);
    const int count = std::clamp(layers, 12, 20);
    const float spread = (std::max)(10.0f, radius * 0.9f);
    const float peak = 0.32f * std::clamp(alpha, 0.0f, 1.0f) * base.w;
    float previous = 0.0f;
    for (int i = count; i >= 1; --i) {
        const float t = static_cast<float>(i) / count;
        const float opacity = peak * std::exp(-4.5f * t * t);
        ImVec4 layer = base;
        layer.w = (opacity - previous) / (1.0f - previous);
        draw_list->AddCircleFilled(center, radius + t * spread, ImGui::ColorConvertFloat4ToU32(layer), 48);
        previous = opacity;
    }
}

void DrawGlowRect(ImDrawList* draw_list, ImVec2 min, ImVec2 max, float rounding, ImU32 color, float alpha, int layers) {
    if (!draw_list || max.x <= min.x || max.y <= min.y || alpha <= 0.0f) return;
    const ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);
    const int count = std::clamp(layers, 12, 20);
    const float peak = 0.18f * std::clamp(alpha, 0.0f, 1.0f) * base.w;
    float previous = 0.0f;
    for (int i = count; i >= 1; --i) {
        const float t = static_cast<float>(i) / count;
        const float expand = t * 9.0f;
        const float opacity = peak * std::exp(-4.5f * t * t);
        ImVec4 layer = base;
        layer.w = (opacity - previous) / (1.0f - previous);
        draw_list->AddRectFilled(ImVec2(min.x - expand, min.y - expand),
            ImVec2(max.x + expand, max.y + expand), ImGui::ColorConvertFloat4ToU32(layer), rounding + expand);
        previous = opacity;
    }
}

bool CustomToggle(const char* label, bool* v, float alpha, float width, float height) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImVec2 p = window->DC.CursorPos;

    float radius = height * 0.5f;

    const ImRect bb(p, ImVec2(p.x + width, p.y + height));
    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed)
        *v = !(*v);

    static std::unordered_map<ImGuiID, float> anim_state;
    float& t = anim_state[id];

    float target = *v ? 1.0f : 0.0f;
    float speed = 8.0f;
    t = t + (target - t) * ImSaturate(g.IO.DeltaTime * speed);

    ImDrawList* draw_list = window->DrawList;

    ImVec4 col_off = ImGui::ColorConvertU32ToFloat4(Menu::Palette::Toggle());
    ImVec4 col_on = ImGui::ColorConvertU32ToFloat4(Menu::Palette::Accent());
    ImVec4 col_lerp = ImLerp(col_off, col_on, t);
    col_lerp.w *= alpha;
    ImU32 col_bg = ImGui::ColorConvertFloat4ToU32(col_lerp);

    if (t > 0.01f) {
        DrawGlowRect(draw_list, bb.Min, bb.Max, radius, Menu::Palette::Accent(), t * alpha, 3);
    }
    draw_list->AddRectFilled(bb.Min, bb.Max, col_bg, radius);
    float circle_radius = radius - 2.5f;
    ImVec2 circle_center = ImVec2(p.x + radius + t * (width - radius * 2.0f), p.y + radius);
    if (t > 0.01f)
        DrawGlowCircle(draw_list, circle_center, circle_radius, Menu::Palette::Accent(), t * alpha, 12);
    draw_list->AddCircleFilled(circle_center, circle_radius, IM_COL32(255, 255, 255, (int)(255 * alpha)));

    return pressed;
}

bool DrawRoundedButton(ImDrawList* draw_list, const char* str_id, const char* label,
                              ImVec2 size, ImU32 bg_col, ImU32 bg_hover_col, ImU32 text_col,
                              float alpha, float rounding, bool glow) {
    ImGuiWindow* window = ImGui::GetCurrentWindow( );
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImVec2 p = window->DC.CursorPos;
    const ImGuiID id = window->GetID(str_id);

    const ImRect bb(p, ImVec2(p.x + size.x, p.y + size.y));
    ImGui::ItemSize(bb, g.Style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    static std::unordered_map<ImGuiID, float> hover_anim;
    float& t = hover_anim[id];
    float target = (hovered || held) ? 1.0f : 0.0f;
    t += (target - t) * ImSaturate(g.IO.DeltaTime * 12.0f);

    ImVec4 col_off = ImGui::ColorConvertU32ToFloat4(bg_col);
    ImVec4 col_on = ImGui::ColorConvertU32ToFloat4(bg_hover_col);
    ImVec4 col_lerp = ImLerp(col_off, col_on, t);
    if (held) {
        col_lerp.x *= 0.9f;
        col_lerp.y *= 0.9f;
        col_lerp.z *= 0.9f;
    }
    col_lerp.w *= alpha;

    if (glow && t > 0.01f)
        DrawGlowRect(draw_list, bb.Min, bb.Max, rounding, bg_hover_col, t * alpha, 3);

    draw_list->AddRectFilled(bb.Min, bb.Max, ImGui::ColorConvertFloat4ToU32(col_lerp), rounding);

    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 text_pos = ImVec2(bb.Min.x + (size.x - text_size.x) * 0.5f, bb.Min.y + (size.y - text_size.y) * 0.5f);
    draw_list->AddText(text_pos, ColA(text_col, alpha), label);

    return pressed;
}

void DrawCheckboxRow(ImDrawList* draw_list, ImVec2 pos, float width, const char* label, bool* v, float alpha) {
    draw_list->AddText(ImVec2(pos.x, pos.y + 5), ColA(Menu::Palette::Label(), alpha), label);

    float toggle_w = 34.0f, toggle_h = 19.0f;
    ImGui::SetCursorScreenPos(ImVec2(pos.x + width - toggle_w, pos.y - 2.0f));
    CustomToggle("##row_toggle", v, alpha, toggle_w, toggle_h);
}

void DrawSliderRow(ImDrawList* draw_list, ImVec2 pos, float width, const char* label, int* v, int vmin, int vmax, const char* suffix, float alpha) {
    draw_list->AddText(ImVec2(pos.x, pos.y), ColA(Menu::Palette::Label(), alpha), label);

    char val_buf[32];
    snprintf(val_buf, sizeof(val_buf), "%d%s", *v, suffix ? suffix : "");
    ImVec2 val_size = ImGui::CalcTextSize(val_buf);
    draw_list->AddText(ImVec2(pos.x + width - val_size.x, pos.y), ColA(Menu::Palette::Accent(), alpha), val_buf);

    // Unsichtbar gestyltes SliderInt nur fÃƒÂ¼r die Interaktion (Drag/Klick)
    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + 22.0f));
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 1.0f);
    ImGui::SliderInt("##slider", v, vmin, vmax, "");
    ImVec2 rmin = ImGui::GetItemRectMin( );
    ImVec2 rmax = ImGui::GetItemRectMax( );
    ImGui::PopStyleVar( );
    ImGui::PopStyleColor(6);

    // Eigene Track-Darstellung darÃƒÂ¼berzeichnen
    float track_y = (rmin.y + rmax.y) * 0.5f;
    float track_h = 4.0f;
    float t = vmax > vmin ? (float)(*v - vmin) / (float)(vmax - vmin) : 0.0f;
    float handle_x = rmin.x + t * (rmax.x - rmin.x);

    if (handle_x > rmin.x + 2.0f) {
        DrawGlowRect(draw_list, ImVec2(rmin.x, track_y - track_h * 0.5f), ImVec2(handle_x, track_y + track_h * 0.5f), track_h * 0.5f, Menu::Palette::Accent(), alpha * 0.35f, 12);
    }
    draw_list->AddRectFilled(ImVec2(rmin.x, track_y - track_h * 0.5f), ImVec2(rmax.x, track_y + track_h * 0.5f), ColA(Menu::Palette::Toggle(), alpha), track_h * 0.5f);
    draw_list->AddRectFilled(ImVec2(rmin.x, track_y - track_h * 0.5f), ImVec2(handle_x, track_y + track_h * 0.5f), ColA(Menu::Palette::Accent(), alpha), track_h * 0.5f);
    // Colored core with a soft halo, slightly stronger during interaction.
    const bool interacting = ImGui::IsItemHovered() || ImGui::IsItemActive();
    const ImVec2 handle(handle_x, track_y);
    const float handle_radius = 5.5f;
    DrawGlowCircle(draw_list, handle, handle_radius, Menu::Palette::Accent(), alpha * (interacting ? 1.0f : 0.85f), 12);
    draw_list->AddCircleFilled(handle, handle_radius, ColA(Menu::Palette::Accent(), alpha), 32);
    draw_list->AddCircleFilled(ImVec2(handle.x - 1.3f, handle.y - 1.5f), 1.5f,
        ColA(IM_COL32(255, 255, 255, 255), alpha * 0.25f), 16);
}

}

namespace Menu::Widgets {
bool CustomToggle(const char* label, std::atomic<bool>* value, float alpha, float width, float height) {
    bool local = value->load();
    bool changed = CustomToggle(label, &local, alpha, width, height);
    if (changed) value->store(local);
    return changed;
}
void DrawCheckboxRow(ImDrawList* draw, ImVec2 pos, float width, const char* label, std::atomic<bool>* value, float alpha) {
    bool before = value->load(), local = before;
    DrawCheckboxRow(draw, pos, width, label, &local, alpha);
    if (local != before) value->store(local);
}
void DrawSliderRow(ImDrawList* draw, ImVec2 pos, float width, const char* label, std::atomic<int>* value, int min, int max, const char* suffix, float alpha) {
    int before = value->load(), local = before;
    DrawSliderRow(draw, pos, width, label, &local, min, max, suffix, alpha);
    if (local != before) value->store(local);
}
}
