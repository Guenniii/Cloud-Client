#pragma once
// Logic shared by both menu variants (classic DrawPhantomMenu in menu.cpp, ClickGui::Draw in clickgui.cpp):
// categories, icons, search/filter, counters, conflict tooltip, module reset, setting help, OreSim seed field.
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <windows.h>
#include "../dependencies/font/IconsFontAwesome5.h"
#include "../dependencies/imgui/imgui.h"
#include "../utils/config/config.hpp"
#include "../utils/oresim/oresim.hpp"
#include "../utils/theme/theme.hpp"
#include "../modules/settings.hpp"
#include "module_registry.hpp"
#include "pages.hpp"
#include "palette.hpp"
#include "widgets.hpp"

namespace Menu::ModuleView {
using namespace Menu::Widgets;

struct CategoryTab {
    const char* icon;
    const char* label;
    int page;
};

// "Pages" of the main window: 0-3 are module categories, the rest are client pages.
inline constexpr int PAGE_COMBAT = 0;
inline constexpr int PAGE_MOVEMENT = 1;
inline constexpr int PAGE_UTILITY = 2;
inline constexpr int PAGE_VISUALS = 3;
inline constexpr int PAGE_CONFIG = 4;
inline constexpr int PAGE_SETTINGS = 5;
inline constexpr int PAGE_SEEDCRACKER = 6;
inline constexpr int PAGE_FRIENDS = 7;
inline constexpr int PAGE_THEMES = 8;

inline constexpr CategoryTab categories[] = {
    {ICON_FA_CROSSHAIRS, "Combat", PAGE_COMBAT},
    {ICON_FA_WIND, "Movement", PAGE_MOVEMENT},
    {ICON_FA_HAMMER, "Utility", PAGE_UTILITY},
    {ICON_FA_EYE, "Visuals", PAGE_VISUALS},
    {ICON_FA_WRENCH, "Settings", PAGE_SETTINGS},
    {ICON_FA_PALETTE, "Themes", PAGE_THEMES},
    {ICON_FA_FILE, "Profiles", PAGE_CONFIG},
    {ICON_FA_USER_FRIENDS, "Friends", PAGE_FRIENDS},
    {ICON_FA_ANCHOR, "SeedCracker", PAGE_SEEDCRACKER},
};

inline constexpr const char* kPageTitles[] = {"Combat modules", "Movement modules", "Utility modules", "Visual modules",
                                              "Profiles", "Client settings", "SeedCracker", "Friends", "Themes"};

// Client pages (PAGE_CONFIG..PAGE_THEMES); area is given by the caller, the page draws its own card.
inline void DrawClientPage(int page, ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha,
                           std::span<ModuleData> modules) {
    if (page == PAGE_CONFIG)
        Pages::DrawConfigPage(draw_list, area_min, area_w, area_h, alpha, modules.data(), static_cast<int>(modules.size()));
    else if (page == PAGE_SETTINGS)
        Pages::DrawSettingsPage(draw_list, area_min, area_w, area_h, alpha);
    else if (page == PAGE_SEEDCRACKER)
        Pages::DrawSeedCrackerPage(draw_list, area_min, area_w, area_h, alpha);
    else if (page == PAGE_THEMES)
        Pages::DrawThemePage(draw_list, area_min, area_w, area_h, alpha);
    else if (page == PAGE_FRIENDS)
        Pages::DrawFriendsPage(draw_list, area_min, area_w, area_h, alpha);
}

// Presentation-only icons; module identities and profile keys stay unchanged.
inline const char* ModuleIcon(const ModuleData& module) {
    struct Entry { const char* name; const char* icon; };
    static const Entry entries[] = {
        {"Target HUD", ICON_FA_CROSSHAIRS},
        {"Loot Beacon", ICON_FA_GEM},
        {"Echo Trail", ICON_FA_WIND},
        {"Item ESP", ICON_FA_GEM},
        {"Friends", ICON_FA_USER_FRIENDS},
        {"Explosion Preview", ICON_FA_BOMB},
        {"Projectile Prediction", ICON_FA_LOCATION_ARROW},
        {"Sound ESP", ICON_FA_VOLUME_UP},
        {"Storage ESP", ICON_FA_BOX},
        {"Auto Armor", ICON_FA_SHIELD_ALT},
        {"Refill", ICON_FA_SYNC_ALT},
        {"Trigger Bot", ICON_FA_CROSSHAIRS},
        {"Hit Effect", ICON_FA_MAGIC},
        {"Predict Double Hand", ICON_FA_HAND_HOLDING_HEART},
        {"Auto Shield Breaker", ICON_FA_HAMMER},
        {"Fake Lag", ICON_FA_STOPWATCH},
        {"Aim Assist", ICON_FA_BULLSEYE},
        {"Auto Tool", ICON_FA_TOOLS},
        {"Safe Anchor", ICON_FA_ANCHOR},
        {"Breach Swap", ICON_FA_EXCHANGE_ALT},
        {"Auto Mace", ICON_FA_HAMMER},
        {"Auto Pearl Catch", ICON_FA_FEATHER_ALT},
        {"Short Wind Charge", ICON_FA_WIND},
        {"Key Pearl", ICON_FA_FEATHER_ALT},
        {"Totem Warner", ICON_FA_EYE},
        {"Safe Walk", ICON_FA_SHOE_PRINTS},
        {"AntiBot", ICON_FA_ROBOT},
        {"SilentAim", ICON_FA_CROSSHAIRS},
        {"Auto Crystal", ICON_FA_SKULL_CROSSBONES},
        {"Reach", ICON_FA_ARROWS_ALT_H},
        {"FastPlace", ICON_FA_CUBES},
        {"AutoSprint", ICON_FA_RUNNING},
        {"NoJumpDelay", ICON_FA_PARACHUTE_BOX},
        {"AutoTotem", ICON_FA_HEART},
        {"SeedCracker", ICON_FA_SEEDLING},
        {"OreSim", ICON_FA_CUBES},
        {"Player ESP", ICON_FA_EYE},
        {"Info HUD", ICON_FA_LIST},
    };
    for (const auto& entry : entries) if (std::string_view(module.name) == entry.name) return entry.icon;
    return module.category == 0 ? ICON_FA_CROSSHAIRS : module.category == 1 ? ICON_FA_WIND : module.category == 3 ? ICON_FA_EYE : ICON_FA_HAMMER;
}

// Search / filter state is owned by each menu; this only evaluates it.
struct Filter {
    const char* search;
    bool favorites_only;
    bool active_only;
    int page;

