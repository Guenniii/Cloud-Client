#include "clickgui.hpp"
#include "module_view.hpp"
#include "history.hpp"
#include "key_names.hpp"
#include "menu.hpp"
#include "module_registry.hpp"
#include "palette.hpp"
#include "pages.hpp"
#include "resources.hpp"
#include "widgets.hpp"
#include "../utils/config/config.hpp"
#include "../utils/theme/theme.hpp"
#include "../dependencies/font/IconsFontAwesome5.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace MV = Menu::ModuleView;
namespace P = Menu::Palette;
using namespace Menu::Widgets;

namespace {

constexpr const char* WINDOW_ID = "###client_menu_v2"; // own ID: dragging/position independent of the classic menu

float g_s = 1.0f;      // menu scale for this frame
float g_fs = 1.0f;     // font size factor (font_size_percent): scales drawn text only, not layout
float g_alpha = 1.0f;  // alpha applied by A() and the text/icon helpers

float Sc(float v) { return v * g_s; }
float Fz(float size) { return Sc(size) * g_fs; } // pixel size of drawn text/icons
ImU32 A(ImU32 c, float extra = 1.0f) { return ColA(c, g_alpha * extra); }

ImU32 Mix(ImU32 a, ImU32 b, float t) {
    const ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32(ImLerp(x, y, ImSaturate(t)));
}

// v += (target - v) * saturate(dt * k), state keyed by ImGui ID.
float Anim(ImGuiID id, float target, float k) {
    static std::unordered_map<ImGuiID, float> values;
    float& v = values[id];
    v += (target - v) * ImSaturate(ImGui::GetIO().DeltaTime * k);
    return v;
}

// Fonts: no new atlas sizes; one weight. <=16 px uses the Poppins 16 atlas, larger sizes the Poppins 25 atlas.
ImFont* TextFont(float size) { return size <= 16.0f ? ImGui::GetIO().Fonts->Fonts[0] : poppins; }
ImVec2 TextSize(float size, const char* t) { return TextFont(size)->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, t); }
// cy = vertical centre of the line
void TextC(ImDrawList* dl, float size, float x, float cy, ImU32 col, const char* t) {
    dl->AddText(TextFont(size), Fz(size), ImVec2(x, cy - Fz(size) * 0.5f), col, t);
}
void IconC(ImDrawList* dl, float size, float cx, float cy, ImU32 col, const char* icon) {
    const ImVec2 sz = icons->CalcTextSizeA(Fz(size), FLT_MAX, 0.0f, icon);
    dl->AddText(icons, Fz(size), ImVec2(cx - sz.x * 0.5f, cy - Fz(size) * 0.5f), col, icon);
}
// Section label: uppercase, letter spacing +1.2
void SpacedText(ImDrawList* dl, float size, ImVec2 pos, ImU32 col, const char* t) {
    for (; *t; ++t) {
        const char ch[2] = {*t, 0};
        dl->AddText(TextFont(size), Fz(size), pos, col, ch);
        pos.x += TextSize(size, ch).x + Sc(1.2f);
    }
}

// ---------- settings rows ----------

struct Place { int col; float y, h; };
struct Plan {
    std::vector<Place> items;
    float col_w = 0, desc_h = 0, cols = 0, total = 0;
};

float RowH(const ModuleSetting& s) { return Sc(s.type == SettingType::SliderInt ? 44.0f : 28.0f); }
float RowGap(const ModuleSetting& s) { return Sc(s.type == SettingType::SliderInt ? 10.0f : 8.0f); }

// Card layout below the header; total = full expanded extra height (divider included).
Plan MakePlan(const ModuleData& m, float card_w, bool narrow, bool oresim) {
    Plan plan;
    const float inner = card_w - Sc(32);
    plan.col_w = narrow ? inner : (inner - Sc(24)) * 0.5f;
    plan.desc_h = TextFont(12)->CalcTextSizeA(Fz(12), FLT_MAX, inner, Menu::Registry::Description(m.name)).y;
    int left = 0, right = 0;
    for (const auto& s : m.settings_list) (s.type == SettingType::SliderInt ? right : left)++;
    const bool alternate = !narrow && (left == 0 || right == 0); // single type: spread over both columns
    float y[2] = {0, 0}, last_gap[2] = {0, 0};
    int idx = 0;
    for (const auto& s : m.settings_list) {
        const int c = narrow ? 0 : alternate ? (idx % 2) : (s.type == SettingType::SliderInt ? 1 : 0);
        ++idx;
        plan.items.push_back({c, y[c], RowH(s)});
        y[c] += RowH(s) + RowGap(s);
        last_gap[c] = RowGap(s);
    }
    plan.cols = (std::max)(y[0] - (y[0] > 0 ? last_gap[0] : 0), y[1] - (y[1] > 0 ? last_gap[1] : 0));
    plan.total = Sc(1 + 12) + plan.desc_h + Sc(12) + (oresim ? MV::ORESIM_SEED_ROW_HEIGHT + Sc(12) : 0.0f)
               + (plan.cols > 0 ? plan.cols + Sc(12) : 0.0f) + Sc(24 + 14);
    return plan;
}

void CheckboxRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.b_val) return;
    const float h = Sc(28), tw = Sc(36);
    // Label area is the click target; the toggle handles its own clicks (no overlapping items).
    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton("##row", ImVec2(w - tw, h))) s.b_val->store(!s.b_val->load());
    TextC(dl, 13, p.x, p.y + h * 0.5f, A(P::Label()), s.DisplayName());
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - tw, p.y + (h - Sc(20)) * 0.5f));
    CustomToggle("##t", s.b_val, g_alpha, tw, Sc(20));
}

void ColorRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.i_val) return;
    const float h = Sc(28);
    TextC(dl, 13, p.x, p.y + h * 0.5f, A(P::Label()), s.DisplayName());
    const int rgb = s.i_val->load();
    const ImVec2 at(p.x + w - Sc(112), p.y + Sc(1)), end(at.x + Sc(112), at.y + Sc(26));
    ImGui::SetCursorScreenPos(at);
    const bool opened = ImGui::InvisibleButton("##swatch", ImVec2(Sc(112), Sc(26)));
    const float hv = Anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 16);
    dl->AddRectFilled(at, end, A(P::Background()), Sc(6));
    dl->AddRect(at, end, A(Mix(P::Border(), P::Accent(), 0.6f * hv)), Sc(6));
    dl->AddRectFilled(ImVec2(at.x + Sc(7), at.y + Sc(5)), ImVec2(at.x + Sc(25), at.y + Sc(21)),
                      A(IM_COL32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255)), Sc(4));
    char hex[12];
    std::snprintf(hex, sizeof(hex), "#%06X", rgb & 0xFFFFFF);
    TextC(dl, 12, at.x + Sc(33), at.y + Sc(13), A(P::Text()), hex);
    DrawCustomColorPopup(s.DisplayName(), s.i_val, at, opened, g_alpha);
}

void SliderRow(ImDrawList* dl, ImVec2 p, float w, ModuleSetting& s) {
    if (!s.i_val) return;
    int v = s.i_val->load();
    const int before = v;
    char val[32];
    std::snprintf(val, sizeof(val), "%d%s", v, s.suffix ? s.suffix : "");
    TextC(dl, 13, p.x, p.y + Sc(10), A(P::Label()), s.DisplayName());
    TextC(dl, 13, p.x + w - TextSize(13, val).x, p.y + Sc(10), A(P::Accent()), val);

    // Invisible SliderInt for interaction; Ctrl+click shows the text entry in normal colors while active.
    const ImGuiID sid = ImGui::GetID("##slider");
    const bool typing = ImGui::TempInputIsActive(sid);
    const float pad_y = (std::max)(0.0f, (Sc(12) - ImGui::GetFontSize()) * 0.5f);
    const float fh = ImGui::GetFontSize() + pad_y * 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + Sc(32) - fh * 0.5f)); // track centre = 6 px below the 26 px mark
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
    ImGui::SliderInt("##slider", &v, s.i_min, s.i_max, "", ImGuiSliderFlags_AlwaysClamp); // Ctrl+click entry must stay in range
    const ImVec2 rmin = ImGui::GetItemRectMin(), rmax = ImGui::GetItemRectMax();
    const bool active = ImGui::IsItemHovered() || ImGui::IsItemActive();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(6);
    if (v != before) s.i_val->store(v);

    const float hv = Anim(sid, active ? 1.0f : 0.0f, 18);
    const float cy = (rmin.y + rmax.y) * 0.5f, th = Sc(4);
    const float t = s.i_max > s.i_min ? ImSaturate(static_cast<float>(v - s.i_min) / static_cast<float>(s.i_max - s.i_min)) : 0.0f;
    const float hx = rmin.x + t * (rmax.x - rmin.x);
    if (hx > rmin.x + 2.0f)
        DrawGlowRect(dl, ImVec2(rmin.x, cy - th * 0.5f), ImVec2(hx, cy + th * 0.5f), th * 0.5f, P::Accent(), g_alpha * 0.35f, 12);
    dl->AddRectFilled(ImVec2(rmin.x, cy - th * 0.5f), ImVec2(rmax.x, cy + th * 0.5f), A(P::Toggle()), th * 0.5f);
    dl->AddRectFilled(ImVec2(rmin.x, cy - th * 0.5f), ImVec2(hx, cy + th * 0.5f), A(P::Accent()), th * 0.5f);
    const float r = Sc(5.5f + hv);
    DrawGlowCircle(dl, ImVec2(hx, cy), r, P::Accent(), g_alpha * (0.85f + 0.15f * hv), 12);
    dl->AddCircleFilled(ImVec2(hx, cy), r, A(Mix(P::Accent(), Theme::Current().hover, hv)), 32);
}

// ---------- module card ----------

std::unordered_map<int, bool> g_expanded;
std::unordered_map<int, float> g_anim;

