#include "orbit.hpp"
#include "history.hpp"
#include "key_names.hpp"
#include "login.hpp"
#include "module_registry.hpp"
#include "module_view.hpp"
#include "menu.hpp"
#include "palette.hpp"
#include "resources.hpp"
#include "widgets.hpp"
#include "../modules/color_rgb.hpp"
#include "../utils/config/config.hpp"
#include "../utils/theme/theme.hpp"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_internal.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MV = Menu::ModuleView;
namespace P = Menu::Palette;
using namespace Menu::Widgets;

namespace {

constexpr const char* WINDOW_ID = "###client_menu_orbit"; // own ID: independent of the classic menu and v1

// ---------- tokens (docs/clickgui-design-v2.md; px at 100 %) ----------
constexpr float WIN_W = 900.0f, WIN_H = 600.0f, WIN_ROUNDING = 20.0f;
constexpr float LEFT_ZONE_W = 330.0f;
constexpr float MIN_TEXT_PX = 10.0f; // readability floor: user feedback overrides the spec's 7.5-9 px sizes
// derived colors: accent/muted with fixed alpha (no extra Theme::Palette fields)
constexpr float ALPHA_GLOW = 0.55f, ALPHA_POWER_RING = 0.18f, ALPHA_SEGMENT_HALO = 0.22f, ALPHA_DOTS = 0.14f;

float g_s = 1.0f;     // menu scale for this frame
float g_fs = 1.0f;    // font size factor (font_size_percent): scales drawn text only, not layout
float g_alpha = 1.0f; // alpha applied by A()
int g_login_handoff_frame = -100; // frame of the last login morph frame; Orbit::Draw within 3 frames starts fully opened

float Sc(float v) { return v * g_s; }
float Fz(float size) { return (std::max)(MIN_TEXT_PX, Sc(size) * g_fs); } // pixel size of drawn text
ImU32 A(ImU32 c, float extra = 1.0f) { return ColA(c, g_alpha * extra); }

// derived colors
[[maybe_unused]] ImU32 GlowCol() { return A(P::Accent(), ALPHA_GLOW); }
[[maybe_unused]] ImU32 PowerRingCol() { return A(P::Accent(), ALPHA_POWER_RING); }
[[maybe_unused]] ImU32 SegmentHaloCol() { return A(P::Accent(), ALPHA_SEGMENT_HALO); }
[[maybe_unused]] ImU32 DotCol() { return A(P::Muted(), ALPHA_DOTS); }

[[maybe_unused]] ImU32 Mix(ImU32 a, ImU32 b, float t) {
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImLerp(x, y, ImSaturate(t)));
}

// v += (target - v) * saturate(dt * k), state keyed by ImGui ID.
[[maybe_unused]] float Anim(ImGuiID id, float target, float k) {
    static std::unordered_map<ImGuiID, float> values;
    float& v = values[id];
    v += (target - v) * ImSaturate((std::min)(ImGui::GetIO().DeltaTime, 0.1f) * k); // dt clamped: no jump after a hitch
    return v;
}

// ---------- fonts (fall back when an Orbit font failed to load) ----------
ImFont* Fallback() { return poppins ? poppins : ImGui::GetIO().Fonts->Fonts[0]; }
// Nearest loaded size (by ratio) to the final drawn pixel size of a spec size; the atlas is static, so this avoids blurry scaling.
ImFont* Pick(const OrbitFace& face, float size) {
    const float px = Fz(size);
    int best = -1;
    float best_ratio = 1e9f;
    for (int i = 0; i < face.count; ++i) {
        const float r = (std::max)(px, face.size[i]) / (std::min)(px, face.size[i]);
        if (r < best_ratio) { best_ratio = r; best = i; }
    }
    return best >= 0 ? face.font[best] : Fallback();
}
[[maybe_unused]] ImFont* TextFont(float size, bool bold = false) { return Pick(bold ? orbit_text_bold : orbit_text, size); }
[[maybe_unused]] ImFont* MonoFont(float size, bool bold = false) { return Pick(bold ? orbit_mono_bold : orbit_mono, size); }

// ---------- text ----------
[[maybe_unused]] ImVec2 TextSize(ImFont* f, float size, const char* t) { return f->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, t); }
// cy = vertical centre of the line
[[maybe_unused]] void TextC(ImDrawList* dl, ImFont* f, float size, float x, float cy, ImU32 col, const char* t) {
    dl->AddText(f, Fz(size), ImVec2(ImFloor(x), ImFloor(cy - Fz(size) * 0.5f)), col, t); // integer pixel position: no blur
}
// Letter-spaced text (spacing in unscaled px); returns the end x.
[[maybe_unused]] float SpacedText(ImDrawList* dl, ImFont* f, float size, ImVec2 pos, ImU32 col, const char* t, float spacing) {
    while (*t) { // advance per UTF-8 codepoint
        unsigned int cp;
        const int n = (std::max)(1, ImTextCharFromUtf8(&cp, t, nullptr));
        dl->AddText(f, Fz(size), ImVec2(ImFloor(pos.x), ImFloor(pos.y)), col, t, t + n);
        pos.x += f->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, t, t + n).x + Sc(spacing);
        t += n;
    }
    return pos.x;
}

// ---------- dashed shapes ----------
void DashLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float dash, float gap, float thickness = 1.0f) {
    const float len = std::hypot(b.x - a.x, b.y - a.y);
    if (len < 1.0f || dash <= 0.0f) return;
    const ImVec2 d((b.x - a.x) / len, (b.y - a.y) / len);
    for (float t = 0.0f; t < len; t += dash + gap) {
        const float e = (std::min)(t + dash, len);
        dl->AddLine(ImVec2(a.x + d.x * t, a.y + d.y * t), ImVec2(a.x + d.x * e, a.y + d.y * e), col, thickness);
    }
}
// Angles in radians; dash/gap are arc lengths in px.
[[maybe_unused]] void DashArc(ImDrawList* dl, ImVec2 c, float r, float a0, float a1, ImU32 col, float dash, float gap, float thickness = 1.0f) {
    if (r < 1.0f || dash <= 0.0f) return;
    const float step = (dash + gap) / r, span = dash / r;
    for (float a = a0; a < a1; a += step) {
        dl->PathArcTo(c, r, a, (std::min)(a + span, a1), 6);
        dl->PathStroke(col, 0, thickness);
    }
}

// ---------- hexagon (pointy side up) ----------
[[maybe_unused]] void HexPoints(ImVec2 c, float w, float h, ImVec2 out[6]) {
    const float hw = w * 0.5f, hh = h * 0.5f;
    out[0] = ImVec2(c.x, c.y - hh);
    out[1] = ImVec2(c.x + hw, c.y - hh * 0.5f);
    out[2] = ImVec2(c.x + hw, c.y + hh * 0.5f);
    out[3] = ImVec2(c.x, c.y + hh);
    out[4] = ImVec2(c.x - hw, c.y + hh * 0.5f);
    out[5] = ImVec2(c.x - hw, c.y - hh * 0.5f);
}
[[maybe_unused]] void HexFill(ImDrawList* dl, ImVec2 c, float w, float h, ImU32 col) {
    ImVec2 p[6];
    HexPoints(c, w, h, p);
    dl->AddConvexPolyFilled(p, 6, col);
}
[[maybe_unused]] void HexOutline(ImDrawList* dl, ImVec2 c, float w, float h, ImU32 col, float thickness) {
    ImVec2 p[6];
    HexPoints(c, w, h, p);
    dl->AddPolyline(p, 6, col, ImDrawFlags_Closed, thickness);
}

// ---------- small glow: 4 grown layers x MenuGlow (DrawGlowCircle is too heavy for small elements) ----------
[[maybe_unused]] void GlowHex(ImDrawList* dl, ImVec2 c, float w, float h, float reach, ImU32 accent, float alpha) {
    const float glow = Theme::MenuGlow();
    if (glow <= 0.0f) return;
    constexpr int layers = 4;
    for (int i = layers; i >= 1; --i) {
        const float grow = Sc(reach) * i / layers;
        // h grows by 2g/cos30 so the outline offset is uniform on the slanted sides
        HexFill(dl, c, w + grow * 2.0f, h + grow * 2.0f / 0.8660254f, ColA(accent, g_alpha * alpha * ALPHA_GLOW * glow * 0.35f / i));
    }
}
[[maybe_unused]] void GlowDot(ImDrawList* dl, ImVec2 c, float r, float reach, ImU32 accent, float alpha) {
    const float glow = Theme::MenuGlow();
    if (glow <= 0.0f) return;
    constexpr int layers = 4;
    for (int i = layers; i >= 1; --i)
        dl->AddCircleFilled(c, r + Sc(reach) * i / layers, ColA(accent, g_alpha * alpha * ALPHA_GLOW * glow * 0.35f / i), 24);
}

// Icon (Font Awesome atlas) centred on (cx, cy).
void IconC(ImDrawList* dl, float size, float cx, float cy, ImU32 col, const char* icon) {
    const ImVec2 sz = icons->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, icon);
    dl->AddText(icons, Fz(size), ImVec2(cx - sz.x * 0.5f, cy - Fz(size) * 0.5f), col, icon);
}
float SpacedWidth(ImFont* f, float size, const char* t, float spacing) {
    float x = 0.0f;
    while (*t) {
        unsigned int cp;
        const int n = (std::max)(1, ImTextCharFromUtf8(&cp, t, nullptr));
        x += f->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, t, t + n).x + Sc(spacing);
        t += n;
    }
    return x - Sc(spacing);
}
// Text shortened to max_w px with "..." (per codepoint); sets trunc when it was cut.
std::string FitText(ImFont* f, float size, float max_w, const std::string& text, bool& trunc) {
    const float px = Fz(size);
    if (f->CalcTextSizeA(px, FLT_MAX, 0.0f, text.c_str()).x <= max_w) return text;
    trunc = true;
    const float dots = f->CalcTextSizeA(px, FLT_MAX, 0.0f, "...").x;
    const char* b = text.c_str();
    const char* e = b + text.size();
    const char* p = b;
    float x = 0.0f;
    while (p < e) {
        unsigned int cp;
        const int n = (std::max)(1, ImTextCharFromUtf8(&cp, p, e));
        const float cw = f->CalcTextSizeA(px, FLT_MAX, 0.0f, p, p + n).x;
        if (x + cw + dots > max_w) break;
        x += cw;
        p += n;
    }
    std::string out(b, p);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "...";
}
std::string Upper(const char* t) {
    std::string s(t);
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

constexpr float PI = 3.14159265f, DEG = PI / 180.0f;
// ring geometry (spec): centre (165,238), r 96, width 14, 4 x 82 deg with 8 deg gaps, start -86 deg clockwise
constexpr float RING_CX = 165.0f, RING_CY = 238.0f, RING_R = 96.0f, RING_W = 14.0f, RING_START = -86.0f, RING_SPAN = 82.0f;
constexpr float DOCK_Y = 444.0f, DOCK_D = 44.0f, DOCK_GAP = 10.0f, DOCK_X = 35.0f;
constexpr float RIGHT_X = 331.0f, CONTENT_X = 352.0f, CONTENT_TOP = 110.0f, CONTENT_MARGIN = 24.0f;

// ---------- settings widgets (inspector body) ----------

// small rounded-rect glow: 3 grown layers x MenuGlow
void GlowRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, float reach, ImU32 accent, float alpha) {
    const float glow = Theme::MenuGlow();
    if (glow <= 0.0f || alpha <= 0.0f) return;
    for (int i = 3; i >= 1; --i) {
        const float g = Sc(reach) * i / 3.0f;
        dl->AddRectFilled(ImVec2(a.x - g, a.y - g), ImVec2(b.x + g, b.y + g), ColA(accent, g_alpha * alpha * ALPHA_GLOW * glow * 0.35f / i), rounding + g);
    }
}

// Dot grid (2x2 squares every 18 px) over [x0,x1) x [y0,y1) in unscaled window coordinates.
void DotGrid(ImDrawList* dl, ImVec2 wp, float x0, float x1, float y0, float y1) {
    const float d = Sc(2);
    for (float gy = y0; gy < y1; gy += 18.0f)
        for (float gx = x0; gx < x1; gx += 18.0f) {
            const ImVec2 p(ImFloor(wp.x + Sc(gx)), ImFloor(wp.y + Sc(gy)));
            dl->AddRectFilled(p, ImVec2(p.x + d, p.y + d), DotCol());
        }
}

// Label with ellipsis; tooltip = SettingHelp (plus the full name when clipped) while hovered.
void SettingLabel(ImDrawList* dl, const ModuleSetting& s, float x, float cy, float max_w, float hit_h, ImU32 col) {
    bool trunc = false;
    ImFont* f = TextFont(13);
    const std::string t = FitText(f, 13, max_w, s.DisplayName(), trunc);
    TextC(dl, f, 13, x, cy, col, t.c_str());
    if (ImGui::IsMouseHoveringRect(ImVec2(x, cy - hit_h * 0.5f), ImVec2(x + max_w, cy + hit_h * 0.5f)) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
        const char* help = MV::SettingHelp(s);
        if (help && trunc) ImGui::SetTooltip("%s\n%s", s.DisplayName(), help);
        else if (help) ImGui::SetTooltip("%s", help);
        else if (trunc) ImGui::SetTooltip("%s", s.DisplayName());
    }
}

// Diamond toggle 40x20: clicking the label toggles too.
void CheckRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.b_val) return;
    const float h = Sc(30), tw = Sc(40), th = Sc(20);
    ImGui::PushID(&s);
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("##row", ImVec2(w, h))) s.b_val->store(!s.b_val->load());
    const bool hov = ImGui::IsItemHovered();
    const float hv = Anim(ImGui::GetItemID(), hov ? 1.0f : 0.0f, 25.0f);
    const float ov = Anim(ImGui::GetID("##on"), s.b_val->load() ? 1.0f : 0.0f, 18.75f);
    ImGui::PopID();
    const ImVec2 tmin(p.x + w - tw, p.y + (h - th) * 0.5f), tmax(tmin.x + tw, tmin.y + th);
    dl->AddRectFilled(tmin, tmax, A(Mix(P::Toggle(), ColA(P::Accent(), 0.18f), ov)), Sc(5));
    dl->AddRect(tmin, tmax, A(Mix(Mix(P::Toggle(), P::Label(), hv), P::Accent(), ov)), Sc(5));
    const ImVec2 k(tmin.x + Sc(5.0f + 20.0f * ov + 4.5f), tmin.y + th * 0.5f);
    const float d = Sc(4.5f) * 1.41421356f; // 9x9 square rotated by 45 degrees
    if (ov > 0.01f) GlowDot(dl, k, Sc(3), 5, P::Accent(), ov);
    dl->AddQuadFilled(ImVec2(k.x, k.y - d), ImVec2(k.x + d, k.y), ImVec2(k.x, k.y + d), ImVec2(k.x - d, k.y),
                      A(Mix(Mix(P::Label(), P::Text(), hv), P::Accent(), ov)));
    ImFont* mf = MonoFont(7.5f, true);
    TextC(dl, mf, 7.5f, tmin.x + Sc(5), tmin.y + th * 0.5f, A(P::Accent(), ov), "ON");
    // 10 px floor: at small menu sizes "OFF" would leave the toggle, so it moves left (never into the knob at its left position)
    const float off_x = (std::max)(tmin.x + Sc(15.9f) + 1.0f, (std::min)(tmin.x + Sc(18), tmax.x - 1.5f - TextSize(mf, 7.5f, "OFF").x));
    TextC(dl, mf, 7.5f, off_x, tmin.y + th * 0.5f, A(P::Muted(), 1.0f - ov), "OFF");
    SettingLabel(dl, s, p.x, p.y + h * 0.5f, w - tw - Sc(10), Sc(20), A(Mix(P::Label(), P::Text(), hv)));
}