    bool Matches(const char* name) const {
        std::string haystack(name), needle(search);
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
        std::transform(needle.begin(), needle.end(), needle.begin(), lower);
        return haystack.find(needle) != std::string::npos;
    }
    bool MatchesModule(const ModuleData& module) const {
        if (Matches(module.name)) return true;
        for (const auto& setting : module.settings_list) if (Matches(setting.DisplayName())) return true;
        return module.enabled == &OreSim_Enabled && Matches("World seed Weltseed");
    }
    bool InScope(const ModuleData& module) const {
        return favorites_only ? module.favorite : (search[0] != 0 || module.category == page);
    }
    bool Visible(const ModuleData& module) const {
        return InScope(module) && (!active_only || *module.enabled) && MatchesModule(module);
    }
};

struct Stats { int shown = 0, total = 0, enabled = 0; };
inline Stats CountScope(std::span<const ModuleData> modules, const Filter& filter) {
    Stats stats;
    for (const auto& module : modules) if (filter.InScope(module)) {
        ++stats.total; if (*module.enabled) ++stats.enabled; if (filter.Visible(module)) ++stats.shown;
    }
    return stats;
}

// Enabled modules per category (tab badges).
inline void CountEnabledPerCategory(std::span<const ModuleData> modules, int (&counts)[4]) {
    for (const auto& module : modules)
        if (*module.enabled && module.category >= 0 && module.category < 4) ++counts[module.category];
}

// Visible module indices, favorites first (stable).
inline std::vector<int> VisibleSorted(std::span<const ModuleData> modules, const Filter& filter) {
    std::vector<int> visible;
    for (int i = 0; i < static_cast<int>(modules.size()); ++i)
        if (filter.Visible(modules[i])) visible.push_back(i);
    std::stable_sort(visible.begin(), visible.end(), [&](int a, int b) {
        return modules[a].favorite && !modules[b].favorite;
    });
    return visible;
}

// Tooltip for a hotkey conflict; call while the badge is hovered.
inline void DrawConflictTooltip(std::span<const ModuleData> modules, int i) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted("This key is assigned more than once or reserved:");
    if (modules[i].hotkey == Config::menu_keybind) ImGui::BulletText("Menu toggle");
    if (modules[i].hotkey == VK_END) ImGui::BulletText("DLL unload");
    if (modules[i].hotkey == VK_ESCAPE) ImGui::BulletText("Escape");
    for (int other = 0; other < static_cast<int>(modules.size()); ++other)
        if (other != i && modules[other].hotkey == modules[i].hotkey) ImGui::BulletText("%s", modules[other].name);
    ImGui::EndTooltip();
}

// Reset a module to defaults, including the OreSim seed field state.
inline void ResetModule(std::span<ModuleData> modules, int i) {
    Registry::ResetModule(i);
    if (modules[i].enabled == &OreSim_Enabled) { OreSim::hasManualSeed = false; OreSim::manualSeed = 0; OreSim::seedInput[0] = 0; OreSim::seedError.clear(); OreSim::seedFromCracker = false; }
}