void DrawCard(ImDrawList* dl, std::span<ModuleData> modules, int i, const MV::Filter& filter, bool narrow, bool search_changed) {
    ModuleData& m = modules[i];
    ImGui::PushID(i);
    const ImVec2 cp = ImGui::GetCursorScreenPos();
    const float cw = ImGui::GetContentRegionAvail().x;
    const float head_h = Sc(60);
    bool& open = g_expanded[i];
    float& anim = g_anim[i];
    if (search_changed && filter.search[0] && !filter.Matches(m.name) && filter.MatchesModule(m)) open = true;

    const bool oresim = m.enabled == &OreSim_Enabled;
    Plan plan;
    if (open || anim > 0.5f) plan = MakePlan(m, cw, narrow, oresim);
    anim += ((open ? plan.total : 0.0f) - anim) * ImSaturate(ImGui::GetIO().DeltaTime * 10.0f);
    if (!open && anim < 0.5f) anim = 0.0f;
    const ImVec2 cmax(cp.x + cw, cp.y + head_h + anim);

    // Glow is expensive: cards outside the visible area only advance their animation.
    // Never cull an open card while an item is active or a popup (color picker) is open, or it would lose the interaction.
    const bool keep = open && (ImGui::IsAnyItemActive() || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId));
    if (keep || ImGui::IsRectVisible(cp, cmax)) {
        const float cy = cp.y + head_h * 0.5f;
        const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(cp, ImVec2(cmax.x, cp.y + head_h));
        const float hv = Anim(ImGui::GetID("##hover"), hovered ? 1.0f : 0.0f, 16);
        const float ex = plan.total > 0 ? ImSaturate(anim / plan.total) : 0.0f;
        const bool on = m.enabled->load();

        ImU32 border = Mix(P::Border(), P::Accent(), 0.45f * hv);
        border = Mix(border, P::Accent(), 0.35f * ex);
        dl->AddRectFilled(cp, cmax, A(Mix(P::Card(), P::Soft(), 0.35f * hv)), Sc(12));
        dl->AddRect(cp, cmax, A(border), Sc(12), 0, 1.0f);
        if (on) {
            const ImVec2 imin(cp.x, cp.y + Sc(16)), imax(cp.x + Sc(3), cp.y + Sc(44));
            DrawGlowRect(dl, imin, imax, Sc(2), P::Accent(), g_alpha, 12);
            dl->AddRectFilled(imin, imax, A(P::Accent()), Sc(2));
        }

        // geometry
        const float icon_x = cp.x + Sc(16);
        const float text_x = icon_x + Sc(32) + Sc(12);
        const float tog_x = cp.x + cw - Sc(16) - Sc(36);
        const float star_x = tog_x - Sc(12) - Sc(28);
        const float line1 = cy - Sc(9.5f), line2 = cy + Sc(10.5f);

        dl->AddRectFilled(ImVec2(icon_x, cy - Sc(16)), ImVec2(icon_x + Sc(32), cy + Sc(16)), A(P::Soft()), Sc(9));
        IconC(dl, 15, icon_x + Sc(16), cy, A(on ? P::Accent() : P::Label()), MV::ModuleIcon(m));

        // hotkey badge
        const bool capturing = Config::capturing_module_key == i;
        const bool has_key = m.hotkey != 0;
        const std::string key_label = capturing ? "..." : has_key ? Menu::GetKeyName(m.hotkey) : "Set Key";
        const float badge_w = TextSize(11, key_label.c_str()).x + Sc(14), badge_h = Sc(20);
        // badge follows the name but never reaches the star; a long name is clipped instead
        const ImVec2 bmin((std::min)(text_x + TextSize(14, m.name).x + Sc(10), star_x - Sc(8) - badge_w), line1 - badge_h * 0.5f);
        ImGui::PushClipRect(ImVec2(text_x, cp.y), ImVec2((std::max)(text_x, bmin.x - Sc(6)), cmax.y), true);
        TextC(dl, 14, text_x, line1, A(P::Text()), m.name);
        ImGui::PopClipRect();
        const ImVec2 bmax(bmin.x + badge_w, bmin.y + badge_h);
        const bool conflict = Menu::Registry::HasConflict(m.hotkey, i);
        const float conflict_w = conflict ? Sc(8) + TextSize(11, "Key conflict").x : 0.0f;

        // header = expand/collapse; partitioned around badge, star and toggle so no hit areas overlap
        auto hit = [&](const char* id, float x0, float y0, float x1, float y1) {
            if (x1 - x0 < 1.0f || y1 - y0 < 1.0f) return;
            ImGui::SetCursorScreenPos(ImVec2(x0, y0));
            if (ImGui::InvisibleButton(id, ImVec2(x1 - x0, y1 - y0))) open = !open;
        };
        const float top = cp.y, bot = cp.y + head_h, bx = (std::min)(bmin.x, star_x);
        hit("##h_l", cp.x, top, bx, bot);
        hit("##h_t", bx, top, star_x, bmin.y);
        hit("##h_b", bx, bmax.y, star_x, bot);
        hit("##h_r", bmax.x, bmin.y, star_x, bmax.y);
        hit("##h_gap", star_x + Sc(28), top, tog_x, bot);
        hit("##h_end", tog_x + Sc(36), top, cmax.x, bot);
        hit("##h_tt", tog_x, top, tog_x + Sc(36), cy - Sc(10));
        hit("##h_tb", tog_x, cy + Sc(10), tog_x + Sc(36), bot);

        ImGui::SetCursorScreenPos(bmin);
        if (ImGui::InvisibleButton("##hotkey", ImVec2(badge_w, badge_h))) {
            Config::capturing_menu_key = false;
            Config::capturing_module_key = capturing ? -1 : i;
        }
        const bool badge_hover = ImGui::IsItemHovered();
        if (badge_hover && ImGui::IsMouseClicked(1)) { m.hotkey = 0; if (capturing) Config::capturing_module_key = -1; }
        const float bh = Anim(ImGui::GetItemID(), badge_hover ? 1.0f : 0.0f, 16);
        dl->AddRectFilled(bmin, bmax, A(capturing ? P::Soft() : Mix(P::Background(), P::Soft(), bh)), Sc(5));
        if (capturing || conflict) {
            const float pulse = capturing ? 0.75f + 0.25f * std::sin(static_cast<float>(ImGui::GetTime()) * 6.2831853f / 0.8f) : 1.0f;
            dl->AddRect(bmin, bmax, A(P::Accent(), pulse), Sc(5), 0, 1.5f);
        } else if (has_key || bh > 0.01f) {
            dl->AddRect(bmin, bmax, A(P::Border(), has_key ? 1.0f : bh), Sc(5), 0, 1.0f);
        }
        TextC(dl, 11, bmin.x + Sc(7), bmin.y + badge_h * 0.5f,
              A(capturing ? P::Accent() : has_key ? P::Text() : P::Muted()), key_label.c_str());
        if (conflict) {
            if (bmax.x + conflict_w <= star_x) TextC(dl, 11, bmax.x + Sc(8), bmin.y + badge_h * 0.5f, A(P::Accent()), "Key conflict");
            if (ImGui::IsMouseHoveringRect(bmin, ImVec2(bmax.x + conflict_w, bmax.y))) MV::DrawConflictTooltip(modules, i);
        }

        // meta line: "N settings", chevron, "N active"
        char sub[32];
        std::snprintf(sub, sizeof(sub), "%d settings", static_cast<int>(m.settings_list.size()));
        TextC(dl, 12, text_x, line2, A(P::Muted()), sub);
        float mx = text_x + TextSize(12, sub).x + Sc(6);
        const char* chevron = open ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
        dl->AddText(icons, Fz(8), ImVec2(mx, line2 - Fz(4)), A(P::Muted()), chevron);
        mx += Sc(8) + Sc(8);
        if (m.active_settings > 0) {
            dl->AddCircleFilled(ImVec2(mx + Sc(2), line2), Sc(2), A(P::Muted()));
            char act[32];
            std::snprintf(act, sizeof(act), "%d active", m.active_settings);
            TextC(dl, 12, mx + Sc(10), line2, A(P::Accent()), act);
        }

        // favorite star
        ImGui::SetCursorScreenPos(ImVec2(star_x, cy - Sc(16)));
        if (ImGui::InvisibleButton("##favorite", ImVec2(Sc(28), Sc(32)))) m.favorite = !m.favorite;
        const float sh = Anim(ImGui::GetItemID(), ImGui::IsItemHovered() ? 1.0f : 0.0f, 16);
        IconC(dl, 14, star_x + Sc(14), cy, A(m.favorite ? P::Accent() : Mix(P::Muted(), P::Label(), sh)), ICON_FA_STAR);

        ImGui::SetCursorScreenPos(ImVec2(tog_x, cy - Sc(10)));
        CustomToggle("##enabled", m.enabled, g_alpha, Sc(36), Sc(20));

        // expanded body
        if (anim > 1.0f && ex >= 0.4f && plan.items.size() == m.settings_list.size()) { // controls only while visible
            const float saved = g_alpha;
            g_alpha *= ImSaturate((ex - 0.4f) / 0.6f); // body appears once 40 % of the height is reached
            dl->AddLine(ImVec2(cp.x, cp.y + head_h), ImVec2(cmax.x, cp.y + head_h), A(P::Border()));
            ImGui::PushClipRect(cp, cmax, true);
            const float x0 = cp.x + Sc(16);
            float y = cp.y + head_h + Sc(1 + 12);
            dl->AddText(TextFont(12), Fz(12), ImVec2(x0, y), A(P::Muted()), Menu::Registry::Description(m.name), nullptr, cw - Sc(32));
            y += plan.desc_h + Sc(12);
            if (oresim) {
                // shortcut: seed field keeps its fixed pixel layout, not scaled; rescale when menu scaling matters here
                ImGui::SetCursorScreenPos(ImVec2(x0, y));
                MV::DrawOreSimSeedInput(dl, ImVec2(x0, y), plan.col_w, g_alpha);
                y += MV::ORESIM_SEED_ROW_HEIGHT + Sc(12);
            }
            for (size_t k = 0; k < m.settings_list.size(); ++k) {
                ModuleSetting& s = m.settings_list[k];
                const Place& pl = plan.items[k];
                const ImVec2 pos(x0 + pl.col * (plan.col_w + Sc(24)), y + pl.y);
                ImGui::PushID(s.DisplayName());
                switch (s.type) {
                    case SettingType::Checkbox: CheckboxRow(dl, pos, plan.col_w, s); break;
                    case SettingType::ColorRGB: ColorRow(dl, pos, plan.col_w, s); break;
                    case SettingType::SliderInt: SliderRow(dl, pos, plan.col_w, s); break;
                }
                if (ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + plan.col_w * 0.45f, pos.y + Sc(22)))) {
                    if (const char* help = MV::SettingHelp(s)) ImGui::SetTooltip("%s", help);
                }
                ImGui::PopID();
            }
            y += plan.cols + (plan.cols > 0 ? Sc(12) : 0.0f);

            // action row: hotkey mode segment (left), reset (right)
            const float ah = Sc(24);
            const bool mode_used = !m.action_request; // those use press actions, mode ignored
            if (mode_used) {
                const float w1 = TextSize(11, "Toggle").x + Sc(20), w2 = TextSize(11, "Hold").x + Sc(20);
                dl->AddRectFilled(ImVec2(x0, y), ImVec2(x0 + w1 + w2 + Sc(4), y + ah), A(P::Background()), Sc(8));
                dl->AddRect(ImVec2(x0, y), ImVec2(x0 + w1 + w2 + Sc(4), y + ah), A(P::Border()), Sc(8));
                const bool hold = m.hotkey_mode == Input::HotkeyMode::Hold;
                const char* names[2] = {"Toggle", "Hold"};
                const float widths[2] = {w1, w2};
                float sx = x0 + Sc(2);
                for (int mode = 0; mode < 2; ++mode) {
                    const bool active = (mode == 1) == hold;
                    ImGui::SetCursorScreenPos(ImVec2(sx, y + Sc(2)));
                    if (ImGui::InvisibleButton(names[mode], ImVec2(widths[mode], ah - Sc(4))))
                        m.hotkey_mode = mode == 1 ? Input::HotkeyMode::Hold : Input::HotkeyMode::Toggle;
                    if (active) dl->AddRectFilled(ImVec2(sx, y + Sc(2)), ImVec2(sx + widths[mode], y + ah - Sc(2)), A(P::Soft()), Sc(6));
                    TextC(dl, 11, sx + (widths[mode] - TextSize(11, names[mode]).x) * 0.5f, y + ah * 0.5f,
                          A(active ? P::Accent() : P::Muted()), names[mode]);
                    sx += widths[mode];
                }
                TextC(dl, 11, x0 + w1 + w2 + Sc(14), y + ah * 0.5f, A(P::Muted()), "Hotkey mode");
            }
            const float rw = Sc(10) + Sc(10) + Sc(6) + TextSize(11, "Reset module").x + Sc(10);
            const ImVec2 rmin(cmax.x - Sc(16) - rw, y);
            ImGui::SetCursorScreenPos(rmin);
            if (DrawRoundedButton(dl, "##reset", "", ImVec2(rw, ah), ColA(P::Soft(), 0.0f), P::Soft(), P::Label(), g_alpha, Sc(8)))
                MV::ResetModule(modules, i);
            dl->AddRect(rmin, ImVec2(rmin.x + rw, rmin.y + ah), A(P::Border()), Sc(8));
            IconC(dl, 10, rmin.x + Sc(10) + Sc(5), y + ah * 0.5f, A(P::Muted()), ICON_FA_UNDO);
            TextC(dl, 11, rmin.x + Sc(10) + Sc(10) + Sc(6), y + ah * 0.5f, A(P::Label()), "Reset module");
            ImGui::PopClipRect();
            g_alpha = saved;
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(cp.x, cmax.y + Sc(8)));
    ImGui::PopID();
}

} // namespace

