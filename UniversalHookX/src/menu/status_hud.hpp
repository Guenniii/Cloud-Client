#pragma once
#include <Windows.h>
#include "watermark.hpp"
#include "../utils/config/config.hpp"
#include "../utils/theme/theme.hpp"
#include "../modules/settings.hpp"
#include "module_registry.hpp"
#include "module_view.hpp"
#include "resources.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>

// Info HUD module: Orbit-style list of all other active modules. Render thread only (Menu::Render).
namespace Menu::StatusHud {
inline void Draw() {
    Watermark::Draw();
    if (!InfoHUD_Enabled) return;
    const auto modules = Registry::GetModules();
    static std::vector<float> anim; // per-module slide/fade, indexed like GetModules()
    static float width = 0.f;
    if (anim.size() != modules.size()) anim.assign(modules.size(), 0.f);
    const float s = std::clamp(InfoHUD_Scale.load(), 75, 150) / 100.f;
    const float k = (std::min)((std::min)(ImGui::GetIO().DeltaTime, 0.1f) * 12.f, 1.f); // dt clamped: no jump after a hitch
    const float tpx = 13.f * s, mpx = 11.f * s, ipx = 11.f * s, pad = 10.f * s, headH = 28.f * s, top = headH + 4.f * s;
    ImFont* bold = NearestOrbitFont(orbit_text_bold, tpx, nullptr);
    ImFont* text = NearestOrbitFont(orbit_text, tpx, nullptr);
    ImFont* mono = NearestOrbitFont(orbit_mono, mpx, nullptr);

    struct Row { int index; float w; };
    static std::vector<Row> rows;
    rows.clear();
    for (size_t i = 0; i < modules.size(); ++i) {
        const auto& m = modules[i];
        if (m.enabled == &InfoHUD_Enabled) continue;
        anim[i] += ((m.enabled->load() ? 1.f : 0.f) - anim[i]) * k;
        if (anim[i] >= 0.01f) rows.push_back({static_cast<int>(i), text->CalcTextSizeA(tpx, FLT_MAX, 0.f, m.name).x});
    }
    std::sort(rows.begin(), rows.end(), [&](const Row& a, const Row& b) {
        return a.w != b.w ? a.w > b.w : std::string_view(modules[a.index].name) < modules[b.index].name;
    });

    char info[48] = "";
    if (InfoHUD_Clock) {
        SYSTEMTIME t; GetLocalTime(&t);
        std::snprintf(info, sizeof(info), "%d FPS \xC2\xB7 %02d:%02d", static_cast<int>(ImGui::GetIO().Framerate + 0.5f), t.wHour, t.wMinute);
    }
    const float infoW = info[0] ? mono->CalcTextSizeA(mpx, FLT_MAX, 0.f, info).x : 0.f;
    float target = (std::max)(150.f * s, pad + 12.f * s + bold->CalcTextSizeA(tpx, FLT_MAX, 0.f, "FUSION").x + (info[0] ? 16.f * s + infoW : 0.f) + pad);
    float rowsH = 0.f;
    for (const auto& r : rows) { target = (std::max)(target, pad + 22.f * s + r.w + pad); rowsH += 22.f * s * anim[r.index]; }
    rowsH = (std::max)(rowsH, 22.f * s); // at least one row: no height pop between the empty state and the first row
    width = width <= 0.f ? target : width + (target - width) * k;

    const auto* vp = ImGui::GetMainViewport();
    const float w = (std::max)(1.f, (std::min)(width, vp->WorkSize.x));
    const float h = (std::max)(1.f, (std::min)(top + rowsH + 8.f * s, vp->WorkSize.y));
    const int maxX = (std::max)(0, static_cast<int>(vp->WorkSize.x - w));
    const int maxY = (std::max)(0, static_cast<int>(vp->WorkSize.y - h));
    // clamp locally (watermark pattern): writing back while the size animates would drift the saved position and spam undo
    const int px = std::clamp(Config::hud_x, 0, maxX), py = std::clamp(Config::hud_y, 0, maxY);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + px, vp->WorkPos.y + py));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (!Menu_Enabled) flags |= ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("##status_hud", nullptr, flags)) {
        const ImVec2 o = ImGui::GetWindowPos();
        if (Menu_Enabled) {
            ImGui::InvisibleButton("##drag", ImVec2(w, h));
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                Config::hud_x = std::clamp(px + static_cast<int>(ImGui::GetIO().MouseDelta.x), 0, maxX);
                Config::hud_y = std::clamp(py + static_cast<int>(ImGui::GetIO().MouseDelta.y), 0, maxY);
            }
        }
        const auto& p = Theme::Current();
        auto* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), Widgets::ColA(p.card, 0.88f), 10.f * s);
        dl->AddRect(o, ImVec2(o.x + w, o.y + h), p.border, 10.f * s);
        const float hy = o.y + headH * 0.5f;
        dl->AddCircleFilled(ImVec2(o.x + pad + 3.f * s, hy), 3.f * s, p.accent);
        dl->AddText(bold, tpx, ImVec2(std::floor(o.x + pad + 12.f * s), std::floor(hy - tpx * 0.5f)), p.text, "FUSION");
        if (info[0]) dl->AddText(mono, mpx, ImVec2(std::floor(o.x + w - pad - infoW), std::floor(hy - mpx * 0.5f)), p.muted, info);
        dl->AddLine(ImVec2(o.x + 1.f, o.y + headH), ImVec2(o.x + w - 1.f, o.y + headH), p.border);
        dl->AddRectFilled(ImVec2(o.x + pad, o.y + headH - 1.f), ImVec2(o.x + pad + 40.f * s, o.y + headH + 1.f), p.accent);
        float y = o.y + top;
        for (const auto& r : rows) {
            const auto& m = modules[r.index];
            const float a = anim[r.index], rh = 22.f * s * a, cy = y + rh * 0.5f, x = o.x + pad + (1.f - a) * 12.f * s;
            dl->PushClipRect(ImVec2(o.x, y), ImVec2(o.x + w, y + rh), true); // a sliding row must not overlap its neighbours
            dl->AddRectFilled(ImVec2(x, cy - 7.f * s), ImVec2(x + 2.f * s, cy + 7.f * s), Widgets::ColA(p.accent, a));
            if (icons) {
                const char* icon = ModuleView::ModuleIcon(m);
                const float iw = icons->CalcTextSizeA(ipx, FLT_MAX, 0.f, icon).x;
                dl->AddText(icons, ipx, ImVec2(std::floor(x + 12.f * s - iw * 0.5f), std::floor(cy - ipx * 0.5f)), Widgets::ColA(p.accent, a), icon);
            }
            dl->AddText(text, tpx, ImVec2(std::floor(x + 22.f * s), std::floor(cy - tpx * 0.5f)), Widgets::ColA(p.text, a), m.name);
            dl->PopClipRect();
            y += rh;
        }
        if (rows.empty()) dl->AddText(mono, mpx, ImVec2(std::floor(o.x + pad), std::floor(y + 11.f * s - mpx * 0.5f)), p.muted, "No active modules");
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
}
}