// Tooltip text for a setting, or nullptr.
inline const char* SettingHelp(const ModuleSetting& s) {
    const std::string_view name(s.DisplayName());
    const char* help = nullptr;
    if (s.type == SettingType::ColorRGB) help = "Choose a custom color. Click the swatch to open the color picker.";
    else if (name.find("Glow") != std::string_view::npos) help = "Adds a soft glow around the rendered markers or effects.";
    else if (name.find("Range") != std::string_view::npos || name.find("radius") != std::string_view::npos) help = "Maximum distance or area considered by this module; the unit is shown beside the value.";
    else if (name.find("Delay") != std::string_view::npos || name.find("delay") != std::string_view::npos) help = "Time between the corresponding actions. A larger value makes the action wait longer.";
    else if (name.find("Line width") != std::string_view::npos) help = "Thickness of the rendered lines in pixels.";
    else if (name.find("Display duration") != std::string_view::npos) help = "How long the visual marker stays visible.";
    else if (name.find("Antibot") != std::string_view::npos || name.find("AntiBot") != std::string_view::npos) help = "Uses the global AntiBot checks when selecting eligible players.";
    else if (name.find("Theme") != std::string_view::npos) help = "Uses colors from the selected theme instead of custom colors.";
    return help;
}

// Seed entry uses the same palette and animated button as the other settings.
inline constexpr float ORESIM_SEED_ROW_HEIGHT = 108.0f;
inline void DrawOreSimSeedInput(ImDrawList* draw_list, ImVec2 pos, float width, float alpha) {
    draw_list->AddText(pos, ColA(Menu::Palette::Label(), alpha), "World Seed");
    const char* badge = OreSim::seedFromCracker ? "CRACKED" : "MANUAL";
    ImVec2 badge_text = ImGui::CalcTextSize(badge);
    ImVec2 badge_min(pos.x + width - badge_text.x - 16.0f, pos.y - 2.0f);
    draw_list->AddRectFilled(badge_min, ImVec2(pos.x + width, pos.y + badge_text.y + 2.0f), ColA(Menu::Palette::Soft(), alpha), 5.0f);
    draw_list->AddText(ImVec2(badge_min.x + 8.0f, pos.y),
                       ColA(OreSim::seedFromCracker ? Menu::Palette::Success() : Menu::Palette::Accent(), alpha), badge);

    const float field_y = pos.y + 26.0f;
    const float button_w = ImGui::CalcTextSize("set").x + 28.0f;
    const float field_w = (std::max)(80.0f, width - button_w - 10.0f);
    ImGui::SetCursorScreenPos(ImVec2(pos.x, field_y));
    ImGui::SetNextItemWidth(field_w);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 9.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ColV(Menu::Palette::Background(), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColV(Menu::Palette::Soft(), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ColV(Menu::Palette::Background(), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ColV(Menu::Palette::Text(), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ColV(Menu::Palette::Muted(), 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ColV(Menu::Palette::Soft(), 1.0f));
    bool submit = ImGui::InputTextWithHint("##oresim_seed", "Enter Seed ...", OreSim::seedInput,
                                         sizeof(OreSim::seedInput), ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemEdited()) {
        OreSim::hasManualSeed = false;
        OreSim::seedError.clear();
    }
    const bool focused = ImGui::IsItemActive();
    ImVec2 field_min = ImGui::GetItemRectMin(), field_max = ImGui::GetItemRectMax();
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + field_w + 10.0f, field_y));
    submit |= DrawRoundedButton(draw_list, "##oresim_apply_seed", "set",
                                ImVec2(button_w, field_max.y - field_min.y), Menu::Palette::Accent(),
                                Theme::Current().hover, Theme::Current().onAccent, alpha, 8.0f, true);
    if (submit) {
        OreSim::hasManualSeed = OreSim::ParseSeed(OreSim::seedInput, OreSim::manualSeed);
        OreSim::seedError = OreSim::hasManualSeed ? "" : "Please enter a valid numeric seed.";
    }
    const bool error = !OreSim::seedError.empty();
    const ImU32 error_color = IM_COL32(215, 85, 105, 255);
    draw_list->AddRect(field_min, field_max,
        ColA(error ? error_color : focused ? Menu::Palette::Accent() : Menu::Palette::Border(), alpha), 8.0f, 0, focused ? 1.5f : 1.0f);
    const ImU32 state_color = error ? error_color : OreSim::hasManualSeed ? Menu::Palette::Success() : Menu::Palette::Muted();
    const char* message = error ? OreSim::seedError.c_str() : OreSim::hasManualSeed
        ? (OreSim::seedFromCracker ? "Seed verified by the SeedCracker" : "Seed applied")
        : "Press Enter or click Apply to confirm";
    const float status_y = field_max.y + 10.0f;
    draw_list->AddCircleFilled(ImVec2(pos.x + 3.0f, status_y + ImGui::GetFontSize() * 0.5f), 3.0f, ColA(state_color, alpha));
    draw_list->AddText(ImVec2(pos.x + 14.0f, status_y), ColA(state_color, alpha), message);
    draw_list->AddLine(ImVec2(pos.x, pos.y + 98.0f), ImVec2(pos.x + width, pos.y + 98.0f), ColA(Menu::Palette::Border(), alpha));
}
} // namespace Menu::ModuleView