// 20-cell segment slider. Operated through an invisible SliderInt (AlwaysClamp, Ctrl+click text entry), pattern of clickgui.cpp.
void SliderRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.i_val) return;
    ImGui::PushID(&s);
    int v = s.i_val->load();
    const int before = v;
    const int lo = s.i_min, hi = s.i_max;
    // header: label left, value + unit right
    ImFont* vf = MonoFont(13, true);
    ImFont* uf = MonoFont(10);
    char val[16];
    std::snprintf(val, sizeof(val), "%d", v);
    const char* unit = s.suffix ? s.suffix : "";
    while (*unit == ' ') ++unit;
    const float uw = *unit ? TextSize(uf, 10, unit).x + Sc(3) : 0.0f, vw = TextSize(vf, 13, val).x;
    const float ly = p.y + Sc(8);
    TextC(dl, vf, 13, p.x + w - uw - vw, ly, A(P::Accent()), val);
    if (*unit) TextC(dl, uf, 10, p.x + w - uw + Sc(3), ly + Sc(1), A(P::Muted()), unit);
    SettingLabel(dl, s, p.x, ly, w - vw - uw - Sc(14), Sc(16), A(P::Label()));

    const float ctop = p.y + Sc(21), chh = Sc(10), cmid = ctop + chh * 0.5f;
    const ImGuiID sid = ImGui::GetID("##slider");
    const bool typing = ImGui::TempInputIsActive(sid);
    const float pad_y = (std::max)(0.0f, (Sc(18) - ImGui::GetFontSize()) * 0.5f);
    const float fh = ImGui::GetFontSize() + pad_y * 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(p.x, cmid - fh * 0.5f));
    ImGui::SetNextItemWidth(w);
    const ImVec4 clear(0, 0, 0, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, typing ? ColV(P::Background(), 1.0f) : clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, typing ? ColV(P::Background(), 1.0f) : clear);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, typing ? ColV(P::Background(), 1.0f) : clear);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, clear);
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, clear);
    ImGui::PushStyleColor(ImGuiCol_Text, typing ? ColV(P::Text(), 1.0f) : clear);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(typing ? 6.0f : 0.0f, pad_y));
    ImGui::SliderInt("##slider", &v, lo, hi, "", ImGuiSliderFlags_AlwaysClamp); // Ctrl+click entry must stay in range
    const bool hov = ImGui::IsItemHovered(), act = ImGui::IsItemActive();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(6);
    if (v != before) s.i_val->store(v);
    const float hv = Anim(sid, (hov || act) ? 1.0f : 0.0f, 25.0f);
    const float dv = Anim(ImGui::GetID("##drag"), act ? 1.0f : 0.0f, 33.0f);
    const float t = hi > lo ? ImSaturate(static_cast<float>(v - lo) / static_cast<float>(hi - lo)) : 0.0f;
    int n = static_cast<int>(std::floor(t * 20.0f + 0.5f));
    if (v > lo && n == 0) n = 1; // a value above the minimum always shows at least the head cell
    const float shown = Anim(ImGui::GetID("##cells"), static_cast<float>(n), 33.0f); // cells fill/empty smoothly (90 ms)
    ImGui::PopID();

    const float gap = Sc(3), cw = (w - gap * 19.0f) / 20.0f;
    for (int k = 0; k < 20; ++k) {
        const float x = p.x + k * (cw + gap);
        const float fill = ImSaturate(shown - k);                       // 0 = empty cell, 1 = filled
        const float head = ImSaturate(1.0f - std::fabs(shown - (k + 1))); // weight of the head highlight
        const float grow = Sc(2) * dv * head;
        const ImVec2 a(x, ctop - grow), b(x + cw, ctop + chh + grow);
        if (head > 0.01f && hv > 0.01f) GlowRect(dl, a, b, Sc(2), 8.0f + 4.0f * dv, P::Accent(), hv * head);
        const ImU32 filled = ColA(P::Accent(), 0.35f + 0.65f * (k + 1) / (std::max)(1.0f, shown));
        dl->AddRectFilled(a, b, A(Mix(Mix(P::Toggle(), filled, fill), Theme::Current().hover, head)), Sc(2));
    }
    ImFont* mf = MonoFont(9);
    char lo_txt[16], hi_txt[16];
    std::snprintf(lo_txt, sizeof(lo_txt), "%d", lo);
    std::snprintf(hi_txt, sizeof(hi_txt), "%d", hi);
    TextC(dl, mf, 9, p.x, ctop + chh + Sc(11), A(P::Label(), 0.8f), lo_txt);
    TextC(dl, mf, 9, p.x + w - TextSize(mf, 9, hi_txt).x, ctop + chh + Sc(11), A(P::Label(), 0.8f), hi_txt);
}

// ----- colour picker popup: hue ring (16 px) + SV square (90 px) drawn with ImDrawList, values stored as 0xRRGGBB like DrawCustomColorPopup -----
struct Picker {
    float h = 0, s = 1, v = 1, fade = 0;
    int rgb = -1, drag = 0; // drag: 0 none, 1 ring, 2 square
    char hex[9] = {};
    void Sync(int next, bool snap = false) {
        if (rgb == next && !snap) return;
        const auto c = ColorRGB::Unpack(next);
        float hue, sat, val;
        ImGui::ColorConvertRGBtoHSV(c[0], c[1], c[2], hue, sat, val);
        if (sat > 0.0001f && val > 0.0001f) h = hue; // hue stays editable for gray/black
        if (val > 0.0001f) s = sat;
        v = val;
        rgb = next;
        std::snprintf(hex, sizeof(hex), "%06X", static_cast<unsigned>(rgb));
    }
    void Apply(std::atomic<int>* out) {
        std::array<float, 3> c{};
        ImGui::ColorConvertHSVtoRGB(h, s, v, c[0], c[1], c[2]);
        rgb = ColorRGB::Pack(c);
        out->store(rgb);
        std::snprintf(hex, sizeof(hex), "%06X", static_cast<unsigned>(rgb));
    }
};
ImU32 Rgb(int c, float alpha = 1.0f) { return ColA(IM_COL32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255), alpha); }

void ColorPopup(const char* label, std::atomic<int>* value, ImVec2 amin, ImVec2 amax, bool opened) {
    static std::unordered_map<ImGuiID, Picker> states;
    Picker& st = states[ImGui::GetID("##orbit_color")];
    if (opened) {
        st.Sync(value->load(), true);
        st.fade = 0.0f;
        st.drag = 0;
        ImGui::OpenPopup("##orbit_color");
    }
    if (!ImGui::IsPopupOpen("##orbit_color")) return;
    st.Sync(value->load());
    st.fade += (1.0f - st.fade) * ImSaturate(ImGui::GetIO().DeltaTime * 24.0f);

    const float W = Sc(192), H = Sc(284), pad = Sc(16);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float x = ImClamp(amax.x - W, vp->WorkPos.x + 8.0f, (std::max)(vp->WorkPos.x + 8.0f, vp->WorkPos.x + vp->WorkSize.x - W - 8.0f));
    float y = amax.y + Sc(6);
    if (y + H > vp->WorkPos.y + vp->WorkSize.y - 8.0f) y = amin.y - H - Sc(6);
    y = (std::max)(vp->WorkPos.y + 8.0f, y);
    ImGui::SetNextWindowPos(ImVec2(x, y + 5.0f * (1.0f - st.fade)));
    ImGui::SetNextWindowSize(ImVec2(W, H));
    const float saved_alpha = g_alpha;
    g_alpha = saved_alpha * st.fade; // our own draw colours; the ImGui style alpha below covers background/border/input
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, Sc(16));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * st.fade);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, P::Card());
    ImGui::PushStyleColor(ImGuiCol_Border, P::Border());
    if (ImGui::BeginPopup("##orbit_color", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetWindowPos();
        bool tr = false;
        ImFont* tf = MonoFont(10);
        const std::string title = FitText(tf, 10, W - pad * 2.0f, Upper(label), tr);
        TextC(dl, tf, 10, o.x + pad, o.y + Sc(23), A(P::Muted()), title.c_str());

        const ImVec2 C(o.x + pad + Sc(80), o.y + Sc(38 + 80));
        const float r_out = Sc(80), r_in = Sc(64), sq = Sc(45);
        ImGui::SetCursorScreenPos(ImVec2(C.x - r_out, C.y - r_out));
        ImGui::InvisibleButton("##picker", ImVec2(r_out * 2, r_out * 2));
        const ImVec2 m = ImGui::GetIO().MousePos;
        if (ImGui::IsItemActivated()) {
            const float dist = std::hypot(m.x - C.x, m.y - C.y);
            if (std::fabs(m.x - C.x) <= sq + Sc(4) && std::fabs(m.y - C.y) <= sq + Sc(4)) st.drag = 2;
            else if (dist >= r_in - Sc(4) && dist <= r_out + Sc(4)) st.drag = 1;
            else st.drag = 0;
        }
        if (!ImGui::IsItemActive()) st.drag = 0;
        if (st.drag == 1) {
            const float a = std::atan2(m.x - C.x, -(m.y - C.y)) / (2.0f * PI); // 0 at the top, clockwise
            st.h = (std::min)(0.999999f, a < 0.0f ? a + 1.0f : a);
            st.Apply(value);
        } else if (st.drag == 2) {
            st.s = ImSaturate((m.x - (C.x - sq)) / (sq * 2.0f));
            st.v = 1.0f - ImSaturate((m.y - (C.y - sq)) / (sq * 2.0f));
            st.Apply(value);
        }

        // hue ring: coloured quads with per-vertex colours
        constexpr int N = 96;
        const ImVec2 uv = dl->_Data->TexUvWhitePixel;
        dl->PrimReserve(6 * N, 4 * N);
        for (int i = 0; i < N; ++i) {
            float rr, gg, bb;
            ImVec2 pt[4];
            ImU32 col[2];
            for (int e = 0; e < 2; ++e) {
                const float f = static_cast<float>(i + e) / N, th = f * 2.0f * PI;
                ImGui::ColorConvertHSVtoRGB(f, 1.0f, 1.0f, rr, gg, bb);
                col[e] = ColA(IM_COL32(static_cast<int>(rr * 255), static_cast<int>(gg * 255), static_cast<int>(bb * 255), 255), g_alpha);
                const ImVec2 dir(std::sin(th), -std::cos(th));
                pt[e == 0 ? 0 : 3] = ImVec2(C.x + dir.x * r_in, C.y + dir.y * r_in);
                pt[e == 0 ? 1 : 2] = ImVec2(C.x + dir.x * r_out, C.y + dir.y * r_out);
            }
            const ImDrawIdx base = static_cast<ImDrawIdx>(dl->_VtxCurrentIdx);
            dl->PrimWriteVtx(pt[0], uv, col[0]);
            dl->PrimWriteVtx(pt[1], uv, col[0]);
            dl->PrimWriteVtx(pt[2], uv, col[1]);
            dl->PrimWriteVtx(pt[3], uv, col[1]);
            dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
            dl->PrimWriteIdx(base); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2)); dl->PrimWriteIdx(static_cast<ImDrawIdx>(base + 3));
        }
        // raw quads have no AA fringe: soften both ring edges with thin card-coloured circles
        dl->AddCircle(C, r_in, A(P::Card()), N, 1.0f);
        dl->AddCircle(C, r_out, A(P::Card()), N, 1.0f);
        {   // ring handle
            const float th = st.h * 2.0f * PI, rm = (r_in + r_out) * 0.5f;
            const ImVec2 hp(C.x + std::sin(th) * rm, C.y - std::cos(th) * rm);
            float rr, gg, bb;
            ImGui::ColorConvertHSVtoRGB(st.h, 1.0f, 1.0f, rr, gg, bb);
            dl->AddCircleFilled(hp, Sc(6), ColA(IM_COL32(static_cast<int>(rr * 255), static_cast<int>(gg * 255), static_cast<int>(bb * 255), 255), g_alpha), 20);
            dl->AddCircle(hp, Sc(6), A(IM_COL32_WHITE), 20, Sc(2));
        }

        // SV square: horizontal white -> hue, vertical transparent -> black (exact colours, no texture)
        const ImVec2 s0(C.x - sq, C.y - sq), s1(C.x + sq, C.y + sq);
        int begin = dl->VtxBuffer.Size;
        dl->AddRectFilled(s0, s1, A(IM_COL32_WHITE), Sc(6));
        const ImU32 hue = ImColor::HSV(st.h, 1.0f, 1.0f);
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, begin, dl->VtxBuffer.Size, s0, ImVec2(s1.x, s0.y), IM_COL32_WHITE, hue);
        begin = dl->VtxBuffer.Size;
        dl->AddRectFilled(s0, s1, IM_COL32(0, 0, 0, 255), Sc(6));
        for (int i = begin; i < dl->VtxBuffer.Size; ++i) {
            const float shade = ImSaturate((dl->VtxBuffer[i].pos.y - s0.y) / (s1.y - s0.y));
            dl->VtxBuffer[i].col = IM_COL32(0, 0, 0, static_cast<int>(255.0f * shade * g_alpha));
        }
        const ImVec2 hp(s0.x + st.s * sq * 2.0f, s0.y + (1.0f - st.v) * sq * 2.0f);
        dl->AddCircleFilled(hp, Sc(5), A(IM_COL32(15, 15, 24, 140)), 20);
        dl->AddCircle(hp, Sc(5), A(IM_COL32_WHITE), 20, Sc(2));

        // quick colours (design: hexagon swatches 16x18, gap 6)
        static const int quick[6] = {0xF459D5, 0x9587FF, 0x4BDBB7, 0xF4597B, 0xF0B852, 0xFFFFFF};
        for (int i = 0; i < 6; ++i) {
            ImGui::PushID(i);
            const ImVec2 at(o.x + pad + Sc(i * 22.0f), o.y + Sc(210));
            ImGui::SetCursorScreenPos(at);
            if (ImGui::InvisibleButton("##quick", ImVec2(Sc(16), Sc(18)))) { value->store(quick[i]); st.Sync(quick[i], true); }
            const bool qh = ImGui::IsItemHovered();
            ImGui::PopID();
            const ImVec2 qc(at.x + Sc(8), at.y + Sc(9));
            HexFill(dl, qc, Sc(16), Sc(18), Rgb(quick[i], g_alpha));
            if (qh || value->load() == quick[i]) HexOutline(dl, qc, Sc(20), Sc(22), A(P::Accent()), Sc(1.5f));
        }

        // hex field
        const ImVec2 hmin(o.x + pad, o.y + Sc(240)), hmax(hmin.x + Sc(160), hmin.y + Sc(28));
        dl->AddRectFilled(hmin, hmax, A(P::Background()), Sc(6));
        HexFill(dl, ImVec2(hmin.x + Sc(15), hmin.y + Sc(14)), Sc(14), Sc(16), Rgb(value->load(), g_alpha));
        ImFont* mf = MonoFont(12);
        TextC(dl, mf, 12, hmin.x + Sc(29), hmin.y + Sc(14), A(P::Muted()), "#");
        ImGui::SetWindowFontScale(Fz(12) / mf->FontSize);
        ImGui::PushFont(mf);
        const float fpad = (std::max)(0.0f, (Sc(28) - ImGui::GetFontSize()) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(hmin.x + Sc(29) + TextSize(mf, 12, "#").x + Sc(2), hmin.y));
        ImGui::SetNextItemWidth(hmax.x - Sc(8) - ImGui::GetCursorScreenPos().x);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, fpad));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(P::Text(), 1.0f));
        const bool edited = ImGui::InputText("##hex", st.hex, sizeof(st.hex), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsHexadecimal);
        const bool hex_active = ImGui::IsItemActive();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();
        ImGui::SetWindowFontScale(1.0f);
        if (edited) {
            int rgb;
            if (ColorRGB::ParseHex(st.hex, rgb)) { value->store(rgb); st.Sync(rgb); }
        }
        if (!hex_active) std::snprintf(st.hex, sizeof(st.hex), "%06X", static_cast<unsigned>(value->load()));
        dl->AddRect(hmin, hmax, A(hex_active ? P::Accent() : P::Border()), Sc(6));
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    g_alpha = saved_alpha;
}