namespace ClickGui {

void Draw() {
    using namespace MV;
    auto modules = Menu::Registry::GetModules();
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;
    g_s = Config::menu_size_percent.load() / 100.0f;
    const float fs = Config::font_size_percent.load() / 100.0f;
    g_fs = fs;
    const ImVec2 display = io.DisplaySize;
    const float w = (std::min)(900.0f * g_s, (std::max)(1.0f, display.x - 32.0f));
    const float h = (std::min)(600.0f * g_s, (std::max)(1.0f, display.y - 32.0f));
    const bool narrow = w < 760.0f;

    // persistent UI state
    static int active_page = PAGE_COMBAT, trans_from = -1, trans_to = -1;
    static float trans_t = -1.0f; // -1 = no transition
    static char search[96] = {};
    static std::string previous_search;
    static bool favorites_only = false;
    static float pill_rel = -1.0f;
    static float open_t = 0.0f;
    static int last_frame = -10;

    // The caller skips Draw while the menu is closed: a frame gap restarts the fade-in.
    if (ImGui::GetFrameCount() != last_frame + 1) open_t = 0.0f;
    last_frame = ImGui::GetFrameCount();
    open_t += (1.0f - open_t) * ImSaturate(dt * 14.0f);

    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImVec2 menu_pos((display.x - w) * 0.5f, (display.y - h) * 0.5f);
    if (const ImGuiWindow* previous = ImGui::FindWindowByName(WINDOW_ID))
        menu_pos = ImVec2(ImClamp(previous->Pos.x, 0.0f, (std::max)(0.0f, display.x - w)),
                          ImClamp(previous->Pos.y, 0.0f, (std::max)(0.0f, display.y - h)));
    ImGui::SetNextWindowPos(menu_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, open_t);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Sc(16));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, P::Background());
    ImGui::Begin(WINDOW_ID, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetWindowFontScale(fs);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const float oa = open_t;

    // soft window shadow (background list, so it is not clipped to the window)
    for (int layer = 1; layer <= 3; ++layer) {
        const float e = Sc(10.0f * layer);
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(wp.x - e, wp.y + Sc(16) - e), ImVec2(wp.x + w + e, wp.y + h + Sc(16) + e),
                                                      ColA(IM_COL32(0, 0, 0, 255), 0.14f * oa), Sc(16) + e);
    }
    if (Theme::gradient_enabled.load()) {
        const int first = dl->VtxBuffer.Size;
        const ImU32 top = ColA(Theme::Current().background, oa), bottom = ColA(Theme::GradientEnd(), oa);
        dl->AddRectFilled(wp, ImVec2(wp.x + w, wp.y + h), top, Sc(16));
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, first, dl->VtxBuffer.Size, wp, ImVec2(wp.x, wp.y + h), top, bottom);
    }

    // page transition (same crossfade as the classic menu)
    float page_alpha = 1.0f;
    int display_page = active_page;
    if (trans_t >= 0.0f) {
        trans_t += dt * 5.0f;
        if (trans_t < 0.5f) { display_page = trans_from; page_alpha = 1.0f - trans_t / 0.5f; }
        else if (trans_t < 1.0f) { display_page = trans_to; page_alpha = (trans_t - 0.5f) / 0.5f; active_page = trans_to; }
        else { trans_t = -1.0f; active_page = trans_to; display_page = active_page; }
    }
    const int selected_page = trans_t >= 0.0f ? trans_to : active_page;

    // ================= sidebar =================
    g_alpha = oa;
    const float sb_w = narrow ? Sc(64) : Sc(200);
    const float pad = narrow ? Sc(8) : Sc(16);
    const float tab_w = sb_w - 2.0f * pad, tab_h = Sc(36);
    dl->AddRectFilled(wp, ImVec2(wp.x + sb_w, wp.y + h), A(P::Card(), 0.6f), Sc(16), ImDrawFlags_RoundCornersLeft);

    float y = wp.y + Sc(20);
    {
        const ImVec2 mark(narrow ? wp.x + (sb_w - Sc(30)) * 0.5f : wp.x + pad, y);
        const ImVec2 mark_max(mark.x + Sc(30), mark.y + Sc(30));
        DrawGlowRect(dl, mark, mark_max, Sc(9), P::Accent(), g_alpha, 12);
        dl->AddRectFilled(mark, mark_max, A(P::Accent()), Sc(9));
        IconC(dl, 14, mark.x + Sc(15), mark.y + Sc(15), A(Theme::Current().onAccent), ICON_FA_GHOST);
        if (!narrow) {
            TextC(dl, 15, mark.x + Sc(40), mark.y + Sc(9), A(P::Text()), "PhantomUI");
            TextC(dl, 11, mark.x + Sc(40), mark.y + Sc(24), A(P::Muted()), "Minecraft 26.2");
        }
    }
    y += Sc(30 + 14);

    float tab_y[9];
    for (int c = 0; c < 9; ++c) {
        if (c == 0 || c == 4) {
            if (!narrow) {
                SpacedText(dl, 10, ImVec2(wp.x + pad + Sc(6), y + Sc(8)), A(P::Muted()), c == 0 ? "MODULE" : "CLIENT");
                y += Sc(8 + 15 + 4);
            } else if (c == 4) {
                dl->AddLine(ImVec2(wp.x + pad, y + Sc(6)), ImVec2(wp.x + pad + tab_w, y + Sc(6)), A(P::Border()));
                y += Sc(14);
            }
        }
        tab_y[c] = y;
        y += tab_h + Sc(4);
    }

    int cat_counts[4] = {};
    CountEnabledPerCategory(modules, cat_counts);

    // gliding active pill
    int selected_tab = 0;
    for (int c = 0; c < 9; ++c) if (categories[c].page == selected_page) selected_tab = c;
    const float pill_target = tab_y[selected_tab] - wp.y;
    if (pill_rel < 0.0f) pill_rel = pill_target;
    pill_rel += (pill_target - pill_rel) * ImSaturate(dt * 14.0f);
    {
        const ImVec2 pmin(wp.x + pad, wp.y + pill_rel);
        dl->AddRectFilled(pmin, ImVec2(pmin.x + tab_w, pmin.y + tab_h), A(P::Soft()), Sc(10));
        const ImVec2 imin(pmin.x, pmin.y + Sc(10)), imax(pmin.x + Sc(3), pmin.y + Sc(26));
        DrawGlowRect(dl, imin, imax, Sc(2), P::Accent(), g_alpha, 12);
        dl->AddRectFilled(imin, imax, A(P::Accent()), Sc(2));
    }

    for (int c = 0; c < 9; ++c) {
        const ImVec2 tmin(wp.x + pad, tab_y[c]), tmax(tmin.x + tab_w, tmin.y + tab_h);
        ImGui::PushID(c);
        ImGui::SetCursorScreenPos(tmin);
        const bool clicked = ImGui::InvisibleButton("##tab", ImVec2(tab_w, tab_h));
        const bool hov = ImGui::IsItemHovered();
        const float hv = Anim(ImGui::GetItemID(), hov ? 1.0f : 0.0f, 16);
        ImGui::PopID();
        const bool active = categories[c].page == selected_page;
        if (narrow && hov) ImGui::SetTooltip("%s", categories[c].label);
        if (!active) dl->AddRectFilled(tmin, tmax, A(P::Border(), 0.55f * hv), Sc(10));
        const float cy = tmin.y + tab_h * 0.5f;
        const float icon_cx = narrow ? tmin.x + tab_w * 0.5f : tmin.x + Sc(14 + 8);
        IconC(dl, 14, icon_cx, cy, A(active ? P::Accent() : Mix(P::Muted(), P::Label(), hv)), categories[c].icon);
        if (!narrow) TextC(dl, 13, tmin.x + Sc(14 + 16 + 10), cy, A(active ? P::Text() : Mix(P::Label(), P::Text(), hv)), categories[c].label);
        if (c < 4) {
            const int count = cat_counts[c];
            if (narrow) {
                if (count > 0) dl->AddCircleFilled(ImVec2(icon_cx + Sc(9), tmin.y + Sc(9)), Sc(3), A(P::Accent()));
            } else {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%d", count);
                const float bw = (std::max)(Sc(18), TextSize(11, buf).x + Sc(12));
                const ImVec2 bmin(tmax.x - Sc(10) - bw, cy - Sc(9));
                dl->AddRectFilled(bmin, ImVec2(bmin.x + bw, bmin.y + Sc(18)), A(active ? P::Accent() : P::Soft()), Sc(9));
                TextC(dl, 11, bmin.x + (bw - TextSize(11, buf).x) * 0.5f, cy, A(active ? Theme::Current().onAccent : P::Label()), buf);
            }
        }
        if (clicked && categories[c].page != active_page && trans_t < 0.0f) {
            trans_from = active_page;
            trans_to = categories[c].page;
            trans_t = 0.0f;
        }
    }

    // profile chip (bottom)
    {
        const float chip_h = Sc(44);
        const ImVec2 cmin(wp.x + pad, wp.y + h - Sc(16) - chip_h), cmax(cmin.x + tab_w, cmin.y + chip_h);
        const float cy = cmin.y + chip_h * 0.5f;
        dl->AddRectFilled(cmin, cmax, A(P::Background()), Sc(10));
        dl->AddRect(cmin, cmax, A(P::Border()), Sc(10));
        if (narrow) {
            IconC(dl, 12, cmin.x + tab_w * 0.5f, cy, A(P::Accent()), ICON_FA_FILE);
        } else {
            IconC(dl, 12, cmin.x + Sc(16), cy, A(P::Accent()), ICON_FA_FILE);
            const float end_w = TextSize(10, "END").x;
            const float tx = cmin.x + Sc(10 + 12 + 8);
            TextC(dl, 10, tx, cy - Sc(8), A(P::Muted()), "Profile");
            ImGui::PushClipRect(ImVec2(tx, cmin.y), ImVec2(cmax.x - Sc(10) - end_w - Sc(6), cmax.y), true);
            TextC(dl, 12, tx, cy + Sc(7), A(P::Text()), Config::active_profile.empty() ? "(none)" : Config::active_profile.c_str());
            ImGui::PopClipRect();
            TextC(dl, 10, cmax.x - Sc(10) - end_w, cy, A(P::Muted()), "END");
        }
    }

    // ================= content =================
    const float ca = oa * page_alpha;
    g_alpha = ca;
    const float cx = wp.x + sb_w + Sc(20);
    const float cw = w - sb_w - Sc(40);
    const float hy = wp.y + Sc(20) + Sc(6) * (1.0f - page_alpha); // 6 px rise-in on page change
    const float list_top = hy + Sc(36 + 14);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ca);

    const bool module_page = display_page < 4;
    Filter filter{search, favorites_only, false, display_page}; // favorites flag refreshed after the button below
    const char* title = kPageTitles[display_page];
    if (module_page) {
        if (search[0]) title = favorites_only ? "Search favorites" : "Search results";
        else if (favorites_only) title = "Favorite modules";
    }
    const float title_w = TextSize(18, title).x;
    if (module_page) {
        const float fav_w = Sc(12 + 12 + 7 + 12) + TextSize(12, "Favorites").x;
        const float search_w = ImClamp(cw - title_w - Sc(24) - fav_w - Sc(10) - Sc(34 + 8), Sc(110), Sc(210));
        const ImVec2 fav_pos(cx + cw - fav_w, hy + Sc(1));
        ImGui::SetCursorScreenPos(fav_pos);
        if (DrawRoundedButton(dl, "##favorites_filter", "", ImVec2(fav_w, Sc(34)), favorites_only ? P::Accent() : P::Soft(),
                              favorites_only ? Theme::Current().hover : Mix(P::Soft(), P::Text(), 0.08f), 0, g_alpha, Sc(10)))
            favorites_only = !favorites_only;
        filter.favorites_only = favorites_only;
        const Stats stats = CountScope(modules, filter);
        char sub[48];
        std::snprintf(sub, sizeof(sub), "%d of %d enabled", stats.enabled, stats.total);
        TextC(dl, 18, cx, hy + Sc(11), A(P::Text()), title);
        TextC(dl, 12, cx, hy + Sc(29), A(P::Muted()), sub);
        const ImU32 fav_fg = favorites_only ? Theme::Current().onAccent : P::Accent();
        IconC(dl, 12, fav_pos.x + Sc(12 + 6), fav_pos.y + Sc(17), A(fav_fg), ICON_FA_STAR);
        TextC(dl, 12, fav_pos.x + Sc(12 + 12 + 7), fav_pos.y + Sc(17), A(favorites_only ? Theme::Current().onAccent : P::Text()), "Favorites");

        // undo (same History as the classic menu), left of the search field
        const ImVec2 umin(fav_pos.x - Sc(10) - search_w - Sc(8) - Sc(34), fav_pos.y);
        const bool can_undo = Menu::History::CanUndo();
        ImGui::SetCursorScreenPos(umin);
        ImGui::BeginDisabled(!can_undo);
        if (DrawRoundedButton(dl, "##undo_menu", "", ImVec2(Sc(34), Sc(34)), P::Soft(), Mix(P::Soft(), P::Text(), 0.08f), 0,
                              g_alpha * (can_undo ? 1.0f : 0.5f), Sc(10)))
            Menu::History::Undo();
        ImGui::EndDisabled();
        IconC(dl, 12, umin.x + Sc(17), umin.y + Sc(17), A(P::Accent(), can_undo ? 1.0f : 0.4f), ICON_FA_UNDO);
        if (ImGui::IsMouseHoveringRect(umin, ImVec2(umin.x + Sc(34), umin.y + Sc(34))) && ImGui::IsWindowHovered()) ImGui::SetTooltip("Undo");

        // search field: card + border drawn here, the ImGui input on top is transparent
        const ImVec2 smin(fav_pos.x - Sc(10) - search_w, fav_pos.y), smax(smin.x + search_w, smin.y + Sc(34));
        dl->AddRectFilled(smin, smax, A(P::Card()), Sc(10));
        IconC(dl, 12, smin.x + Sc(12 + 6), smin.y + Sc(17), A(P::Muted()), ICON_FA_SEARCH);
        const float ix = smin.x + Sc(12 + 12 + 8);
        ImGui::SetWindowFontScale(fs * g_s * 0.75f); // ~12 px text in the field
        const float pad_y = (std::max)(0.0f, (Sc(34) - ImGui::GetFontSize()) * 0.5f);
        ImGui::SetCursorScreenPos(ImVec2(ix, smin.y));
        ImGui::SetNextItemWidth((std::max)(10.0f, smax.x - Sc(10) - ix));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, pad_y));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, ColV(P::Text(), 1.0f));
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ColV(P::Muted(), 1.0f));
        ImGui::InputTextWithHint("##module_search", "Search modules...", search, sizeof(search));
        const bool focused = ImGui::IsItemActive();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(2);
        ImGui::SetWindowFontScale(fs);
        dl->AddRect(smin, smax, A(focused ? P::Accent() : P::Border()), Sc(10));
    } else {
        TextC(dl, 18, cx, hy + Sc(18), A(P::Text()), title);
    }
    const bool search_changed = previous_search != search;
    previous_search = search;

    if (module_page) {
        ImGui::SetCursorScreenPos(ImVec2(cx, list_top));
        ImGui::BeginChild("##cg_modules", ImVec2(cw, (std::max)(1.0f, wp.y + h - list_top)), false,
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar); // wheel scrolls, no visible bar
        ImDrawList* cdl = ImGui::GetWindowDrawList();
        const std::vector<int> visible = VisibleSorted(modules, filter);
        for (int i : visible) DrawCard(cdl, modules, i, filter, narrow, search_changed);
        if (visible.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, P::Muted());
            ImGui::TextUnformatted(favorites_only ? "No matching favorites. Pin modules using the star on their card." : "No matching modules or settings.");
            ImGui::PopStyleColor();
        }
        ImGui::Dummy(ImVec2(1, 1));
        ImGui::EndChild();
    } else {
        const ImVec2 area_min(cx, list_top);
        const float area_h = (std::max)(1.0f, wp.y + h - list_top - Sc(20));
        ImGui::SetCursorScreenPos(area_min);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 11.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 6.0f);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ColV(P::Soft(), ca * 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ColV(P::Muted(), ca * 0.55f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ColV(P::Accent(), ca * 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ColV(P::Accent(), ca));
        DrawClientPage(display_page, dl, area_min, cw, area_h, ca, modules);
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
    ImGui::PopStyleVar(); // content alpha

    dl->AddRect(wp, ImVec2(wp.x + w, wp.y + h), ColA(P::Border(), oa), Sc(16), 0, 1.0f);
    if (Config::particles_enabled) Menu::Particles();
    Menu::History::EndFrame();
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);
}

} // namespace ClickGui
