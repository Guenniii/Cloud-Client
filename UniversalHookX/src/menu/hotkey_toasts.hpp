#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "../dependencies/imgui/imgui.h"
#include "../utils/config/config.hpp"
#include "../utils/theme/theme.hpp"
#include "resources.hpp"

// "<Module> on/off" toasts after a hotkey toggle. Push (ProcessHotkeys) and Draw both run in Menu::Render,
// i.e. on the render thread only, so the ring buffer needs no lock.
namespace Menu::HotkeyToasts {
struct Toast { char text[72]; double at; bool on; };
inline Toast ring[3]; // last three toggles, oldest overwritten
inline int next = 0, used = 0;
constexpr double Lifetime = 1.5;

inline void Push(const char* name, bool on) {
    if (!Config::hotkey_toasts) return;
    auto& t = ring[next];
    std::snprintf(t.text, sizeof(t.text), "%s %s", name, on ? "on" : "off");
    t.at = ImGui::GetTime(); t.on = on;
    next = (next + 1) % 3; used = (std::min)(used + 1, 3);
}

inline ImU32 Fade(ImU32 c, float a) {
    const float alpha = ((c >> IM_COL32_A_SHIFT) & 255) * std::clamp(a, 0.f, 1.f);
    return (c & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha) << IM_COL32_A_SHIFT);
}

inline void Draw() {
    if (!used) return;
    // Orbit bold face closest to 14 px; falls back to the legacy font when Orbit fonts failed to load.
    int best = -1;
    for (int i = 0; i < orbit_text_bold.count; ++i)
        if (orbit_text_bold.font[i] && (best < 0 || std::abs(orbit_text_bold.size[i] - 14.f) < std::abs(orbit_text_bold.size[best] - 14.f)))
            best = i;
    ImFont* font = best >= 0 ? orbit_text_bold.font[best] : (poppins ? poppins : ImGui::GetFont());
    const float size = best >= 0 ? orbit_text_bold.size[best] : ImGui::GetFontSize();
    const auto& p = Theme::Current();
    const auto* vp = ImGui::GetMainViewport();
    auto* dl = ImGui::GetBackgroundDrawList();
    const double now = ImGui::GetTime();
    float y = vp->Pos.y + vp->Size.y - 24.f; // bottom-right corner, newest at the bottom
    for (int n = 1; n <= used; ++n) { // newest first, older toasts stack upwards
        const auto& t = ring[(next - n + 3) % 3];
        const double age = now - t.at;
        if (age < 0 || age >= Lifetime) continue;
        const float a = (std::min)(static_cast<float>(age / 0.15), static_cast<float>((Lifetime - age) / 0.4));
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.f, t.text);
        const float w = ts.x + 38.f, h = ts.y + 14.f, x = vp->Pos.x + vp->Size.x - 24.f - w;
        y -= h;
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), Fade(p.card, a), 8.f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), Fade(p.border, a), 8.f);
        dl->AddCircleFilled(ImVec2(x + 15.f, y + h * 0.5f), 4.f, Fade(t.on ? p.accent : p.muted, a));
        dl->AddText(font, size, ImVec2(x + 27.f, y + 7.f), Fade(p.text, a), t.text);
        y -= 6.f;
    }
}
}