// Colour row: label + 116x28 box with hexagon swatch and hex value; click opens the picker.
void ColorRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.i_val) return;
    const float h = Sc(30);
    ImGui::PushID(&s);
    const ImVec2 bmin(p.x + w - Sc(116), p.y + Sc(1)), bmax(bmin.x + Sc(116), bmin.y + Sc(28));
    ImGui::SetCursorScreenPos(bmin);
    const bool opened = ImGui::InvisibleButton("##swatch", ImVec2(Sc(116), Sc(28)));
    const float hv = Anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 25.0f);
    const int rgb = s.i_val->load();
    dl->AddRectFilled(bmin, bmax, A(P::Background()), Sc(6));
    dl->AddRect(bmin, bmax, A(Mix(P::Border(), ColA(P::Accent(), 0.6f), hv)), Sc(6));
    HexFill(dl, ImVec2(bmin.x + Sc(17), bmin.y + Sc(14)), Sc(14), Sc(16), Rgb(rgb, g_alpha));
    char hex[12];
    std::snprintf(hex, sizeof(hex), "#%06X", rgb & 0xFFFFFF);
    TextC(dl, MonoFont(12), 12, bmin.x + Sc(32), bmin.y + Sc(14), A(P::Text()), hex);
    SettingLabel(dl, s, p.x, p.y + h * 0.5f, w - Sc(116) - Sc(10), Sc(20), A(Mix(P::Label(), P::Text(), hv)));
    ColorPopup(s.DisplayName(), s.i_val, bmin, bmax, opened);
    ImGui::PopID();
}

} // namespace

namespace Orbit {

bool TakesLoginHandoff() { return ImGui::GetFrameCount() - g_login_handoff_frame <= 3; }

void Draw() {
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;
    const ImVec2 display = io.DisplaySize;
    const float fit = (std::min)((display.x - 32.0f) / WIN_W, (display.y - 32.0f) / WIN_H);
    g_s = (std::max)(0.1f, (std::min)(Config::menu_size_percent.load() / 100.0f, fit));
    g_fs = Config::font_size_percent.load() / 100.0f;
    const float w = WIN_W * g_s, h = WIN_H * g_s;

    static float open_t = 0.0f;
    static int last_frame = -10;
    // The caller skips Draw while the menu is closed: a frame gap restarts the fade-in.
    // animation state of the grid / connection line (restarted whenever the menu is reopened)
    static double grid_t0 = 0.0;                  // tiles fade in staggered from this time
    static int line_mod = -2;                     // module the connection line currently belongs to
    static float line_g = 0.0f;                   // line growth 0..1
    static double comet_t0 = 0.0;                 // start of the travelling light
    constexpr float COMET_TRAVEL = 0.15f;        // seconds from tile tip to line end (line growth uses the same)
    static bool comet_repeat = false;             // repeats are subtler than the first run
    static int press_mod = -1;                    // tile with a click feedback flash
    static double press_t0 = -10.0;
    const double now = ImGui::GetTime();
    const float dtc = (std::min)(dt, 0.1f);       // frame-rate independent, clamped after hitches
    if (ImGui::GetFrameCount() != last_frame + 1) { open_t = 0.0f; grid_t0 = now; line_mod = -2; press_mod = -1; }
    // login morph already showed the open window: start fully opened (tiles still stagger in), selected segment without a fade
    const bool handoff = ImGui::GetFrameCount() - g_login_handoff_frame <= 3;
    g_login_handoff_frame = -100;
    if (handoff) open_t = 1.0f;
    last_frame = ImGui::GetFrameCount();
    open_t += (1.0f - open_t) * ImSaturate(dtc * 9.4f);

    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImVec2 menu_pos((display.x - w) * 0.5f, (display.y - h) * 0.5f);
    if (const ImGuiWindow* previous = ImGui::FindWindowByName(WINDOW_ID))
        menu_pos = ImVec2(ImClamp(previous->Pos.x, 0.0f, (std::max)(0.0f, display.x - w)),
                          ImClamp(previous->Pos.y, 0.0f, (std::max)(0.0f, display.y - h)));
    ImGui::SetNextWindowPos(menu_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, open_t);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Sc(WIN_ROUNDING));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, P::Background());
    ImGui::Begin(WINDOW_ID, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetWindowFontScale(g_fs);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    g_alpha = open_t;

    // soft window shadow (background list, so it is not clipped to the window)
    for (int layer = 1; layer <= 3; ++layer) {
        const float e = Sc(10.0f * layer);
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(wp.x - e, wp.y + Sc(16) - e), ImVec2(wp.x + w + e, wp.y + h + Sc(16) + e),
                                                      ColA(IM_COL32(0, 0, 0, 255), 0.14f * open_t), Sc(WIN_ROUNDING) + e);
    }
    if (Theme::gradient_enabled.load()) {
        const int first = dl->VtxBuffer.Size;
        const ImU32 top = A(Theme::Current().background), bottom = A(Theme::GradientEnd());
        dl->AddRectFilled(wp, ImVec2(wp.x + w, wp.y + h), top, Sc(WIN_ROUNDING));
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, first, dl->VtxBuffer.Size, wp, ImVec2(wp.x, wp.y + h), top, bottom);
    }

    // zone frame: brand mark and dashed divider (navigation, grid and inspector follow in later steps)
    {
        const float bx = wp.x + Sc(24), by = wp.y + Sc(22);
        ImFont* mono = MonoFont(12);
        const float x1 = bx + TextSize(mono, 12, "phantom").x, x2 = x1 + TextSize(mono, 12, "/").x;
        dl->AddText(mono, Fz(12), ImVec2(bx, by), A(P::Text()), "phantom");
        dl->AddText(mono, Fz(12), ImVec2(x1, by), A(P::Accent()), "/");
        dl->AddText(mono, Fz(12), ImVec2(x2, by), A(P::Text()), "ui");
        const float dx = wp.x + Sc(LEFT_ZONE_W);
        DashLine(dl, ImVec2(dx, wp.y + Sc(24)), ImVec2(dx, wp.y + h - Sc(24)), A(P::Border()), Sc(4), Sc(4));
    }

    // ================= navigation =================
    auto modules = Menu::Registry::GetModules();
    static int sel = MV::PAGE_COMBAT; // selected page: 0-3 ring categories, 4-8 dock pages (module_view.hpp ids)

    int cat_online[4] = {};
    MV::CountEnabledPerCategory(modules, cat_online);
    int cat_total[4] = {}, cat_fav[4] = {}, online_all = 0;
    for (const auto& m : modules) {
        if (m.category >= 0 && m.category < 4) { ++cat_total[m.category]; if (m.favorite) ++cat_fav[m.category]; }
        if (*m.enabled) ++online_all;
    }
    const ImVec2 rc(wp.x + Sc(RING_CX), wp.y + Sc(RING_CY));

    // dot grid of the right zone (2x2 squares every 18 px)
    DotGrid(dl, wp, RIGHT_X + 12.0f, WIN_W - 12.0f, 12.0f, WIN_H - 12.0f);

    DrawGlowCircle(dl, rc, Sc(90), P::Accent(), g_alpha * 0.4f, 12); // zone halo, ~12 % peak

    // --- category ring ---
    int ring_hit = -1;
    auto label_center = [&](int i) {
        const float mid = (RING_START + 90.0f * i + RING_SPAN * 0.5f) * DEG;
        return ImVec2(rc.x + std::cos(mid) * Sc(150), rc.y + std::sin(mid) * Sc(150));
    };
    {
        // Hit test first; the button is only submitted over a segment or label box, so gaps, centre and corners still drag the window.
        const ImVec2 m = io.MousePos;
        ImVec2 bmin, bsize;
        // AllowWhenBlockedByActiveItem: while the button itself is held, the window no longer counts as hovered
        if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
            const float dist = std::hypot(m.x - rc.x, m.y - rc.y);
            if (dist >= Sc(RING_R - RING_W * 0.5f - 3.0f) && dist <= Sc(RING_R + RING_W * 0.5f + 3.0f)) {
                const float rel = std::fmod(std::atan2(m.y - rc.y, m.x - rc.x) / DEG - RING_START + 720.0f, 360.0f);
                const int seg = static_cast<int>(rel / 90.0f);
                if (rel - seg * 90.0f <= RING_SPAN) {
                    ring_hit = seg;
                    bmin = ImVec2(rc.x - Sc(108), rc.y - Sc(108));
                    bsize = ImVec2(Sc(216), Sc(216));
                }
            }
            for (int i = 0; i < 4 && ring_hit < 0; ++i) {
                const ImVec2 lc = label_center(i);
                if (std::fabs(m.x - lc.x) <= Sc(46) && std::fabs(m.y - lc.y) <= Sc(18)) {
                    ring_hit = i;
                    bmin = ImVec2(lc.x - Sc(46), lc.y - Sc(18));
                    bsize = ImVec2(Sc(92), Sc(36));
                }
            }
        }
        static ImVec2 last_min, last_size; // rect of the last submitted button, kept while the button is held
        const ImGuiID ring_id = ImGui::GetID("##orbit_ring");
        const bool held = ImGui::GetActiveID() == ring_id;
        if (ring_hit >= 0) {
            last_min = bmin;
            last_size = bsize;
            // The first click must not be lost to the search field: drop its focus before the button is tested.
            if (!held && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::GetActiveID() != 0) ImGui::ClearActiveID();
        }
        if (ring_hit >= 0 || held) {
            ImGui::SetCursorScreenPos(ring_hit >= 0 ? bmin : last_min);
            // sel changes only when the release is still over a valid segment/label (ring_hit >= 0)
            if (ImGui::InvisibleButton("##orbit_ring", ring_hit >= 0 ? bsize : last_size) && ring_hit >= 0) sel = ring_hit;
        }
    }
    ImGui::PushID("orbit_ring");
    for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        const float hv = Anim(ImGui::GetID("hover"), ring_hit == i ? 1.0f : 0.0f, 25.0f);
        const float av = Anim(ImGui::GetID("active"), sel == i ? 1.0f : 0.0f, handoff ? 1000.0f : 13.6f);
        ImGui::PopID();
        // opening: the ring turns from -20 deg to 0 (labels and hit areas stay fixed)
        const float a0 = (RING_START + 90.0f * i - 20.0f * (1.0f - open_t)) * DEG, a1 = a0 + RING_SPAN * DEG;
        if (av > 0.01f) {
            dl->PathArcTo(rc, Sc(RING_R), a0, a1, 24);
            dl->PathStroke(A(P::Accent(), ALPHA_SEGMENT_HALO * av * (std::min)(1.0f, Theme::MenuGlow())), 0, Sc(24));
        }
        dl->PathArcTo(rc, Sc(RING_R), a0, a1, 24);
        dl->PathStroke(A(P::Toggle()), 0, Sc(RING_W));
        if (hv * (1.0f - av) > 0.01f) {
            dl->PathArcTo(rc, Sc(RING_R), a0, a1, 24);
            dl->PathStroke(A(P::Label(), 0.55f * hv * (1.0f - av)), 0, Sc(RING_W));
        }
        if (av > 0.01f) {
            dl->PathArcTo(rc, Sc(RING_R), a0, a1, 24);
            dl->PathStroke(A(P::Accent(), av), 0, Sc(RING_W));
        }

        // label at the segment's mid angle: icon + name, "0N ON" below
        const ImVec2 lc = label_center(i);
        const MV::CategoryTab& tab = MV::categories[i];
        ImFont* nf = TextFont(13, true);
        const float nw = TextSize(nf, 13, tab.label).x, gap = Sc(6), iw = Fz(14);
        const float x0 = lc.x - (iw + gap + nw) * 0.5f, cy1 = lc.y - Sc(8);
        IconC(dl, 14, x0 + iw * 0.5f, cy1, A(Mix(Mix(P::Muted(), P::Label(), hv), P::Accent(), av)), tab.icon);
        TextC(dl, nf, 13, x0 + iw + gap, cy1, A(Mix(Mix(P::Label(), P::Text(), hv), P::Text(), av)), tab.label);
        char cnt[16];
        std::snprintf(cnt, sizeof(cnt), "%02d ON", cat_online[i]);
        ImFont* mf = MonoFont(9);
        TextC(dl, mf, 9, lc.x - TextSize(mf, 9, cnt).x * 0.5f, lc.y + Sc(10), A(cat_online[i] > 0 ? P::Accent() : P::Label(), cat_online[i] > 0 ? 1.0f : 0.8f), cnt);
    }
    ImGui::PopID();

    // tick ring: one tick per registry module, lit when enabled
    ImGui::PushID("orbit_ticks");
    for (int k = 0; k < static_cast<int>(modules.size()); ++k) {
        ImGui::PushID(k);
        const float lit = Anim(ImGui::GetID("t"), *modules[k].enabled ? 1.0f : 0.0f, 12.0f);
        ImGui::PopID();
        const float ang = (-90.0f + 360.0f * k / static_cast<float>(modules.size())) * DEG;
        const float ca = std::cos(ang), sa = std::sin(ang);
        dl->AddLine(ImVec2(rc.x + ca * Sc(70), rc.y + sa * Sc(70)), ImVec2(rc.x + ca * Sc(77), rc.y + sa * Sc(77)),
                    A(Mix(ColA(P::Muted(), 0.30f), P::Accent(), lit)), Sc(2.5f));
    }
    ImGui::PopID();
    DashArc(dl, rc, Sc(62), 0.0f, 2.0f * PI, A(P::Border()), Sc(3), Sc(4));

    // centre disc: online / total
    {
        dl->AddCircleFilled(rc, Sc(54), A(P::Card()), 64);
        dl->AddCircle(rc, Sc(54), A(P::Border()), 64, 1.0f);
        IconC(dl, 20, rc.x, rc.y - Sc(30), A(P::Accent()), ICON_FA_GHOST);
        char n1[8], n2[8];
        std::snprintf(n1, sizeof(n1), "%d", online_all);
        std::snprintf(n2, sizeof(n2), "/%d", static_cast<int>(modules.size()));
        ImFont* big = MonoFont(24, true);
        ImFont* sm = MonoFont(14, true);
        const float w1 = TextSize(big, 24, n1).x, w2 = TextSize(sm, 14, n2).x;
        const float sx = rc.x - (w1 + w2) * 0.5f;
        TextC(dl, big, 24, sx, rc.y - Sc(2), A(P::Text()), n1);
        TextC(dl, sm, 14, sx + w1, rc.y + Sc(3), A(P::Muted()), n2);
        ImFont* mf = MonoFont(8);
        SpacedText(dl, mf, 8, ImVec2(rc.x - SpacedWidth(mf, 8, "ONLINE", 1.5f) * 0.5f, rc.y + Sc(20)), A(P::Label(), 0.8f), "ONLINE", 1.5f);
    }

    // --- dock: client pages ---
    int tip = -1;
    ImVec2 tip_pos;
    ImGui::PushID("orbit_dock");
    for (int i = 0; i < 5; ++i) {
        const MV::CategoryTab& tab = MV::categories[4 + i];
        const float r = Sc(DOCK_D * 0.5f);
        const ImVec2 c(wp.x + Sc(DOCK_X + DOCK_D * 0.5f + i * (DOCK_D + DOCK_GAP)), wp.y + Sc(DOCK_Y + DOCK_D * 0.5f));
        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(c.x - r, c.y - r));
        ImGui::InvisibleButton("##dock", ImVec2(r * 2, r * 2));
        const bool hov = ImGui::IsItemHovered() && std::hypot(io.MousePos.x - c.x, io.MousePos.y - c.y) <= r;
        const bool clicked = hov && ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const float hv = Anim(ImGui::GetItemID(), (hov || sel == tab.page) ? 1.0f : 0.0f, 25.0f);
        ImGui::PopID();
        if (clicked) sel = tab.page;
        dl->AddCircleFilled(c, r, A(Mix(P::Card(), P::Soft(), hv)), 40);
        dl->AddCircle(c, r, A(Mix(P::Border(), P::Accent(), hv)), 40, 1.0f);
        IconC(dl, 16, c.x, c.y, A(Mix(P::Label(), P::Accent(), hv)), tab.icon);
        if (hov && sel != tab.page) { tip = 4 + i; tip_pos = ImVec2(c.x, c.y - r); }
    }
    ImGui::PopID();
    if (tip >= 0) { // tooltip pill, centred, top 30 px above the button top
        const std::string label = Upper(MV::categories[tip].label);
        ImFont* mf = MonoFont(9, true);
        const float tw = TextSize(mf, 9, label.c_str()).x, ph = Sc(18), pw = tw + Sc(16);
        const ImVec2 pmin(tip_pos.x - pw * 0.5f, tip_pos.y - Sc(30)); // spec: 30 px above (top to top)
        dl->AddRectFilled(pmin, ImVec2(pmin.x + pw, pmin.y + ph), A(P::Accent()), ph * 0.5f);
        TextC(dl, mf, 9, pmin.x + Sc(8), pmin.y + ph * 0.5f, A(Theme::Current().onAccent), label.c_str());
    }

    // --- user bar: logged-in account with avatar (like the classic/v1 footer) ---
    {
        const ImVec2 pmin(wp.x + Sc(24), wp.y + Sc(532)), pmax(pmin.x + Sc(282), pmin.y + Sc(44));
        const float cy = pmin.y + Sc(22);
        dl->AddRectFilled(pmin, pmax, A(P::Card()), Sc(10));
        dl->AddRect(pmin, pmax, A(P::Border()), Sc(10));
        const float ar = Sc(14);
        const ImVec2 ac(pmin.x + Sc(8) + ar, cy);
        dl->AddCircleFilled(ac, ar + Sc(1.5f), A(P::Border()), 32);
        if (const ImTextureID avatar = Menu::Resources::UserTexture()) // null until Menu::Images() uploaded the textures
            dl->AddImageRounded(avatar, ImVec2(ac.x - ar, ac.y - ar), ImVec2(ac.x + ar, ac.y + ar),
                                ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, static_cast<int>(255 * g_alpha)), ar);
        dl->AddCircle(ac, ar, A(P::Accent()), 32, Sc(1.5f));
        const ImVec2 st(ac.x + ar * 0.72f, ac.y + ar * 0.72f); // online dot
        dl->AddCircleFilled(st, Sc(4.5f), A(P::Card()), 16);
        dl->AddCircleFilled(st, Sc(3.2f), A(P::Success()), 16);
        ImFont* mf = MonoFont(9);
        const float tx = ac.x + ar + Sc(10);
        TextC(dl, mf, 9, tx, cy - Sc(8), A(P::Label(), 0.8f), "ONLINE");
        const char* end_txt = "END = unload";
        const float end_w = TextSize(mf, 9, end_txt).x;
        const float name_max = pmax.x - Sc(14) - end_w - Sc(6);
        ImFont* nf = TextFont(13, true);
        const char* name = Menu::Login::UserName().c_str();
        ImGui::PushClipRect(ImVec2(tx, pmin.y), ImVec2(name_max, pmax.y), true);
        TextC(dl, nf, 13, tx, cy + Sc(7), A(P::Text()), name);
        ImGui::PopClipRect();
        // BETA pill right after the name (as in the classic footer), only when it fits
        ImFont* bf = MonoFont(9, true);
        const ImVec2 bt = TextSize(bf, 9, "BETA");
        const float bx = ImFloor(tx + TextSize(nf, 13, name).x + Sc(8)), bw = bt.x + Sc(12), bh = Sc(15);
        if (Menu::Login::HasBeta() && bx + bw <= name_max) {
            const ImVec2 bmin(bx, ImFloor(cy + Sc(7) - bh * 0.5f));
            dl->AddRectFilled(bmin, ImVec2(bmin.x + bw, bmin.y + bh), A(P::Soft()), bh * 0.5f);
            dl->AddRect(bmin, ImVec2(bmin.x + bw, bmin.y + bh), A(P::Accent(), 0.35f), bh * 0.5f);
            TextC(dl, bf, 9, bmin.x + Sc(6), bmin.y + bh * 0.5f, A(P::Accent()), "BETA");
        }
        TextC(dl, mf, 9, pmax.x - Sc(14) - end_w, cy, A(P::Label(), 0.8f), end_txt);
    }

    // --- right zone: search + favorites filter (ring pages only) ---
    static char search[96] = {};
    static bool favorites_only = false;
    static int sel_mod = -1; // selected module (registry index)
    const bool ring_page = sel < 4;
    {
        // The search field is ImGui-driven but drawn by us: transparent input over our own background and border.
        const float sy = wp.y + Sc(26);
        const ImVec2 fmin(wp.x + w - Sc(24 + 40), sy), fmax(fmin.x + Sc(40), sy + Sc(40));
        const ImVec2 smin(fmin.x - Sc(8 + 190), sy), smax(smin.x + Sc(190), sy + Sc(40));
        if (ring_page) {
            ImGui::PushID("orbit_search");
            ImGui::SetCursorScreenPos(fmin);
            ImGui::InvisibleButton("##fav", ImVec2(Sc(40), Sc(40)));
            const bool fav_hov = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) favorites_only = !favorites_only;
            const float fv = Anim(ImGui::GetItemID(), fav_hov ? 1.0f : 0.0f, 25.0f);
            dl->AddRectFilled(fmin, fmax, A(favorites_only ? P::Accent() : Mix(P::Background(), P::Soft(), fv)), Sc(8));
            dl->AddRect(fmin, fmax, A(favorites_only ? P::Accent() : Mix(P::Border(), P::Label(), fv)), Sc(8));
            IconC(dl, 14, (fmin.x + fmax.x) * 0.5f, sy + Sc(20),
                  A(favorites_only ? Theme::Current().onAccent : Mix(P::Muted(), P::Accent(), fv)), ICON_FA_STAR);

            dl->AddRectFilled(smin, smax, A(P::Background()), Sc(8));
            IconC(dl, 10, smin.x + Sc(16), sy + Sc(20), A(P::Accent()), ICON_FA_CHEVRON_RIGHT);
            ImFont* mf = MonoFont(12);
            const float ix = smin.x + Sc(30), iw = (std::max)(10.0f, smax.x - Sc(10) - ix);
            ImGui::SetWindowFontScale(Fz(12) / mf->FontSize);
            ImGui::PushFont(mf);
            const float pad_y = (std::max)(0.0f, (Sc(40) - ImGui::GetFontSize()) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(ix, sy));
            ImGui::SetNextItemWidth(iw);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, pad_y));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Text, ColV(P::Text(), g_alpha));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, ColV(P::Muted(), g_alpha));
            ImGui::InputText("##search", search, sizeof(search)); // no hint: the spec has none and it would overlap the block cursor
            const bool focused = ImGui::IsItemActive();
            ImGui::PopStyleColor(5);
            ImGui::PopStyleVar(2);
            ImGui::PopFont();
            ImGui::SetWindowFontScale(g_fs);
            // static block cursor while the field is inactive (the active field shows the ImGui caret)
            if (!focused) {
                const float tw = search[0] ? mf->CalcTextSizeA(Fz(12), FLT_MAX, 0.0f, search).x : 0.0f;
                const float bx = (std::min)(ix + tw + Sc(2), smax.x - Sc(12));
                dl->AddRectFilled(ImVec2(bx, sy + Sc(13)), ImVec2(bx + Sc(7), sy + Sc(27)), A(P::Accent(), 0.8f));
            }
            dl->AddRect(smin, smax, A(focused ? P::Accent() : P::Border()), Sc(8));
            ImGui::PopID();
        }
    }

    // --- visible modules + selection handling ---
    const MV::Filter filter{search, favorites_only, false, ring_page ? sel : 0};
    std::vector<int> visible;
    if (ring_page) {
        visible = MV::VisibleSorted(modules, filter);
        if (std::find(visible.begin(), visible.end(), sel_mod) == visible.end()) sel_mod = visible.empty() ? -1 : visible.front();
    }
    const bool filtered = ring_page && (search[0] || favorites_only);
    // hotkey capture belongs to the selected module of a ring page; drop it when the selection or page changes
    if (Config::capturing_module_key >= 0 && (!ring_page || Config::capturing_module_key != sel_mod)) Config::capturing_module_key = -1;

    // --- right zone header ---
    {
        const MV::CategoryTab* tab = &MV::categories[0];
        int pos_in_group = 0;
        for (int c = 0; c < 9; ++c)
            if (MV::categories[c].page == sel) { tab = &MV::categories[c]; pos_in_group = ring_page ? c : c - 4; }
        char line1[64], line3[96];
        if (filtered) std::snprintf(line1, sizeof(line1), "ALL CATEGORIES \xC2\xB7 FILTER");
        else std::snprintf(line1, sizeof(line1), ring_page ? "%02d / 04 \xC2\xB7 CATEGORY" : "%02d / 05 \xC2\xB7 CLIENT", pos_in_group + 1);
        const char* title = tab->label;
        if (filtered) title = search[0] ? "Search" : "Favorites"; // short: the 44 px title must stay left of the search field
        if (ring_page) {
            int online = 0, favs = 0;
            for (int idx : visible) { if (*modules[idx].enabled) ++online; if (modules[idx].favorite) ++favs; }
            std::snprintf(line3, sizeof(line3), "%d MODULES \xC2\xB7 %d ONLINE \xC2\xB7 %d FAVORITES", static_cast<int>(visible.size()), online, favs);
        } else {
            line3[0] = 0;
        }
        const float hx = wp.x + Sc(CONTENT_X);
        ImFont* mf = MonoFont(10);
        SpacedText(dl, mf, 10, ImVec2(hx, wp.y + Sc(20)), A(P::Label(), 0.85f), line1, 1.5f);
        SpacedText(dl, TextFont(44, true), 44, ImVec2(hx, wp.y + Sc(36)), A(P::Text()), title, -1.5f);
        if (line3[0]) SpacedText(dl, mf, 10, ImVec2(hx, wp.y + Sc(92)), A(P::Label(), 0.85f), line3, 1.0f);
    }

    // --- honeycomb grid (ring pages) ---
    if (ring_page) {
        constexpr float CH_X = 336.0f, CH_Y = 100.0f, CH_W = 556.0f, CH_H = 176.0f; // child rect, zone-relative to the window
        constexpr float TILE_W = 76.0f, TILE_H = 88.0f, COL_STEP = 82.0f, ROW_STEP = 70.0f, GRID_X = RIGHT_X + 40.0f, GRID_Y = 110.0f;
        // tile centres (window-relative, unscaled): even rows 6 tiles, odd rows 5 tiles shifted by half a step
        const int count = static_cast<int>(visible.size());
        std::vector<ImVec2> tpos(count);
        int rows = 0;
        for (int k = 0, row = 0, col = 0; k < count; ++k) {
            tpos[k] = ImVec2(GRID_X + TILE_W * 0.5f + col * COL_STEP + (row % 2 ? COL_STEP * 0.5f : 0.0f), GRID_Y + TILE_H * 0.5f + row * ROW_STEP);
            rows = row + 1;
            if (++col == (row % 2 ? 5 : 6)) { ++row; col = 0; }
        }
        const ImVec2 cmin(wp.x + Sc(CH_X), wp.y + Sc(CH_Y)), cmax(cmin.x + Sc(CH_W), cmin.y + Sc(CH_H));
        float scroll_y = 0.0f, scroll_max = 0.0f;
        ImVec2 sel_c(0, 0);
        bool sel_visible = false;
        float sel_ap = 0.0f, sel_half = 0.0f; // fade-in progress and animated half height of the selected tile
        int hov_tile = -1;
        static int last_scope = -1;
        const int scope_key = sel * 1000003 + static_cast<int>(std::hash<std::string>()(search) & 0xFFFF) * 3 + (favorites_only ? 1 : 0);
        if (scope_key != last_scope) { ImGui::SetNextWindowScroll(ImVec2(0, 0)); last_scope = scope_key; }
        // the fade-in restarts on category / favorites change and when the search becomes empty or non-empty, not on every keystroke
        static int last_fade_key = -1;
        const int fade_key = sel * 4 + (favorites_only ? 1 : 0) + (search[0] ? 2 : 0);
        if (fade_key != last_fade_key) { last_fade_key = fade_key; grid_t0 = now; }
        ImGui::SetCursorScreenPos(cmin);
        ImGui::BeginChild("##orbit_grid", ImVec2(Sc(CH_W), Sc(CH_H)), false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
        {
            ImDrawList* cdl = ImGui::GetWindowDrawList();
            const ImVec2 org = ImGui::GetCursorScreenPos(); // child origin including scroll
            const float content_h = Sc(GRID_Y - CH_Y + (rows > 0 ? (rows - 1) * ROW_STEP + TILE_H : 0.0f)); // 2 rows = 168 <= child height, so up to 11 tiles never scroll
            ImGui::SetCursorScreenPos(org);
            ImGui::InvisibleButton("##grid", ImVec2(Sc(CH_W), content_h),
                                   ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            const bool grid_hov = ImGui::IsItemHovered();
            auto centre = [&](int k) { return ImVec2(org.x + Sc(tpos[k].x - CH_X), org.y + Sc(tpos[k].y - CH_Y)); };
            // tiles that stick out of the child (3rd row, scrolling) fade out instead of overlapping the tab/inspector
            auto edge_vis = [&](const ImVec2& c) {
                const float out = (std::max)(cmin.y - (c.y - Sc(TILE_H) * 0.5f), (c.y + Sc(TILE_H) * 0.5f) - cmax.y);
                return (std::max)(0.3f, ImSaturate(1.0f - out / Sc(20.0f))); // faint peek hints that the grid scrolls
            };
            if (grid_hov) { // point-in-hexagon (pointy top); bounding boxes overlap, so a single button covers the grid
                for (int k = 0; k < count && hov_tile < 0; ++k) {
                    const ImVec2 c = centre(k);
                    if (edge_vis(c) < 0.5f) continue; // faded tiles are not clickable
                    const float dx = std::fabs(io.MousePos.x - c.x) / g_s, dy = std::fabs(io.MousePos.y - c.y) / g_s;
                    if (dx <= TILE_W * 0.5f && dy <= TILE_H * 0.5f - dx * (TILE_H * 0.25f / (TILE_W * 0.5f))) hov_tile = k;
                }
            }
            if (hov_tile >= 0) {
                const int mi = visible[hov_tile];
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) { sel_mod = mi; press_mod = mi; press_t0 = now; }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) modules[mi].enabled->store(!modules[mi].enabled->load());
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) modules[mi].favorite = !modules[mi].favorite;
                if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                    sel_mod = mi;
                    Config::capturing_menu_key = false;
                    Config::capturing_module_key = mi;
                }
            }
            std::vector<char> truncated(count, 0);
            ImGui::PushID("orbit_tiles");
            for (int pass = 0; pass < 2; ++pass) { // selected tile last so its glow is not covered by neighbours
                for (int k = 0; k < count; ++k) {
                    const int mi = visible[k];
                    const bool is_sel = mi == sel_mod;
                    if (is_sel != (pass == 1)) continue;
                    const ModuleData& m = modules[mi];
                    const ImVec2 c = centre(k);
                    // staggered appearance (18 ms per tile) and a short press flash after a click
                    const float appear = ImSaturate(static_cast<float>(now - grid_t0) / 0.18f - k * 0.1f); // 0.18 s per tile, start offset 18 ms * k
                    const float ap = (1.0f - (1.0f - appear) * (1.0f - appear)) * edge_vis(c);
                    const float press = mi == press_mod ? 1.0f - ImSaturate(static_cast<float>(now - press_t0) / 0.3f) : 0.0f;
                    const float tw = Sc(TILE_W) * (0.85f + 0.15f * ap) * (1.0f + 0.06f * press), th = Sc(TILE_H) * (0.85f + 0.15f * ap) * (1.0f + 0.06f * press);
                    if (!ImGui::IsRectVisible(ImVec2(c.x - tw * 0.5f - Sc(12), c.y - th * 0.5f - Sc(12)), ImVec2(c.x + tw * 0.5f + Sc(12), c.y + th * 0.5f + Sc(12))) && !is_sel)
                        continue;
                    ImGui::PushID(mi);
                    const bool on = m.enabled->load();
                    const float hv = Anim(ImGui::GetID("h"), hov_tile == k ? 1.0f : 0.0f, 25.0f);
                    const float ov = Anim(ImGui::GetID("o"), on ? 1.0f : 0.0f, 18.75f);
                    const float sv = Anim(ImGui::GetID("s"), is_sel ? 1.0f : 0.0f, 18.75f);
                    ImGui::PopID();
                    const float tile_alpha = g_alpha;
                    g_alpha *= ap; // fade-in; restored at the end of this tile
                    if (sv > 0.01f || press > 0.01f) {
                        // the extra press glow must stay inside the grid child (rows 1/2 sit close to its edges)
                        const float room = (std::min)((std::min)(c.x - tw * 0.5f - cmin.x, cmax.x - c.x - tw * 0.5f),
                                                      (std::min)(c.y - th * 0.5f - cmin.y, cmax.y - c.y - th * 0.5f)) / g_s;
                        GlowHex(cdl, c, tw, th, 10.0f + (std::min)(6.0f * press, (std::max)(0.0f, room - 10.0f)), P::Accent(), (std::max)(sv, press));
                    }
                    HexFill(cdl, c, tw, th, A(Mix(Mix(P::Card(), P::Soft(), 0.6f * hv), P::Soft(), ov)));
                    HexOutline(cdl, c, tw, th, A(Mix(Mix(P::Border(), P::Label(), hv), ColA(P::Accent(), 0.55f), ov)), 1.0f);
                    if (sv > 0.01f) HexOutline(cdl, c, tw, th, A(P::Accent(), sv), Sc(2));

                    // LED (+ star for favorites), icon, name
                    const float led_y = c.y - Sc(30);
                    const ImVec2 led(c.x - (m.favorite ? Sc(6) : 0.0f), led_y);
                    if (ov > 0.01f) GlowDot(cdl, led, Sc(2.5f), 5, P::Accent(), ov);
                    cdl->AddCircleFilled(led, Sc(2.5f), A(Mix(P::Toggle(), P::Accent(), ov)), 12);
                    if (m.favorite) IconC(cdl, 8, c.x + Sc(6), led_y, A(P::Accent()), ICON_FA_STAR);
                    IconC(cdl, 20, c.x, c.y - Sc(8), A(Mix(Mix(P::Muted(), P::Text(), hv), P::Accent(), ov)), MV::ModuleIcon(m));

                    ImFont* nf = TextFont(8.5f, true);
                    const std::string up = Upper(m.name);
                    const float maxw = Sc(60);
                    const float scale = Fz(8.5f) / nf->FontSize;
                    const char* s0 = up.c_str();
                    const char* send = s0 + up.size();
                    const char* brk = nf->CalcWordWrapPositionA(scale, s0, send, maxw);
                    bool trunc = false;
                    std::string l1(s0, brk), l2;
                    while (!l1.empty() && l1.back() == ' ') l1.pop_back();
                    if (brk < send) {
                        while (brk < send && *brk == ' ') ++brk;
                        l2 = FitText(nf, 8.5f, Sc(50),std::string(brk, send), trunc); // narrower: the second line sits where the hexagon tapers
                    }
                    l1 = FitText(nf, 8.5f, maxw, l1, trunc);
                    truncated[k] = trunc;
                    const ImU32 ncol = A(Mix(P::Label(), P::Text(), ov));
                    const float l1y = c.y + (l2.empty() ? Sc(15) : Sc(9)); // 10 px text needs a bit more room than 8.5 px
                    TextC(cdl, nf, 8.5f, c.x - TextSize(nf, 8.5f, l1.c_str()).x * 0.5f, l1y, ncol, l1.c_str());
                    if (!l2.empty()) TextC(cdl, nf, 8.5f, c.x - TextSize(nf, 8.5f, l2.c_str()).x * 0.5f, c.y + Sc(21), ncol, l2.c_str());
                    g_alpha = tile_alpha;
                    if (is_sel) { sel_ap = ap; sel_half = th * 0.5f; }
                    if (is_sel) { sel_c = c; sel_visible = c.y - Sc(TILE_H) * 0.5f >= cmin.y - 1.0f && c.y + Sc(TILE_H) * 0.5f <= cmax.y + 1.0f; }
                }
            }
            ImGui::PopID();
            if (hov_tile >= 0 && truncated[hov_tile]) ImGui::SetTooltip("%s", modules[visible[hov_tile]].name);
            scroll_y = ImGui::GetScrollY();
            scroll_max = ImGui::GetScrollMaxY();
        }
        ImGui::EndChild();

        if (scroll_max > 1.0f) { // thin Orbit-style scroll indicator (wheel only)
            const float tx = cmax.x - Sc(5), th = cmax.y - cmin.y - Sc(8);
            const float thumb = (std::max)(Sc(18), th * (cmax.y - cmin.y) / (cmax.y - cmin.y + scroll_max));
            const float ty = cmin.y + Sc(4) + (th - thumb) * (scroll_y / scroll_max);
            dl->AddRectFilled(ImVec2(tx, cmin.y + Sc(4)), ImVec2(tx + Sc(3), cmin.y + Sc(4) + th), A(P::Toggle(), 0.5f), Sc(2));
            dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + Sc(3), ty + thumb), A(P::Accent(), 0.8f), Sc(2));
        }
        if (count == 0) {
            const char* msg = favorites_only ? "NO FAVORITES MATCH" : "NO MATCHING MODULES";
            ImFont* mf = MonoFont(10);
            TextC(dl, mf, 10, wp.x + Sc(RIGHT_X + 40), wp.y + Sc(180), A(P::Muted()), msg);
        }

        // --- inspector tab + card ---
        {
            const ImVec2 imin(wp.x + Sc(RIGHT_X + 21), wp.y + Sc(302)), imax(imin.x + Sc(524), imin.y + Sc(282));
            const ImVec2 tmin(imin.x, wp.y + Sc(276)), tmax(tmin.x + Sc(170), imin.y + Sc(1));
            const ImDrawFlags card_flags = ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottom;
            dl->AddRectFilled(imin, imax, A(P::Card()), Sc(18), card_flags);
            dl->AddRect(imin, imax, A(P::Border()), Sc(18), card_flags, 1.0f);
            dl->AddRectFilled(tmin, tmax, A(P::Card()), Sc(10), ImDrawFlags_RoundCornersTop);
            dl->AddRect(tmin, tmax, A(P::Border()), Sc(10), ImDrawFlags_RoundCornersTop, 1.0f);
            dl->AddLine(ImVec2(tmin.x + Sc(1), imin.y), ImVec2(tmax.x - Sc(1), imin.y), A(P::Card()), Sc(2)); // joins tab and card
            char tag[24] = "---";
            if (sel_mod >= 0) {
                std::snprintf(tag, sizeof(tag), "%.3s\xC2\xB7%02d", Upper(MV::categories[(std::max)(0, (std::min)(3, modules[sel_mod].category))].label).c_str(),
                              static_cast<int>(std::find(visible.begin(), visible.end(), sel_mod) - visible.begin()) + 1);
            }
            ImFont* mf = MonoFont(10);
            const float ty = tmin.y + Sc(27) * 0.5f;
            float tx = tmin.x + Sc(14);
            TextC(dl, mf, 10, tx, ty, A(P::Accent()), tag);
            tx += TextSize(mf, 10, tag).x + Sc(8);
            IconC(dl, 7, tx + Sc(3), ty, A(P::Muted()), ICON_FA_CHEVRON_RIGHT);
            TextC(dl, mf, 10, tx + Sc(12), ty, A(P::Label()), "INSPECT");

            if (sel_mod < 0) {
                TextC(dl, mf, 10, imin.x + Sc(18), imin.y + Sc(30), A(P::Muted()), "NO MODULE SELECTED");
            } else {
                ModuleData& m = modules[sel_mod];
                const int mi = sel_mod;
                const float cy = imin.y + Sc(18 + 22);
                ImGui::PushID("orbit_insp");

                // right-aligned controls, built from the right edge: reset, star, TOG/HOLD, keycap
                const bool mode_used = !m.action_request; // press-action modules ignore the mode
                float rx = imax.x - Sc(18);
                const float cd = Sc(30);
                auto round_button = [&](const char* id, float right, const char* icon, bool active, const char* tip_text) {
                    const ImVec2 c(right - cd * 0.5f, cy);
                    ImGui::SetCursorScreenPos(ImVec2(c.x - cd * 0.5f, c.y - cd * 0.5f));
                    ImGui::InvisibleButton(id, ImVec2(cd, cd));
                    const bool hov = ImGui::IsItemHovered() && std::hypot(io.MousePos.x - c.x, io.MousePos.y - c.y) <= cd * 0.5f;
                    const bool clicked = hov && ImGui::IsItemClicked(ImGuiMouseButton_Left);
                    const float hv = Anim(ImGui::GetItemID(), hov ? 1.0f : 0.0f, 25.0f);
                    dl->AddCircleFilled(c, cd * 0.5f, A(P::Background()), 24);
                    dl->AddCircle(c, cd * 0.5f, A(Mix(P::Border(), P::Label(), hv)), 24, 1.0f);
                    IconC(dl, 12, c.x, c.y, A(active ? P::Accent() : Mix(P::Muted(), P::Label(), hv)), icon);
                    if (hov) ImGui::SetTooltip("%s", tip_text);
                    return clicked;
                };
                if (round_button("##reset", rx, ICON_FA_UNDO, false, "Reset module")) MV::ResetModule(modules, mi);
                rx -= cd + Sc(12);
                if (round_button("##star", rx, ICON_FA_STAR, m.favorite, m.favorite ? "Remove favorite" : "Add favorite")) m.favorite = !m.favorite;
                rx -= cd + Sc(12);

                if (mode_used) {
                    ImFont* bf = MonoFont(9, true);
                    const float w1 = TextSize(bf, 9, "TOG").x + Sc(14), w2 = TextSize(bf, 9, "HOLD").x + Sc(14), sh = Sc(24);
                    const ImVec2 smin(rx - w1 - w2 - Sc(4), cy - sh * 0.5f);
                    dl->AddRectFilled(smin, ImVec2(rx, smin.y + sh), A(P::Background()), Sc(8));
                    dl->AddRect(smin, ImVec2(rx, smin.y + sh), A(P::Border()), Sc(8));
                    const bool hold = m.hotkey_mode == Input::HotkeyMode::Hold;
                    const char* names[2] = {"TOG", "HOLD"};
                    const float widths[2] = {w1, w2};
                    float sx = smin.x + Sc(2);
                    for (int mode = 0; mode < 2; ++mode) {
                        const bool active = (mode == 1) == hold;
                        ImGui::SetCursorScreenPos(ImVec2(sx, smin.y + Sc(2)));
                        ImGui::PushID(mode);
                        if (ImGui::InvisibleButton("##mode", ImVec2(widths[mode], sh - Sc(4))))
                            m.hotkey_mode = mode == 1 ? Input::HotkeyMode::Hold : Input::HotkeyMode::Toggle;
                        ImGui::PopID();
                        if (active) dl->AddRectFilled(ImVec2(sx, smin.y + Sc(2)), ImVec2(sx + widths[mode], smin.y + sh - Sc(2)), A(P::Soft()), Sc(6));
                        TextC(dl, bf, 9, sx + (widths[mode] - TextSize(bf, 9, names[mode]).x) * 0.5f, cy, A(active ? P::Accent() : P::Muted()), names[mode]);
                        sx += widths[mode];
                    }
                    rx = smin.x - Sc(12);
                }

                // keycap (logic as clickgui.cpp: click = capture, right click = clear, conflict tooltip)
                const bool capturing = Config::capturing_module_key == mi;
                const bool has_key = m.hotkey != 0;
                const bool conflict = has_key && Menu::Registry::HasConflict(m.hotkey, mi);
                std::string key_txt = capturing ? "\xC2\xB7\xC2\xB7\xC2\xB7" : has_key ? Menu::GetKeyName(m.hotkey) : "+";
                if (conflict && !capturing) key_txt += " !";
                ImFont* kf = MonoFont(10, true);
                const float kw = (std::max)(Sc(30), TextSize(kf, 10, key_txt.c_str()).x + Sc(16));
                const ImVec2 kmin(rx - kw, cy - cd * 0.5f), kmax(rx, cy + cd * 0.5f);
                ImGui::SetCursorScreenPos(kmin);
                if (ImGui::InvisibleButton("##hotkey", ImVec2(kw, cd))) {
                    Config::capturing_menu_key = false;
                    Config::capturing_module_key = capturing ? -1 : mi;
                }
                const bool khov = ImGui::IsItemHovered();
                if (khov && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) { m.hotkey = 0; if (capturing) Config::capturing_module_key = -1; }
                const float kv = Anim(ImGui::GetItemID(), khov ? 1.0f : 0.0f, 25.0f);
                const float pulse = capturing ? 0.75f + 0.25f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.2831853f / 0.8f) : 1.0f;
                if (capturing) DrawGlowRect(dl, kmin, kmax, Sc(6), P::Accent(), g_alpha * pulse * 0.55f, 12);
                dl->AddRectFilled(kmin, kmax, A(P::Border()), Sc(6)); // 3 px bottom edge
                dl->AddRectFilled(kmin, ImVec2(kmax.x, kmax.y - Sc(3)), A(capturing ? P::Soft() : Mix(P::Background(), P::Soft(), kv)), Sc(6));
                if (capturing || conflict) dl->AddRect(kmin, kmax, A(P::Accent(), pulse), Sc(6), 0, 1.5f);
                else if (!has_key) { // empty keycap: dashed outline
                    const ImU32 dc = A(Mix(P::Border(), P::Label(), kv));
                    DashLine(dl, kmin, ImVec2(kmax.x, kmin.y), dc, Sc(3), Sc(3));
                    DashLine(dl, ImVec2(kmax.x, kmin.y), kmax, dc, Sc(3), Sc(3));
                    DashLine(dl, kmax, ImVec2(kmin.x, kmax.y), dc, Sc(3), Sc(3));
                    DashLine(dl, ImVec2(kmin.x, kmax.y), kmin, dc, Sc(3), Sc(3));
                }
                TextC(dl, kf, 10, kmin.x + (kw - TextSize(kf, 10, key_txt.c_str()).x) * 0.5f, cy - Sc(1.5f),
                      A(capturing ? P::Accent() : has_key ? (conflict ? P::Accent() : P::Text()) : P::Muted()), key_txt.c_str());
                if (khov && conflict) MV::DrawConflictTooltip(modules, mi);
                const float text_right = kmin.x - Sc(14);

                // power button
                {
                    const ImVec2 pc(imin.x + Sc(18 + 22), cy);
                    const float pr = Sc(22);
                    ImGui::SetCursorScreenPos(ImVec2(pc.x - pr, pc.y - pr));
                    ImGui::InvisibleButton("##power", ImVec2(pr * 2, pr * 2));
                    const bool phov = ImGui::IsItemHovered() && std::hypot(io.MousePos.x - pc.x, io.MousePos.y - pc.y) <= pr;
                    if (phov && ImGui::IsItemClicked(ImGuiMouseButton_Left)) m.enabled->store(!m.enabled->load());
                    const bool on = m.enabled->load();
                    const float pv = Anim(ImGui::GetItemID(), phov ? 1.0f : 0.0f, 25.0f);
                    const float po = Anim(ImGui::GetID("##power_on"), on ? 1.0f : 0.0f, 18.75f);
                    if (po > 0.01f) {
                        DrawGlowCircle(dl, pc, pr, P::Accent(), g_alpha * po * 0.6f, 12);
                        dl->AddCircle(pc, pr + Sc(2), A(P::Accent(), ALPHA_POWER_RING * po), 40, Sc(4));
                    }
                    dl->AddCircleFilled(pc, pr, A(Mix(P::Background(), P::Accent(), po)), 40);
                    dl->AddCircle(pc, pr - Sc(0.75f), A(Mix(Mix(P::Border(), P::Label(), pv), P::Accent(), po)), 40, Sc(1.5f));
                    IconC(dl, 16, pc.x, pc.y, A(Mix(P::Muted(), Theme::Current().onAccent, po)), ICON_FA_POWER_OFF);
                }

                // name + one-line description, both clipped with ellipsis; full text in a tooltip
                {
                    const float nx = imin.x + Sc(18 + 44 + 14);
                    const float maxw = (std::max)(Sc(40), text_right - nx);
                    bool t1 = false, t2 = false;
                    ImFont* nf = TextFont(22, true);
                    const std::string name = FitText(nf, 22, maxw, m.name, t1);
                    TextC(dl, nf, 22, nx, cy - Sc(10), A(P::Text()), name.c_str());
                    const char* desc_full = Menu::Registry::Description(m.name);
                    ImFont* df = TextFont(12);
                    const std::string desc = FitText(df, 12, maxw, desc_full ? desc_full : "", t2);
                    TextC(dl, df, 12, nx, cy + Sc(15), A(P::Label()), desc.c_str());
                    if ((t1 || t2) && ImGui::IsMouseHoveringRect(ImVec2(nx, cy - Sc(22)), ImVec2(nx + maxw, cy + Sc(22))) && ImGui::IsWindowHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(m.name);
                        if (desc_full && desc_full[0]) ImGui::TextUnformatted(desc_full);
                        ImGui::EndTooltip();
                    }
                }
                ImGui::PopID();

                // --- settings body: scrollable child (wheel + thin indicator), switches/colours left, sliders right ---
                DashLine(dl, ImVec2(imin.x + Sc(18), imin.y + Sc(74)), ImVec2(imax.x - Sc(18), imin.y + Sc(74)), A(P::Border()), Sc(4), Sc(4));
                const ImVec2 bmin(imin.x + Sc(18), imin.y + Sc(87));
                const ImVec2 bsize(Sc(488), imax.y - Sc(18) - bmin.y);
                const bool oresim = m.enabled == &OreSim_Enabled;
                float body_scroll = 0.0f, body_max = 0.0f;
                ImGui::SetCursorScreenPos(bmin);
                ImGui::PushID(mi); // scroll position per module
                ImGui::BeginChild("##orbit_settings", bsize, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
                {
                    // no SetWindowFontScale: direct child of the main window inherits g_fs
                    ImDrawList* cdl = ImGui::GetWindowDrawList();
                    const ImVec2 org = ImGui::GetCursorScreenPos();
                    float y0 = 0.0f;
                    if (oresim) { // seed field: own full-width row, fixed pixel layout like v1
                        ImGui::SetCursorScreenPos(org);
                        MV::DrawOreSimSeedInput(cdl, org, bsize.x, g_alpha);
                        y0 = MV::ORESIM_SEED_ROW_HEIGHT + Sc(12);
                    }
                    const float col_w = Sc(230), col_gap = Sc(28), row_h = Sc(30), row_gap = Sc(4), slider_h = Sc(46), slider_gap = Sc(10);
                    int nsw = 0, nsl = 0;
                    for (const auto& s : m.settings_list) (s.type == SettingType::SliderInt ? nsl : nsw)++;
                    const bool spread = nsw == 0 || nsl == 0; // only one kind: use both columns
                    float cy_[2] = {y0, y0};
                    int idx = 0;
                    for (auto& s : m.settings_list) {
                        const bool slider = s.type == SettingType::SliderInt;
                        const int col = spread ? (idx++ % 2) : (slider ? 1 : 0);
                        const ImVec2 pos(org.x + col * (col_w + col_gap), org.y + cy_[col]);
                        if (slider) { SliderRow(cdl, pos, col_w, s); cy_[col] += slider_h + slider_gap; }
                        else {
                            if (s.type == SettingType::ColorRGB) ColorRow(cdl, pos, col_w, s);
                            else CheckRow(cdl, pos, col_w, s);
                            cy_[col] += row_h + row_gap;
                        }
                    }
                    if (m.settings_list.empty() && !oresim) TextC(cdl, MonoFont(10), 10, org.x, org.y + Sc(10), A(P::Muted()), "NO SETTINGS");
                    const float total = (std::max)(cy_[0], cy_[1]);
                    ImGui::SetCursorScreenPos(ImVec2(org.x, org.y + (std::max)(total, Sc(1)) - 1.0f)); // defines the scroll extent
                    ImGui::Dummy(ImVec2(1, 1));
                    body_scroll = ImGui::GetScrollY();
                    body_max = ImGui::GetScrollMaxY();
                }
                ImGui::EndChild();
                ImGui::PopID();
                if (body_max > 1.0f) { // thin Orbit-style indicator (wheel only)
                    const float tx = imax.x - Sc(11), top = bmin.y, th = bsize.y;
                    const float thumb = (std::max)(Sc(18), th * th / (th + body_max));
                    const float ty = top + (th - thumb) * (body_scroll / body_max);
                    dl->AddRectFilled(ImVec2(tx, top), ImVec2(tx + Sc(3), top + th), A(P::Toggle(), 0.5f), Sc(2));
                    dl->AddRectFilled(ImVec2(tx, ty), ImVec2(tx + Sc(3), ty + thumb), A(P::Accent(), 0.8f), Sc(2));
                }
            }
        }

        // --- connection line (drawn after the card so the end dot sits on top): lower tip of the selected tile to the inspector or its tab ---
        // a new selection (also tile -> tile) restarts the growth and the travelling light
        const bool line_on = sel_mod >= 0 && sel_visible;
        if (!line_on) {
            line_mod = -2; // hidden (scrolled out / nothing selected): grows again with the comet when it returns
        } else {
            if (sel_mod != line_mod) { line_mod = sel_mod; line_g = 0.0f; comet_t0 = now; comet_repeat = false; }
            if (sel_ap < 0.9f) { line_g = 0.0f; comet_t0 = now; } // wait until the tile has faded in
            else if (comet_repeat) line_g = 1.0f;
            else { // the line tip is the light: both use the same smoothstep and arrive together
                const float tp = ImSaturate(static_cast<float>(now - comet_t0) / COMET_TRAVEL);
                line_g = tp * tp * (3.0f - 2.0f * tp);
            }
        }
        if (line_on) {
            const float line_alpha = g_alpha;
            g_alpha *= sel_ap;
            const bool tab_zone = (sel_c.x - wp.x) / g_s - RIGHT_X < 191.0f; // plan sec. 4: the line would cross the tab
            const float y_end = wp.y + Sc(tab_zone ? 276.0f : 302.0f);
            const ImVec2 a(sel_c.x, sel_c.y + sel_half), b(sel_c.x, y_end);
            if (b.y > a.y) {
                const ImVec2 e(a.x, a.y + (b.y - a.y) * line_g); // current end of the growing line
                const float glow = Theme::MenuGlow();
                if (glow > 0.0f)
                    for (int l = 3; l >= 1; --l)
                        dl->AddLine(a, e, A(P::Accent(), ALPHA_GLOW * glow * 0.3f / l), Sc(2.0f + 3.0f * l));
                dl->AddLine(a, e, A(P::Accent()), Sc(2));

                // travelling light ("comet") with a short fading tail; repeats every ~4.5 s at half strength
                double age = now - comet_t0;
                if (age > 4.5) { comet_t0 = now; comet_repeat = true; age = 0.0; }
                const float gain = comet_repeat ? 0.5f : 1.0f;
                constexpr float travel = COMET_TRAVEL, flash_len = 0.4f;
                const float tp = ImSaturate(static_cast<float>(age) / travel);
                const float f = (std::min)(tp * tp * (3.0f - 2.0f * tp), line_g); // smoothstep, never ahead of the grown line
                const ImU32 hot = Theme::Current().hover;
                if (age < travel) {
                    for (int i = 5; i >= 1; --i) { // tail
                        const float tf = f - i * 0.04f;
                        if (tf <= 0.0f) continue;
                        const float fade = 1.0f - i / 6.0f;
                        dl->AddCircleFilled(ImVec2(a.x, a.y + (b.y - a.y) * tf), Sc(2.4f) * (0.5f + 0.5f * fade), A(hot, 0.5f * fade * fade * gain), 12);
                    }
                    const ImVec2 head(a.x, a.y + (b.y - a.y) * f);
                    GlowDot(dl, head, Sc(2.5f), 7, P::Accent(), gain);
                    dl->AddCircleFilled(head, Sc(2.6f), A(hot, gain), 16);
                }

                // end dot; flashes when the light arrives
                const float dot_a = ImSaturate((line_g - 0.85f) / 0.15f);
                const float fl = (age >= travel && age < travel + flash_len) ? 1.0f - static_cast<float>(age - travel) / flash_len : 0.0f;
                if (dot_a > 0.01f) {
                    const float saved_alpha = g_alpha;
                    g_alpha *= dot_a;
                    GlowDot(dl, b, Sc(5), 6.0f + 8.0f * fl, P::Accent(), 1.0f + fl * gain);
                    dl->AddCircleFilled(b, Sc(5.0f + 1.5f * fl * gain), A(Mix(P::Accent(), hot, fl * gain)), 20);
                    if (fl > 0.01f) dl->AddCircle(b, Sc(5.0f + 10.0f * (1.0f - fl)), A(P::Accent(), 0.7f * fl * gain), 24, Sc(1.5f));
                    g_alpha = saved_alpha;
                }
            }
            g_alpha = line_alpha;
        }
    }
    if (!ring_page) line_mod = -2; // returning from a dock page re-grows the line

    // --- client page (dock selection): unchanged page code in a scrollable child ---
    if (sel >= 4) {
        const ImVec2 area_min(wp.x + Sc(CONTENT_X), wp.y + Sc(CONTENT_TOP));
        const float cw = (std::max)(1.0f, w - Sc(CONTENT_X) - Sc(CONTENT_MARGIN));
        const float ch = (std::max)(1.0f, h - Sc(CONTENT_TOP) - Sc(CONTENT_MARGIN));
        ImGui::SetCursorScreenPos(area_min);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 11.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ColV(P::Soft(), g_alpha * 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ColV(P::Muted(), g_alpha * 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ColV(P::Accent(), g_alpha * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ColV(P::Accent(), g_alpha));
        ImGui::PushID(sel); // own scroll state per page
        ImGui::BeginChild("##orbit_page", ImVec2(cw, ch), false, ImGuiWindowFlags_NoBackground);
        ImGui::SetWindowFontScale(g_fs); // client pages inherit font_size_percent (CalcFontSize only uses the direct parent's scale)
        const ImVec2 amin = ImGui::GetCursorScreenPos();
        MV::DrawClientPage(sel, ImGui::GetWindowDrawList(), amin, cw, ch, g_alpha, modules); // pages lay out relative to the real width
        ImGui::SetCursorScreenPos(ImVec2(amin.x + cw - 1.0f, amin.y + ch - 1.0f)); // defines the scroll extent
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild();
        ImGui::PopID();
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }

    dl->AddRect(wp, ImVec2(wp.x + w, wp.y + h), A(P::Border()), Sc(WIN_ROUNDING), 0, 1.0f);
    if (Config::particles_enabled) Menu::Particles();
    Menu::History::EndFrame();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

// Display-only mapping of the raw server text (spec section 3); responseText and StatusText stay untouched.
struct LoginError {
    std::string text, code; // code empty = no raw-code suffix in the pill
    const char* icon = ICON_FA_EXCLAMATION_TRIANGLE;
    int field = 0; // 1 = username, 2 = password
    bool retry = false, wifi_off = false;
};
static LoginError MapLoginError(const std::string& raw) {
    LoginError e;
    if (raw == "FAIL") { e = {"Invalid credentials", raw, ICON_FA_LOCK, 2}; }
    else if (raw == "User not Found") { e = {"User not found", raw, ICON_FA_USER, 1}; }
    else if (raw == "LICENSE_INVALID" || raw == "NO_LICENSE") { e = {"No active license", raw, ICON_FA_KEY}; }
    else if (raw == "License expired") { e = {"License expired", "", ICON_FA_KEY}; }
    else if (raw == "HWID_MISMATCH") { e = {"HWID mismatch", raw, ICON_FA_LOCK}; }
    else if (raw == "This User is banned") { e = {"Account banned", "", ICON_FA_LOCK}; }
    else if (raw == "Empty response" || raw == "Login request failed") { e = {"Server not reachable", "NO RESPONSE", ICON_FA_WIFI, 0, true, true}; }
    else { // unknown server text: first line only, capped, never cut inside a UTF-8 sequence
        e.text = raw.substr(0, raw.find_first_of("\r\n"));
        if (e.text.size() > 60) {
            size_t n = 60;
            while (n > 0 && (static_cast<unsigned char>(e.text[n]) & 0xC0) == 0x80) --n;
            e.text.resize(n);
        }
    }
    return e;
}

// Orbit login (docs/login-orbit-design.md sections 1-3). State comes from the View only; intro and morph follow in step 5.
bool DrawLogin(Menu::Login::View& v) {
    constexpr float LW = 420.0f, LH = 540.0f, LR = 20.0f;
    constexpr float FX = 40.0f, FW = 340.0f, FH = 44.0f; // field column
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 display = io.DisplaySize;
    const float fit = (std::min)((display.x - 32.0f) / LW, (display.y - 32.0f) / LH);
    g_s = (std::max)(0.1f, (std::min)(Config::menu_size_percent.load() / 100.0f, fit));
    g_fs = Config::font_size_percent.load() / 100.0f;
    const float w = LW * g_s, h = LH * g_s;
    const ImU32 err_col = IM_COL32(215, 85, 105, 255);
    const double now = ImGui::GetTime();

    // state, derived from the view only
    const std::string& raw = *v.responseText;
    const bool loading = v.loginInProgress;
    const bool success = v.login_success_pending || v.success_exit;
    const bool error = !raw.empty() && raw != "OK" && !loading && !success;
    const LoginError err = error ? MapLoginError(raw) : LoginError{};

    static bool was_loading = false, was_success = false, was_error = false;
    static double load_t0 = 0.0, success_t0 = 0.0, shake_t0 = -10.0;
    static std::string seen_raw;
    static bool field_err_cleared = false; // typing after an error resets the field style, the pill stays
    if (loading && !was_loading) load_t0 = now;
    if (success && !was_success) success_t0 = now;
    if (error && !was_error) shake_t0 = now;
    was_loading = loading; was_success = success; was_error = error;
    if (raw.empty() || raw != seen_raw) { seen_raw = raw; field_err_cleared = false; }

    const float st = static_cast<float>(now - shake_t0);
    const float shake = st >= 0.0f && st < 0.3f ? Sc(6) * std::sin(2.0f * PI * 5.0f * st) * (1.0f - st / 0.3f) : 0.0f; // 3 swings, damped

    // success timeline: hold + exit = 1.8 s; from 1.1 s the Orbit menu follows (morph) or the window fades out
    constexpr float MORPH_T0 = 1.1f, MORPH_FADE = 0.15f;
    const float total = v.success_hold + v.success_exit_duration;
    const float tsince = v.success_exit ? v.success_hold + v.success_exit_t : v.success_timer;
    const bool orbit_next = Config::menu_style == 2;
    const float mlen = (std::max)(total - MORPH_T0, 0.0f);
    const float morph_raw = !success ? 0.0f : mlen > 0.0f ? ImSaturate((tsince - MORPH_T0) / mlen) : (tsince >= MORPH_T0 ? 1.0f : 0.0f);
    const float mo = orbit_next ? (morph_raw < 0.5f ? 2.0f * morph_raw * morph_raw : 1.0f - std::pow(-2.0f * morph_raw + 2.0f, 2.0f) / 2.0f) : 0.0f;
    const float cf = orbit_next ? 1.0f - ImSaturate(morph_raw * mlen / MORPH_FADE) : 1.0f; // content fade-out
    const float base = v.login_alpha * (orbit_next ? 1.0f : 1.0f - morph_raw);
    g_alpha = base;
    if (mo > 0.0f) g_login_handoff_frame = ImGui::GetFrameCount();

    // intro (only while intro_done is false): phase 0..1 of a time window, ease-out cubic
    auto ph = [&](float t0, float t1) {
        if (*v.intro_done) return 1.0f;
        const float x = 1.0f - ImSaturate((*v.intro_t - t0) / (t1 - t0));
        return 1.0f - x * x * x;
    };

    // window rect of Orbit::Draw (same scale, centring and previous-position clamp) as morph target
    const float so_fit = (std::min)((display.x - 32.0f) / WIN_W, (display.y - 32.0f) / WIN_H);
    const float so = (std::max)(0.1f, (std::min)(Config::menu_size_percent.load() / 100.0f, so_fit));
    const ImVec2 osz(WIN_W * so, WIN_H * so);
    ImVec2 opos((display.x - osz.x) * 0.5f, (display.y - osz.y) * 0.5f);
    if (const ImGuiWindow* previous = ImGui::FindWindowByName(WINDOW_ID))
        opos = ImVec2(ImClamp(previous->Pos.x, 0.0f, (std::max)(0.0f, display.x - osz.x)),
                      ImClamp(previous->Pos.y, 0.0f, (std::max)(0.0f, display.y - osz.y)));
    const ImVec2 lpos((display.x - w) * 0.5f + shake, (display.y - h) * 0.5f);
    const ImVec2 wsize(w + (osz.x - w) * mo, h + (osz.y - h) * mo);

    bool submit = false;
    ImGui::SetNextWindowSize(wsize);
    ImGui::SetNextWindowPos(ImVec2(lpos.x + (opos.x - lpos.x) * mo, lpos.y + (opos.y - lpos.y) * mo));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, base);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Sc(LR) + (so * 20.0f - Sc(LR)) * mo);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, P::Background());
    ImGui::Begin("###login_orbit", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(g_fs);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    auto at = [&](float x, float y) { return ImVec2(wp.x + Sc(x), wp.y + Sc(y)); };

    const float err_t = Anim(ImGui::GetID("##err"), error ? 1.0f : 0.0f, 15.0f);     // ~200 ms to the error colour
    const float close_t = Anim(ImGui::GetID("##close"), success ? 1.0f : 0.0f, 12.0f); // ~250 ms: segments close

    dl->AddRect(wp, ImVec2(wp.x + wsize.x, wp.y + wsize.y), A(P::Border()), Sc(LR) + (so * 20.0f - Sc(LR)) * mo, 0, 1.0f);
    if (mo <= 0.0f) {
        DotGrid(dl, wp, 12.0f, LW - 12.0f, 12.0f, LH - 12.0f);
    } else { // login lattice fades out while the menu's right-zone lattice (same anchors as Orbit::Draw) fades in
        g_alpha = base * (1.0f - mo);
        DotGrid(dl, wp, 12.0f, wsize.x / g_s - 12.0f, 12.0f, wsize.y / g_s - 12.0f);
        const float login_s = g_s;
        g_s = so;
        g_alpha = base * mo;
        DotGrid(dl, wp, RIGHT_X + 12.0f, WIN_W - 12.0f, 12.0f, WIN_H - 12.0f);
        g_s = login_s;
    }
    g_alpha = base * cf;
    const ImVec2 rc = at(210, 132);
    DrawGlowCircle(dl, rc, Sc(80), Mix(P::Accent(), err_col, err_t), g_alpha * (success ? 0.6f : 0.33f), 12); // ~10 % peak (18 % on success)

    // header: brand + version chip, status on the right
    g_alpha = base * cf;
    {
        const ImVec2 b = at(24, 22);
        ImFont* mono = MonoFont(12, true);
        const float x1 = b.x + TextSize(mono, 12, "phantom").x, x2 = x1 + TextSize(mono, 12, "/").x;
        dl->AddText(mono, Fz(12), b, A(P::Text()), "phantom");
        dl->AddText(mono, Fz(12), ImVec2(x1, b.y), A(P::Accent()), "/");
        dl->AddText(mono, Fz(12), ImVec2(x2, b.y), A(P::Text()), "ui");
        ImFont* chip_font = MonoFont(10);
        const float cx = x2 + TextSize(mono, 12, "ui").x + Sc(10), cw = TextSize(chip_font, 10, "MC 26.2").x + Sc(14), ch = Sc(18);
        const float cy = b.y + Fz(12) * 0.5f;
        dl->AddRect(ImVec2(cx, cy - ch * 0.5f), ImVec2(cx + cw, cy + ch * 0.5f), A(P::Border()), Sc(5));
        TextC(dl, chip_font, 10, cx + Sc(7), cy, A(P::Muted()), "MC 26.2");
        char auth[32] = "AUTH";
        if (loading) std::snprintf(auth, sizeof(auth), "AUTH \xC2\xB7 %.1f s", now - load_t0);
        else if (success) std::snprintf(auth, sizeof(auth), "AUTH \xC2\xB7 OK");
        TextC(dl, chip_font, 10, wp.x + w - Sc(24) - TextSize(chip_font, 10, auth).x, cy, A(P::Muted()), auth);
    }

    // ring: 4 segments (82 deg, 8 deg gaps, start -86). Input: top-right accent (segment 0, -86..-4 deg). Loading: all accent, rotating, alpha 1/.5/.65/.8.
    // Success: segments grow to 90 deg (closed circle). Error: error colour. Morph: centre/radius/width/span move to the menu ring.
    const ImVec2 menu_rc(wp.x + RING_CX * so, wp.y + RING_CY * so);
    const ImVec2 rrc(rc.x + (menu_rc.x - rc.x) * mo, rc.y + (menu_rc.y - rc.y) * mo);
    {
        g_alpha = base;
        const float ring_r = Sc(60) + (RING_R * so - Sc(60)) * mo, ring_w = Sc(10) + (RING_W * so - Sc(10)) * mo;
        static constexpr float load_alpha[4] = {1.0f, 0.5f, 0.65f, 0.8f};
        const float rot = loading ? std::fmod(static_cast<float>(now - load_t0) / 1.2f, 1.0f) * 2.0f * PI : 0.0f;
        for (int i = 0; i < 4; ++i) {
            const float grow = ph(0.06f * i, 0.06f * i + 0.62f); // intro: each segment draws 0 -> full, staggered 60 ms
            const float full = RING_SPAN + (90.0f - RING_SPAN) * close_t;
            const float a0 = (RING_START + 90.0f * i) * DEG + rot, a1 = a0 + (full + (RING_SPAN - full) * mo) * grow * DEG;
            ImU32 col = Mix(i == 0 ? P::Accent() : P::Toggle(), err_col, err_t);
            if (loading) col = A(P::Accent(), load_alpha[i]);
            else if (success) col = A(i == 0 ? P::Accent() : Mix(P::Accent(), P::Toggle(), mo));
            else col = A(col);
            if (i == 0 && !loading && !success) {
                dl->PathArcTo(rrc, ring_r, a0, a1, 24);
                dl->PathStroke(A(Mix(P::Accent(), err_col, err_t), ALPHA_SEGMENT_HALO * (std::min)(1.0f, Theme::MenuGlow())), 0, ring_w * 1.8f);
            }
            dl->PathArcTo(rrc, ring_r, a0, a1, 24);
            dl->PathStroke(col, 0, ring_w);
        }
    }
    g_alpha = base * ph(0.0f, 0.8f); // ticks and centre disc fade in with the ring
    {
        const float t0 = Sc(44) + (70.0f * so - Sc(44)) * mo, t1 = Sc(50) + (77.0f * so - Sc(50)) * mo;
        const int sweep = static_cast<int>((now - load_t0) / (1.2 / 35.0)) % 35; // loading: 14 ticks light up and travel with the ring
        for (int k = 0; k < 35; ++k) { // tick ring r 44-50
            const float ang = (-90.0f + 360.0f * k / 35.0f) * DEG, ca = std::cos(ang), sa = std::sin(ang);
            const bool lit = success || (loading && ((k - sweep + 35) % 35) < 14);
            dl->AddLine(ImVec2(rrc.x + ca * t0, rrc.y + sa * t0), ImVec2(rrc.x + ca * t1, rrc.y + sa * t1),
                        lit ? A(Mix(P::Accent(), ColA(P::Muted(), 0.30f), mo)) : A(P::Muted(), 0.30f), Sc(2.5f));
        }
    }

    g_alpha = base * cf * ph(0.0f, 0.8f);
    // centre: avatar on success, otherwise disc with ghost (error icon on error)
    if (success) {
        const float t = ImSaturate(static_cast<float>(now - success_t0) / 0.3f);
        const float u = t - 1.0f, back = 1.0f + 2.70158f * u * u * u + 1.70158f * u * u; // ease-out back
        const float sc = 0.6f + 0.4f * back;
        const ImTextureID tex = Menu::Resources::UserTexture(); // also uploads the downloaded avatar
        const bool has_avatar = tex && Menu::Resources::HasUserAvatar(); // UserTexture() falls back to the default logo
        const float tex_t = Anim(ImGui::GetID("##avatar"), has_avatar ? 1.0f : 0.0f, 12.0f); // ~200 ms crossfade from the initial
        DrawGlowCircle(dl, rc, Sc(30) * sc, P::Accent(), g_alpha * 0.5f, 12);
        dl->AddCircleFilled(rc, Sc(35) * sc, A(P::Accent()), 48);
        dl->AddCircleFilled(rc, Sc(33) * sc, A(P::Card()), 48);
        dl->AddCircleFilled(rc, Sc(30) * sc, A(P::Accent(), 1.0f - tex_t), 48);
        // initial only for an ASCII first character; otherwise the plain accent circle
        if (tex_t < 0.99f && !v.last_logged_user->empty() && static_cast<unsigned char>((*v.last_logged_user)[0]) < 0x80) {
            const char init[2] = {static_cast<char>(std::toupper(static_cast<unsigned char>((*v.last_logged_user)[0]))), 0};
            ImFont* nf = TextFont(26, true);
            TextC(dl, nf, 26, rc.x - TextSize(nf, 26, init).x * 0.5f, rc.y, A(Theme::Current().onAccent, 1.0f - tex_t), init);
        }
        if (has_avatar) {
            const float r = Sc(30) * sc;
            dl->AddImageRounded(tex, ImVec2(rc.x - r, rc.y - r), ImVec2(rc.x + r, rc.y + r), ImVec2(0, 0), ImVec2(1, 1), A(IM_COL32_WHITE, tex_t), r);
        }
    } else {
        dl->AddCircleFilled(rc, Sc(34), A(P::Card()), 48);
        dl->AddCircle(rc, Sc(34), A(P::Border()), 48, 1.0f);
        if (error) {
            IconC(dl, 24, rc.x, rc.y, A(err_col), err.icon);
            if (err.wifi_off) dl->AddLine(ImVec2(rc.x - Sc(11), rc.y - Sc(11)), ImVec2(rc.x + Sc(11), rc.y + Sc(11)), A(err_col), Sc(2.5f));
        } else {
            IconC(dl, 24, rc.x, rc.y, A(P::Accent()), ICON_FA_GHOST);
        }
    }

    // title + subline / error pill (intro: fade in and rise 6 px)
    const float tp = ph(0.6f, 1.0f), ty = Sc(6) * (1.0f - tp);
    g_alpha = base * cf * tp;
    {
        const std::string title = loading ? "Verifying" : success ? *v.last_logged_user : "Sign in";
        bool trunc = false;
        ImFont* tf = TextFont(26, true);
        const std::string tt = FitText(tf, 26, Sc(FW), title, trunc);
        TextC(dl, tf, 26, wp.x + (w - TextSize(tf, 26, tt.c_str()).x) * 0.5f, wp.y + Sc(214) + ty, A(P::Text()), tt.c_str());
        if (error) {
            ImFont* pf = TextFont(12, true);
            ImFont* code_font = MonoFont(9);
            const std::string msg = FitText(pf, 12, Sc(FW - 60), err.text, trunc);
            const float tw = TextSize(pf, 12, msg.c_str()).x, cw = err.code.empty() ? 0.0f : TextSize(code_font, 9, err.code.c_str()).x + Sc(8);
            const float pw = Sc(10 + 12 + 6) + tw + cw + Sc(10), pill_h = Sc(26);
            const ImVec2 a(wp.x + (w - pw) * 0.5f, wp.y + Sc(234) + ty), b(a.x + pw, a.y + pill_h);
            dl->AddRectFilled(a, b, A(err_col, 0.10f), Sc(8));
            dl->AddRect(a, b, A(err_col, 0.40f), Sc(8), 0, 1.0f);
            const float cy = (a.y + b.y) * 0.5f;
            IconC(dl, 12, a.x + Sc(10 + 6), cy, A(err_col), err.icon);
            TextC(dl, pf, 12, a.x + Sc(10 + 12 + 6), cy, A(err_col), msg.c_str());
            if (cw > 0.0f) TextC(dl, code_font, 9, a.x + Sc(10 + 12 + 6) + tw + Sc(8), cy, A(P::Muted()), err.code.c_str());
        } else {
            ImFont* sf = MonoFont(10);
            const char* sub = loading ? "CHECKING LICENSE \xC2\xB7 HWID" : success ? "WELCOME BACK" : "AUTHENTICATE TO OPEN ORBIT";
            SpacedText(dl, sf, 10, ImVec2(wp.x + (w - SpacedWidth(sf, 10, sub, 1.5f)) * 0.5f, wp.y + Sc(247) + ty - Fz(10) * 0.5f), A(P::Muted()), sub, 1.5f);
        }
    }

    g_alpha = base * cf;
    if (success) {
        // license card (days / expiry were stored before login_success_pending was set)
        const ImVec2 a = at(FX, 282), b = at(FX + FW, 282 + 156);
        dl->AddRectFilled(a, b, A(P::Card()), Sc(12));
        dl->AddRect(a, b, A(P::Border()), Sc(12), 0, 1.0f);
        const bool permanent = *v.license_days == "PERMANENT";
        const int days = permanent ? 0 : std::atoi(v.license_days->c_str());
        const float px = a.x + Sc(16), rx = b.x - Sc(16);
        ImFont* m9 = MonoFont(9);
        ImFont* m9b = MonoFont(9, true);
        const float r1 = a.y + Sc(16 + 9);
        IconC(dl, 12, px + Sc(6), r1, A(P::Accent()), ICON_FA_KEY);
        SpacedText(dl, m9, 9, ImVec2(px + Sc(20), r1 - Fz(9) * 0.5f), A(P::Muted()), "LICENSE", 1.5f);
        const float chw = TextSize(m9b, 9, "ACTIVE").x + Sc(16);
        dl->AddRectFilled(ImVec2(rx - chw, r1 - Sc(9)), ImVec2(rx, r1 + Sc(9)), A(P::Success(), 0.14f), Sc(9));
        TextC(dl, m9b, 9, rx - chw + Sc(8), r1, A(P::Success()), "ACTIVE");

        const float r2 = a.y + Sc(70);
        ImFont* nf = MonoFont(30, true);
        char num[16];
        std::snprintf(num, sizeof(num), "%d", days);
        const char* big = permanent ? "PERMANENT" : num;
        TextC(dl, nf, 30, px, r2, A(P::Accent()), big);
        const float nw = TextSize(nf, 30, big).x;
        if (!permanent) {
            ImFont* df = TextFont(15, true);
            TextC(dl, df, 15, px + nw + Sc(8), r2 + Sc(5), A(P::Label()), "days left");
            if (!v.license_expiry->empty() && *v.license_expiry != "PERMANENT") {
                const std::string ex = "EXPIRES " + *v.license_expiry;
                ImFont* ef = MonoFont(10);
                TextC(dl, ef, 10, rx - TextSize(ef, 10, ex.c_str()).x, r2 + Sc(5), A(P::Muted()), ex.c_str());
            }
        }

        const float cw = (rx - px - Sc(3) * 19.0f) / 20.0f, cy0 = a.y + Sc(96), chh = Sc(10);
        const int filled = permanent ? 20 : static_cast<int>(std::ceil(ImSaturate((std::min)(days, 30) / 30.0f) * 20.0f));
        for (int i = 0; i < 20; ++i) {
            const float x = px + i * (cw + Sc(3));
            dl->AddRectFilled(ImVec2(x, cy0), ImVec2(x + cw, cy0 + chh), A(i < filled ? P::Accent() : P::Toggle()), Sc(2));
        }
        const float r4 = b.y - Sc(16 + 6);
        dl->AddLine(ImVec2(px, r4 - Sc(18)), ImVec2(rx, r4 - Sc(18)), A(P::Border()));
        TextC(dl, m9, 9, px, r4, A(P::Muted()), "HWID BOUND");
        if (Menu::Resources::UserTexture() && Menu::Resources::HasUserAvatar())
            TextC(dl, m9, 9, rx - TextSize(m9, 9, "AVATAR SYNCED").x, r4, A(P::Muted()), "AVATAR SYNCED");
    } else {
        // input fields: own rect + border, transparent ImGui frame on top
        const float fa = loading ? 0.5f : 1.0f;
        const float saved_alpha = base * ph(0.8f, 1.3f); // intro: fields fade in at 800-1300 ms
        g_alpha = saved_alpha * fa;
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, saved_alpha * fa);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(P::Text(), 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ColV(P::Muted(), 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ColV(P::Soft(), 1.0f));
        bool edited = false;
        auto field = [&](const char* label, float label_y, float fy, const char* icon, const char* id, const char* hint,
                         char* buf, size_t size, ImGuiInputTextFlags flags, ImFont* font, float right_pad, bool field_error, bool refocus = false) {
            const ImU32 lab = field_error ? err_col : P::Muted();
            ImFont* lf = MonoFont(9);
            SpacedText(dl, lf, 9, ImVec2(wp.x + Sc(FX), wp.y + Sc(label_y + 6) - Fz(9) * 0.5f), A(lab), label, 1.5f);
            const ImVec2 a = at(FX, fy), b = at(FX + FW, fy + FH);
            dl->AddRectFilled(a, b, A(P::Card()), Sc(8));
            if (loading) flags |= ImGuiInputTextFlags_ReadOnly;
            ImGui::PushFont(font);
            ImGui::SetWindowFontScale(Fz(14) / font->FontSize); // Fz already includes g_fs
            ImGui::SetCursorScreenPos(a);
            ImGui::SetNextItemWidth(Sc(FW - right_pad));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(Sc(38), (std::max)(0.0f, (Sc(FH) - ImGui::GetFontSize()) * 0.5f)));
            static ImGuiID refocus_id = 0; // field whose focus-activation select-all must be undone once
            if (refocus) { ImGui::SetKeyboardFocusHere(); refocus_id = ImGui::GetID(id); } // after the eye click: keep typing
            const bool enter = ImGui::InputTextWithHint(id, hint, buf, size, flags);
            if (refocus_id != 0 && ImGui::GetItemID() == refocus_id && ImGui::IsItemActive()) {
                if (ImGuiInputTextState* st = ImGui::GetInputTextState(refocus_id)) { // cursor to the end, nothing selected
                    st->Stb.cursor = st->CurLenW;
                    st->ClearSelection();
                }
                refocus_id = 0;
            }
            if (ImGui::IsItemEdited()) edited = true;
            ImGui::PopStyleVar();
            ImGui::SetWindowFontScale(g_fs);
            ImGui::PopFont();
            const float f = Anim(ImGui::GetID(id), ImGui::IsItemActive() ? 1.0f : 0.0f, 25.0f);
            const ImU32 edge = field_error ? err_col : Mix(P::Border(), P::Accent(), f);
            if (f > 0.01f) dl->AddRect(ImVec2(a.x - Sc(1.5f), a.y - Sc(1.5f)), ImVec2(b.x + Sc(1.5f), b.y + Sc(1.5f)), A(field_error ? err_col : P::Accent(), 0.14f * f), Sc(9.5f), 0, Sc(3));
            dl->AddRect(a, b, A(edge), Sc(8), 0, Sc(1.0f + 0.5f * f));
            IconC(dl, 14, a.x + Sc(12 + 7), (a.y + b.y) * 0.5f, A(field_error ? err_col : Mix(P::Muted(), P::Accent(), f)), icon);
            return enter;
        };
        const bool show_err = error && !field_err_cleared;
        static bool focus_pw = false; // set by the eye button, consumed by the next password field
        field("USERNAME", 282, 300, ICON_FA_USER, "##orbit_user", "Enter username", v.username, v.username_size,
              ImGuiInputTextFlags_CharsNoBlank, TextFont(14), 0.0f, show_err && err.field == 1);
        ImGuiInputTextFlags pw_flags = ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_EnterReturnsTrue;
        if (!*v.show_password) pw_flags |= ImGuiInputTextFlags_Password;
        if (field("PASSWORD", 352, 370, ICON_FA_LOCK, "##orbit_pass", "Enter password", v.password, v.password_size, pw_flags,
                  MonoFont(14, true), 44.0f, show_err && err.field == 2, focus_pw) && !loading)
            submit = true;
        focus_pw = false;
        if (edited) field_err_cleared = true;
        ImGui::PopStyleColor(6);
        ImGui::PopStyleVar(2);

        // eye button inside the password field (toggled state looks like hover)
        {
            const ImVec2 a = at(FX + FW - 6 - 32, 370 + (FH - 32) * 0.5f);
            ImGui::SetCursorScreenPos(a);
            if (ImGui::InvisibleButton("##orbit_eye", ImVec2(Sc(32), Sc(32))) && !loading) { *v.show_password = !*v.show_password; focus_pw = true; }
            const float hv = Anim(ImGui::GetItemID(), (ImGui::IsItemHovered() || ImGui::IsItemActive() || *v.show_password) ? 1.0f : 0.0f, 25.0f);
            dl->AddRectFilled(a, ImVec2(a.x + Sc(32), a.y + Sc(32)), A(P::Soft(), hv), Sc(6));
            IconC(dl, 16, a.x + Sc(16), a.y + Sc(16), A(Mix(P::Muted(), P::Accent(), hv)), *v.show_password ? ICON_FA_EYE_SLASH : ICON_FA_EYE);
        }
        g_alpha = saved_alpha;

        if (!loading && (GetKeyState(VK_CAPITAL) & 1)) {
            ImFont* caps_font = MonoFont(9, true);
            TextC(dl, caps_font, 9, wp.x + Sc(FX + FW) - TextSize(caps_font, 9, "CAPS ON").x, wp.y + Sc(433), A(P::Accent()), "CAPS ON");
        }
    }

    // bottom: progress box while loading / opening, otherwise the submit button (intro: 1100-1600 ms; also drives the footer)
    g_alpha = base * cf * ph(1.1f, 1.6f);
    {
        const ImVec2 a = at(FX, 456), b = at(FX + FW, 456 + 46);
        ImGui::SetCursorScreenPos(a);
        if (loading || success) {
            ImGui::Dummy(ImVec2(Sc(FW), Sc(46)));
            const ImU32 col = success ? P::Success() : P::Accent();
            dl->AddRectFilled(a, b, A(P::Card()), Sc(10));
            dl->AddRect(a, b, A(P::Border()), Sc(10), 0, 1.0f);
            ImFont* lf = MonoFont(9, true);
            const char* label = success ? (orbit_next ? "OPENING ORBIT" : "OPENING MENU") : "VERIFYING";
            char right[16] = "WAIT";
            if (success) std::snprintf(right, sizeof(right), "%.1f s", tsince);
            const float px = a.x + Sc(14), rx = b.x - Sc(14);
            TextC(dl, lf, 9, px, a.y + Sc(9 + 5), A(col), label);
            TextC(dl, lf, 9, rx - TextSize(lf, 9, right).x, a.y + Sc(9 + 5), A(P::Muted()), right);
            const float cw = (rx - px - Sc(3) * 19.0f) / 20.0f, cy1 = b.y - Sc(9), cy0 = cy1 - Sc(8);
            const int head = static_cast<int>(now / 0.06) % 24; // 5-cell window, moves 60 ms per cell, slides out and wraps
            const int filled = success ? static_cast<int>((total > 0.0f ? ImSaturate(tsince / total) : 1.0f) * 20.0f) : 0;
            for (int i = 0; i < 20; ++i) {
                const float x = px + i * (cw + Sc(3));
                const bool is_head = loading && i == head, in_win = loading && i <= head && i > head - 5;
                ImU32 c = P::Toggle();
                if (success && i < filled) c = col;
                else if (is_head) c = Theme::Current().hover;
                else if (in_win) c = col;
                if (is_head) GlowRect(dl, ImVec2(x, cy0), ImVec2(x + cw, cy1), Sc(2), 6.0f, Theme::Current().hover, 1.0f);
                dl->AddRectFilled(ImVec2(x, cy0), ImVec2(x + cw, cy1), A(c), Sc(2));
            }
        } else if (!*v.intro_done) {
            ImGui::Dummy(ImVec2(Sc(FW), Sc(46)));
            ImFont* bf = TextFont(14, true);
            dl->AddRectFilled(a, b, A(P::Card()), Sc(10));
            dl->AddRect(a, b, A(P::Border()), Sc(10), 0, 1.0f);
            TextC(dl, bf, 14, (a.x + b.x) * 0.5f - TextSize(bf, 14, "Sign in").x * 0.5f, (a.y + b.y) * 0.5f, A(P::Muted()), "Sign in");
        } else {
            ImFont* bf = TextFont(14, true);
            if (ImGui::InvisibleButton("##orbit_submit", ImVec2(Sc(FW), Sc(46)))) submit = true;
            const float hv = Anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 25.0f);
            GlowRect(dl, a, b, Sc(10), 18.0f, P::Accent(), 0.35f + 0.65f * hv);
            dl->AddRectFilled(a, b, A(Mix(P::Accent(), Theme::Current().hover, hv)), Sc(10));
            const ImU32 on = A(Theme::Current().onAccent);
            const char* label = err.retry ? "Retry" : "Sign in";
            const float tw = TextSize(bf, 14, label).x, gap = Sc(8), iw = Fz(16), x0 = (a.x + b.x) * 0.5f - (tw + gap + iw) * 0.5f, cy = (a.y + b.y) * 0.5f;
            TextC(dl, bf, 14, x0, cy, on, label);
            IconC(dl, 16, x0 + tw + gap + iw * 0.5f, cy, on, err.retry ? ICON_FA_REDO_ALT : ICON_FA_SIGN_IN_ALT);
        }
    }

    // footer (the return-arrow glyph is not in the Latin-1 text atlas: plain ASCII)
    {
        ImFont* ff = MonoFont(9);
        TextC(dl, ff, 9, wp.x + Sc(FX), wp.y + Sc(522), A(P::Muted()), "ENTER = SUBMIT");
        TextC(dl, ff, 9, wp.x + Sc(FX + FW) - TextSize(ff, 9, "END = UNLOAD").x, wp.y + Sc(522), A(P::Muted()), "END = UNLOAD");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
    if (submit && !loading && !success && *v.intro_done) { // a fast failing retry must shake and restyle again
        was_error = false;
        seen_raw.clear();
        field_err_cleared = false;
    }
    return submit;
}

} // namespace Orbit
