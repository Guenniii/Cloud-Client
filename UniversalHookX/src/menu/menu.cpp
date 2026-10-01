#include "../utils/lifecycle/lifecycle.hpp"
#include "../utils/theme/theme.hpp"
#include "menu.hpp"
#include "../input/utility_suite.hpp"
#include "status_hud.hpp"
#include "performance.hpp"
#include "login.hpp"
#include "../input/key_mapping.hpp"
#include "resources.hpp"
#include "module_registry.hpp"
#include "widgets.hpp"
#include "palette.hpp"
#include "pages.hpp"
#include "key_names.hpp"
#include "../base.hpp"
#include "../console/console.hpp"
#include "../utils/config/config.hpp"
#include <cmath>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <windows.h>
// ImGui
#include "../dependencies/font/IconsFontAwesome5.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_win32.h"
#include "../dependencies/imgui/imgui_internal.h"
// SeedCracker
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include "../utils/oresim/oresim.hpp"
#include "../utils/esp/player_esp.hpp"

#include "../modules/settings.hpp"

#pragma warning(disable : 4244)
#pragma warning(disable : 4005)

namespace ig = ImGui;
using namespace Menu::Widgets;
using Menu::GetKeyName;

// ===== UI STATE VARIABLES =====
static bool toggled = true;
static float open_alpha = 0.0f;

// SettingType / ModuleSetting / ModuleData sind jetzt in config.hpp definiert,
// damit config.cpp sie ebenfalls verwenden kann.

struct CategoryTab {
    const char* icon;
    const char* label;
};

// Persistente Expand-States (auÃƒÅ¸erhalb der Funktion, damit sie ÃƒÂ¼ber Frames erhalten bleiben)
static std::unordered_map<int, bool> card_expanded;
static std::unordered_map<int, float> card_expand_anim;

// --- Icon Placeholders ---
// Replace these with your actual icon font definitions (e.g., FontAwesome)
#define ICON_FA_BOLT "~"
#define ICON_FA_PUZZLE "P"
#define ICON_FA_SAVE "D"
#define ICON_FA_SLIDERS "O"
#define ICON_FA_CHEVRON_D "v"
#define ICON_FA_USER "U"
#define ICON_FA_CLOCK "C"
#define ICON_FA_STAR "*"

// Seed entry uses the same palette and animated button as the other settings.
static constexpr float ORESIM_SEED_ROW_HEIGHT = 108.0f;
static void DrawOreSimSeedInput(ImDrawList* draw_list, ImVec2 pos, float width, float alpha) {
    draw_list->AddText(pos, ColA(Menu::Palette::Label(), alpha), "World Seed");
    const char* badge = "MANUAL";
    ImVec2 badge_text = ImGui::CalcTextSize(badge);
    ImVec2 badge_min(pos.x + width - badge_text.x - 16.0f, pos.y - 2.0f);
    draw_list->AddRectFilled(badge_min, ImVec2(pos.x + width, pos.y + badge_text.y + 2.0f), ColA(Menu::Palette::Soft(), alpha), 5.0f);
    draw_list->AddText(ImVec2(badge_min.x + 8.0f, pos.y), ColA(Menu::Palette::Accent(), alpha), badge);

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
        ? "Seed applied" : "Press Enter or click Apply to confirm";
    const float status_y = field_max.y + 10.0f;
    draw_list->AddCircleFilled(ImVec2(pos.x + 3.0f, status_y + ImGui::GetFontSize() * 0.5f), 3.0f, ColA(state_color, alpha));
    draw_list->AddText(ImVec2(pos.x + 14.0f, status_y), ColA(state_color, alpha), message);
    draw_list->AddLine(ImVec2(pos.x, pos.y + 98.0f), ImVec2(pos.x + width, pos.y + 98.0f), ColA(Menu::Palette::Border(), alpha));
}

// ZeilenhÃƒÂ¶he je nach Setting-Typ (fÃƒÂ¼r die Expand-Animation)
static float SettingRowHeight(const ModuleSetting& s) {
    return s.type == SettingType::SliderInt ? 52.0f : 32.0f;
}

// Index des Moduls, das gerade auf einen Tastendruck wartet (-1 = keine Aufnahme aktiv)
static auto& capturing_module = Config::capturing_module_key;

// "Seiten" im Hauptfenster: 0-2 sind die Kategorie-Tabs, 3/4 sind Config/Settings.
// Alles laeuft ueber denselben Crossfade-Mechanismus wie ein normaler Kategorie-Switch.
constexpr int PAGE_COMBAT = 0;
constexpr int PAGE_MOVEMENT = 1;
constexpr int PAGE_UTILITY = 2;
constexpr int PAGE_CONFIG = 3;
constexpr int PAGE_SETTINGS = 4;
constexpr int PAGE_SEEDCRACKER = 5;

namespace Menu {

    void InitializeContext(HWND hwnd) {
        if (ig::GetCurrentContext( ))
            return;

        ImGui::CreateContext( );
        ImGui_ImplWin32_Init(hwnd);

        ImGuiIO& io = ImGui::GetIO( );
        io.IniFilename = io.LogFilename = nullptr;

        Resources::SetupStyleAndFonts();
    }

    void Particles( ) {

        ImVec2 screen_size = {(float)GetSystemMetrics(SM_CXSCREEN), (float)GetSystemMetrics(SM_CYSCREEN)};

        static ImVec2 partile_pos[100];
        static ImVec2 partile_target_pos[100];
        static float partile_speed[100];
        static float partile_radius[100];

        for (int i = 1; i < 50; i++) {
            if (partile_pos[i].x == 0 || partile_pos[i].y == 0) {
                partile_pos[i].x = rand( ) % (int)screen_size.x + 1;
                partile_pos[i].y = 15.f;
                partile_speed[i] = 1 + rand( ) % 25;
                partile_radius[i] = rand( ) % 4;

                partile_target_pos[i].x = rand( ) % (int)screen_size.x;
                partile_target_pos[i].y = screen_size.y * 2;
            }

            partile_pos[i] = ImLerp(partile_pos[i], partile_target_pos[i], ImGui::GetIO( ).DeltaTime * (partile_speed[i] / 60));

            if (partile_pos[i].y > screen_size.y) {
                partile_pos[i].x = 0;
                partile_pos[i].y = 0;
            }

            ImGui::GetWindowDrawList( )->AddCircleFilled(partile_pos[i], partile_radius[i], ImColor(174, 139, 148, 255 / 2));
        }
    }

    void DrawPhantomMenu( ) {
        auto modules = Registry::GetModules();
        //       auf Namespace-Ebene definiert, siehe oben) =====
        static CategoryTab categories[3] = {
            {ICON_FA_CROSSHAIRS, "Combat"},
            {ICON_FA_WIND, "Movement"},
            {ICON_FA_HAMMER, "Utility"},
        };

        static int active_page = PAGE_COMBAT;   // 0-2 = Kategorie-Tabs, 3 = Config, 4 = Settings
        static int last_category = PAGE_COMBAT; // merkt sich den zuletzt gewaehlten Tab (fuer die Pill-Position)
        static int trans_from = -1, trans_to = -1;
        static float trans_t = -1.0f; // -1 = keine Transition
        const float main_w = 700.0f;
        const float main_h = 520.0f;
        // Window settings to mimic the borderless rounded design
        ImGui::SetNextWindowSize(ImVec2(700, 520));
        ImGui::SetNextWindowPos(
            ImVec2((GetSystemMetrics(SM_CXSCREEN) - main_w) * 0.5f,
                   (GetSystemMetrics(SM_CYSCREEN) - main_h) * 0.5f),
            ImGuiCond_Once);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Menu::Palette::Background());

        ImGui::Begin("PhantomUI", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        ImDrawList* draw_list = ImGui::GetWindowDrawList( );
        ImVec2 p = ImGui::GetCursorScreenPos( );
        float window_width = ImGui::GetWindowWidth( );
        float window_height = ImGui::GetWindowHeight( );
        float footer_reserved_h = 70.0f;

        // ==========================================
        // 1. HEADER SECTION
        // ==========================================
        float logo_radius = 18.0f;
        ImVec2 logo_center = ImVec2(p.x + logo_radius, p.y + logo_radius);

        // Bild rund zugeschnitten ins Kreis-Bounding-Box zeichnen
        ImVec2 img_min = ImVec2(logo_center.x - logo_radius, logo_center.y - logo_radius);
        ImVec2 img_max = ImVec2(logo_center.x + logo_radius, logo_center.y + logo_radius);

        draw_list->AddImageRounded(
            Resources::LogoTexture(),
            img_min, img_max,
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, 255),
            logo_radius);

        ImGui::SetCursorPos(ImVec2(65, 24));
        ImGui::PushStyleColor(ImGuiCol_Text, Menu::Palette::Text());
        ImGui::Text("PHANTOM");
        ImGui::PopStyleColor( );

        ImGui::SetCursorPos(ImVec2(65, 40));
        ImGui::PushStyleColor(ImGuiCol_Text, Menu::Palette::Muted());
        ImGui::Text("v4.2.1");
        ImGui::PopStyleColor( );

        // ==========================================
        // Tab Bar (Category Selector)
        // ==========================================
        ImGui::SetCursorPos(ImVec2(180, 20));
        ImVec2 tab_p = ImGui::GetCursorScreenPos( );
        ImVec2 win_pos = ImGui::GetWindowPos( );
        float pill_h = 36.0f, pill_w = 360.0f;
        draw_list->AddRectFilled(tab_p, ImVec2(tab_p.x + pill_w, tab_p.y + pill_h), Menu::Palette::Soft(), 18.0f);

        int cat_counts[3] = {0, 0, 0};
        for (int m = 0; m < static_cast<int>(modules.size()); m++)
            if (*modules[m].enabled)
                cat_counts[modules[m].category]++;

        float tab_y = tab_p.y + 4.0f;
        float tab_h = pill_h - 8.0f;

        struct TabRect {
            float x, w;
        };
        static TabRect tab_rects[3];

        float icon_font_size = 15.0f;
        float icon_gap = 6.0f;

        float local_base_x = (tab_p.x - win_pos.x) + 4.0f;
        float measure_x = local_base_x;
        for (int c = 0; c < 3; c++) {
            ImVec2 icon_size = icons->CalcTextSizeA(icon_font_size, FLT_MAX, 0.0f, categories[c].icon);
            ImVec2 label_size = ImGui::CalcTextSize(categories[c].label);
            float tw = icon_size.x + icon_gap + label_size.x + 30.0f; // Padding links/rechts + Badge-Platz
            tab_rects[c] = {measure_x, tw};
            measure_x += tw + 6.0f;
        }

        static float highlight_local_x = 0.0f, highlight_local_w = 0.0f;
        static bool highlight_init = false;

        TabRect target = tab_rects[last_category];
        if (!highlight_init) {
            highlight_local_x = target.x;
            highlight_local_w = target.w;
            highlight_init = true;
        }
        highlight_local_x += (target.x - highlight_local_x) * ImSaturate(ImGui::GetIO( ).DeltaTime * 12.0f);
        highlight_local_w += (target.w - highlight_local_w) * ImSaturate(ImGui::GetIO( ).DeltaTime * 12.0f);

        float highlight_x = highlight_local_x + win_pos.x;
        float highlight_w = highlight_local_w;

        draw_list->AddRectFilled(ImVec2(highlight_x, tab_y), ImVec2(highlight_x + highlight_w, tab_y + tab_h), Menu::Palette::Card(), 14.0f);

        for (int c = 0; c < 3; c++) {
            bool is_active = (c == last_category);
            ImU32 col = is_active ? Menu::Palette::Accent() : Menu::Palette::Muted();

            float screen_x = tab_rects[c].x + win_pos.x;

            // Icon mit der Icon-Font zeichnen
            float icon_font_size = 15.0f;
            draw_list->AddText(icons, icon_font_size, ImVec2(screen_x + 10, tab_y + 9), col, categories[c].icon);

            // Icon-Breite messen, um das Label sauber danach zu positionieren
            ImVec2 icon_size = icons->CalcTextSizeA(icon_font_size, FLT_MAX, 0.0f, categories[c].icon);
            float icon_gap = 6.0f;

            // Label mit der normalen Text-Font
            draw_list->AddText(ImVec2(screen_x + 10 + icon_size.x + icon_gap, tab_y + 8), col, categories[c].label);

            if (cat_counts[c] > 0) {
                float badge_cx = screen_x + tab_rects[c].w - 14.0f;
                float badge_cy = tab_y + tab_h * 0.5f;
                draw_list->AddCircleFilled(ImVec2(badge_cx, badge_cy), 8.0f, Menu::Palette::Accent());
                char count_buf[8];
                snprintf(count_buf, sizeof(count_buf), "%d", cat_counts[c]);
                draw_list->AddText(ImVec2(badge_cx - 3, badge_cy - 7), Theme::Current().onAccent, count_buf);
            }

            ImGui::SetCursorScreenPos(ImVec2(screen_x, tab_y));
            ImGui::PushID(c);
            ImGui::InvisibleButton("##cat_tab", ImVec2(tab_rects[c].w, tab_h));
            if (ImGui::IsItemClicked( ) && c != active_page && trans_t < 0.0f) {
                trans_from = active_page;
                trans_to = c;
                trans_t = 0.0f;
            }
            ImGui::PopID( );
        }

        // Top Right Icons (Config, Settings, SeedCracker)
        ImVec2 icons_screen_pos = ImVec2(win_pos.x + window_width - 80, win_pos.y + 28);
        float icon_slot_w = 22.0f;

        ImU32 config_icon_col = (active_page == PAGE_CONFIG) ? Menu::Palette::Accent() : Menu::Palette::Muted();
        ImU32 settings_icon_col = (active_page == PAGE_SETTINGS) ? Menu::Palette::Accent() : Menu::Palette::Muted();
        ImU32 seedcracker_icon_col = (active_page == PAGE_SEEDCRACKER) ? Menu::Palette::Accent() : Menu::Palette::Muted();

        ImGui::PushFont(icons);
        ImGui::SetWindowFontScale(0.5f);
        draw_list->AddText(icons_screen_pos, config_icon_col, ICON_FA_FILE);
        draw_list->AddText(ImVec2(icons_screen_pos.x + icon_slot_w, icons_screen_pos.y), settings_icon_col, ICON_FA_WRENCH);
        draw_list->AddText(ImVec2(icons_screen_pos.x + icon_slot_w * 2.0f, icons_screen_pos.y), seedcracker_icon_col, ICON_FA_ANCHOR);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont( );

        // Config-Icon
        ImGui::SetCursorScreenPos(ImVec2(icons_screen_pos.x - 4, icons_screen_pos.y - 4));
        if (ImGui::InvisibleButton("##icon_config", ImVec2(icon_slot_w, 20.0f))) {
            if (active_page != PAGE_CONFIG && trans_t < 0.0f) {
                trans_from = active_page;
                trans_to = PAGE_CONFIG;
                trans_t = 0.0f;
            }
        }
        // Settings-Icon
        ImGui::SetCursorScreenPos(ImVec2(icons_screen_pos.x + icon_slot_w - 4, icons_screen_pos.y - 4));
        if (ImGui::InvisibleButton("##icon_settings", ImVec2(icon_slot_w, 20.0f))) {
            if (active_page != PAGE_SETTINGS && trans_t < 0.0f) {
                trans_from = active_page;
                trans_to = PAGE_SETTINGS;
                trans_t = 0.0f;
            }
        }
        // SeedCracker-Icon
        ImGui::SetCursorScreenPos(ImVec2(icons_screen_pos.x + icon_slot_w * 2.0f - 4, icons_screen_pos.y - 4));
        if (ImGui::InvisibleButton("##icon_seedcracker", ImVec2(icon_slot_w, 20.0f))) {
            if (active_page != PAGE_SEEDCRACKER && trans_t < 0.0f) {
                trans_from = active_page;
                trans_to = PAGE_SEEDCRACKER;
                trans_t = 0.0f;
            }
        }
        // ==========================================
        // 2. MODULES SECTION (Cards)
        // ==========================================

        // ===== Hotkey-Aufnahme: wartet auf die nÃƒÂ¤chste gedrÃƒÂ¼ckte Taste =====
        ImGui::SetCursorPosY(80);

        float content_alpha = 1.0f;
        int display_page = active_page;

        if (trans_t >= 0.0f) {
            trans_t += ImGui::GetIO( ).DeltaTime * 5.0f;

            if (trans_t < 0.5f) {
                display_page = trans_from;
                content_alpha = 1.0f - (trans_t / 0.5f);
            } else if (trans_t < 1.0f) {
                display_page = trans_to;
                content_alpha = (trans_t - 0.5f) / 0.5f;
                active_page = trans_to;
                if (trans_to < 3)
                    last_category = trans_to;
            } else {
                trans_t = -1.0f;
                active_page = trans_to;
                if (trans_to < 3)
                    last_category = trans_to;
                display_page = active_page;
                content_alpha = 1.0f;
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_alpha);

        if (display_page < 3) {
            const float cardsHeight = win_pos.y + window_height - 24.0f - footer_reserved_h - 12.0f - ImGui::GetCursorScreenPos().y;
            // Hide the scrollbar without disabling mouse-wheel scrolling or reserving its width.
            ImGui::BeginChild("ModuleCards", ImVec2(window_width - 48.0f, (std::max)(1.0f, cardsHeight)), false, ImGuiWindowFlags_NoScrollbar);
            draw_list = ImGui::GetWindowDrawList();
            for (int i = 0; i < static_cast<int>(modules.size()); i++) {
                if (modules[i].category != display_page)
                    continue;

                ImGui::PushID(i);

                ImVec2 card_p = ImGui::GetCursorScreenPos( );
                float card_w = ImGui::GetContentRegionAvail().x;
                float header_h = 60.0f;

                bool has_settings = !modules[i].settings_list.empty( );

                // --- Expand-Animation ---
                bool& expanded = card_expanded[i];
                float& anim = card_expand_anim[i];
                float target_h = 0.0f;
                for (auto& s : modules[i].settings_list)
                    target_h += SettingRowHeight(s);
                if (modules[i].enabled == &OreSim_Enabled) target_h += ORESIM_SEED_ROW_HEIGHT;
                if (has_settings)
                    target_h += 14.0f;
                float target_anim = expanded ? target_h : 0.0f;
                anim += (target_anim - anim) * ImSaturate(ImGui::GetIO( ).DeltaTime * 10.0f);

                float card_h = header_h + anim;

                // --- Card Background & Border ---
                draw_list->AddRectFilled(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), ColA(Menu::Palette::Card(), content_alpha), 12.0f);
                draw_list->AddRect(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), ColA(Menu::Palette::Border(), content_alpha), 12.0f, 0, 1.0f);

                if (*modules[i].enabled)
                    draw_list->AddRectFilled(ImVec2(card_p.x, card_p.y + 10), ImVec2(card_p.x + 4, card_p.y + header_h - 10), ColA(Menu::Palette::Accent(), content_alpha), 2.0f);

                draw_list->AddText(ImVec2(card_p.x + 15, card_p.y + 12), ColA(*modules[i].enabled ? Menu::Palette::Text() : Menu::Palette::Muted(), content_alpha), modules[i].name);

                // --- Hotkey-Badge: leer bis gesetzt, klickbar zum Binden ---
                ImVec2 name_size = ImGui::CalcTextSize(modules[i].name);
                bool is_capturing = (capturing_module == i);

                std::string hotkey_label;
                if (is_capturing)
                    hotkey_label = "...";
                else if (modules[i].hotkey != 0)
                    hotkey_label = GetKeyName(modules[i].hotkey);
                else
                    hotkey_label = "Set Key";

                ImVec2 hk_size = ImGui::CalcTextSize(hotkey_label.c_str( ));
                float badge_padding_x = 7.0f;
                float badge_w = hk_size.x + badge_padding_x * 2.0f;
                float badge_h = 18.0f;
                ImVec2 badge_min = ImVec2(card_p.x + 15 + name_size.x + 10.0f, card_p.y + 11.0f);
                ImVec2 badge_max = ImVec2(badge_min.x + badge_w, badge_min.y + badge_h);

                ImU32 badge_bg = is_capturing ? Menu::Palette::Soft() : Menu::Palette::Background();
                ImU32 badge_text = is_capturing ? Menu::Palette::Accent() : (modules[i].hotkey != 0 ? Menu::Palette::Text() : Menu::Palette::Muted());

                draw_list->AddRectFilled(badge_min, badge_max, ColA(badge_bg, content_alpha), 4.0f);
                if (is_capturing)
                    draw_list->AddRect(badge_min, badge_max, ColA(Menu::Palette::Accent(), content_alpha), 4.0f, 0, 1.5f);
                draw_list->AddText(ImVec2(badge_min.x + badge_padding_x, badge_min.y + 1.0f), ColA(badge_text, content_alpha), hotkey_label.c_str( ));

                ImGui::SetCursorScreenPos(badge_min);
                if (ImGui::InvisibleButton("##hotkey_badge", ImVec2(badge_w, badge_h))) {
                    Config::capturing_menu_key = false;
                    capturing_module = is_capturing ? -1 : i;
                }
                if (ImGui::IsItemHovered( ) && ImGui::IsMouseClicked(1)) {
                    modules[i].hotkey = 0;
                    if (is_capturing)
                        capturing_module = -1;
                }

                char sub_text[64];
                snprintf(sub_text, sizeof(sub_text), "%d settings", (int)modules[i].settings_list.size( ));
                draw_list->AddText(ImVec2(card_p.x + 15, card_p.y + 35), ColA(Menu::Palette::Muted(), content_alpha), sub_text);

                ImVec2 sub_size = ImGui::CalcTextSize(sub_text);
                if (has_settings)
                    draw_list->AddText(ImVec2(card_p.x + 15 + sub_size.x + 5, card_p.y + 35), ColA(Menu::Palette::Muted(), content_alpha), expanded ? "^" : ICON_FA_CHEVRON_D);

                if (modules[i].active_settings > 0) {
                    draw_list->AddCircleFilled(ImVec2(card_p.x + 95, card_p.y + 42), 2.0f, ColA(Menu::Palette::Muted(), content_alpha));
                    char active_text[64];
                    snprintf(active_text, sizeof(active_text), "%d active", modules[i].active_settings);
                    draw_list->AddText(ImVec2(card_p.x + 105, card_p.y + 35), ColA(Menu::Palette::Accent(), content_alpha), active_text);
                    draw_list->AddText(ImVec2(card_p.x + 105 + ImGui::CalcTextSize(active_text).x + 5, card_p.y + 35), ColA(Menu::Palette::Muted(), content_alpha), ICON_FA_CHEVRON_D);
                }

                // --- Klickbereich: nur der Header klappt die Card auf/zu ---
                float left_w = badge_min.x - card_p.x;
                ImGui::SetCursorScreenPos(card_p);
                if (has_settings && left_w > 1.0f) {
                    if (ImGui::InvisibleButton("##card_click_l", ImVec2(left_w, header_h)))
                        expanded = !expanded;
                }

                float right_start_x = badge_max.x;
                float right_w = (card_p.x + card_w - 60.0f) - right_start_x;
                if (has_settings && right_w > 1.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(right_start_x, card_p.y));
                    if (ImGui::InvisibleButton("##card_click_r", ImVec2(right_w, header_h)))
                        expanded = !expanded;
                }

                // --- Toggle Switch rechts (Enable/Disable, unabhÃƒÂ¤ngig vom Expand) ---
                float card_toggle_w = 36.0f, card_toggle_h = 20.0f;
                float card_toggle_margin = 20.0f;
                ImGui::SetCursorScreenPos(ImVec2(card_p.x + card_w - card_toggle_margin - card_toggle_w, card_p.y + header_h * 0.5f - card_toggle_h * 0.5f));
                CustomToggle("##mod_toggle", modules[i].enabled, content_alpha, card_toggle_w, card_toggle_h);

                // --- Settings-Inhalt rendern, sobald sichtbar ---
                if (anim > 1.0f) {
                    draw_list->PushClipRect(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), true);

                    float sy = card_p.y + header_h + 8.0f;
                    float row_w = card_w - 40.0f;
                    if (modules[i].enabled == &OreSim_Enabled) {
                        ImGui::SetCursorScreenPos(ImVec2(card_p.x + 20, sy));
                        DrawOreSimSeedInput(draw_list, ImVec2(card_p.x + 20, sy), row_w, content_alpha);
                        sy += ORESIM_SEED_ROW_HEIGHT;
                    }
                    for (auto& s : modules[i].settings_list) {
                        ImGui::PushID(s.name);
                        switch (s.type) {
                            case SettingType::Checkbox:
                                DrawCheckboxRow(draw_list, ImVec2(card_p.x + 20, sy), row_w, s.name, s.b_val, content_alpha);
                                break;
                            case SettingType::SliderInt:
                                DrawSliderRow(draw_list, ImVec2(card_p.x + 20, sy), row_w, s.name, s.i_val, s.i_min, s.i_max, s.suffix, content_alpha);
                                break;
                        }
                        ImGui::PopID( );
                        sy += SettingRowHeight(s);
                    }

                    draw_list->PopClipRect( );
                }

                ImGui::SetCursorScreenPos(ImVec2(card_p.x, card_p.y + card_h + 12.0f));
                ImGui::PopID( );
            }
            ImGui::Dummy(ImVec2(1,1));
            ImGui::EndChild();
            draw_list = ImGui::GetWindowDrawList();
        } else {
            // ==========================================
            // Config- / Settings-Seite (wie eine normale Kategorie-Seite gerendert)
            // ==========================================
            ImVec2 page_area_min = ImGui::GetCursorScreenPos( );
            float page_area_w = window_width - 48.0f;
            float footer_top_y = win_pos.y + window_height - 24.0f - footer_reserved_h;
            float page_area_h = (footer_top_y - 12.0f) - page_area_min.y;

            if (display_page == PAGE_CONFIG)
                Pages::DrawConfigPage(draw_list, page_area_min, page_area_w, page_area_h, content_alpha, modules.data(), static_cast<int>(modules.size()));
            else if (display_page == PAGE_SETTINGS)
                Pages::DrawSettingsPage(draw_list, page_area_min, page_area_w, page_area_h, content_alpha);
            else if (display_page == PAGE_SEEDCRACKER)
                Pages::DrawSeedCrackerPage(draw_list, page_area_min, page_area_w, page_area_h, content_alpha);
        }

        ImGui::PopStyleVar( ); // Alpha

        // ==========================================
        // 3. FOOTER SECTION
        // ==========================================
        ImVec2 footer_p = ImVec2(p.x, win_pos.y + window_height - 24.0f - footer_reserved_h);

        ImVec2 footer_bg_min = ImVec2(footer_p.x - 8.0f, footer_p.y + 5.0f);
        ImVec2 footer_bg_max = ImVec2(footer_p.x + window_width - 48.0f + 8.0f, footer_p.y + 65.0f);
        draw_list->AddRectFilled(footer_bg_min, footer_bg_max, Menu::Palette::Background(), 10.0f);

        draw_list->AddLine(ImVec2(footer_p.x, footer_p.y), ImVec2(footer_p.x + window_width - 48, footer_p.y), Menu::Palette::Border());

        footer_p.y += 15;

        float avatar_radius = 16.0f;
        ImVec2 avatar_center = ImVec2(footer_p.x + 18, footer_p.y + 18);

        draw_list->AddCircleFilled(ImVec2(avatar_center.x, avatar_center.y + 1.5f), avatar_radius + 1.0f, IM_COL32(0, 0, 0, 20));
        draw_list->AddCircleFilled(avatar_center, avatar_radius + 2.0f, IM_COL32(255, 255, 255, 255));

        ImVec2 avatar_min = ImVec2(avatar_center.x - avatar_radius, avatar_center.y - avatar_radius);
        ImVec2 avatar_max = ImVec2(avatar_center.x + avatar_radius, avatar_center.y + avatar_radius);
        draw_list->AddImageRounded(Resources::UserTexture(), avatar_min, avatar_max, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255), avatar_radius);

        draw_list->AddCircle(avatar_center, avatar_radius, Menu::Palette::Accent(), 0, 1.5f);

        ImVec2 status_pos = ImVec2(footer_p.x + 30, footer_p.y + 30);
        draw_list->AddCircleFilled(status_pos, 5.5f, IM_COL32(255, 255, 255, 255));
        draw_list->AddCircleFilled(status_pos, 4.0f, Menu::Palette::Success());
        DrawGlowCircle(draw_list, status_pos, 4.0f, Menu::Palette::Success(), 1.0f, 3);

        draw_list->AddText(ImVec2(footer_p.x + 45, footer_p.y + 4), Menu::Palette::Text(), Login::UserName().c_str( ));

        ImVec2 username_size = ImGui::CalcTextSize(Login::UserName().c_str( ));
        float badge_gap = 10.0f;
        ImVec2 badge_text_size = ImGui::CalcTextSize("BETA");
        float badge_padding_x2 = 8.0f;
        float badge_w2 = badge_text_size.x + badge_padding_x2 * 2.0f;
        float badge_h2 = 17.0f;

        ImVec2 badge_p2 = ImVec2(footer_p.x + 45 + username_size.x + badge_gap, footer_p.y + 3.5f);
        draw_list->AddRectFilled(badge_p2, ImVec2(badge_p2.x + badge_w2, badge_p2.y + badge_h2), Menu::Palette::Soft(), badge_h2 * 0.5f);
        draw_list->AddText(ImVec2(badge_p2.x + badge_padding_x2, badge_p2.y + 0.8f), Menu::Palette::Accent(), "BETA");

        draw_list->AddText(ImVec2(footer_p.x + 45, footer_p.y + 21), ImColor(140, 150, 170), "Online");

        float right_align_x = footer_p.x + window_width - 48.0f - 140.0f;

        if (Login::LicenseDays() == "PERMANENT") {
            draw_list->AddText(ImVec2(right_align_x, footer_p.y + 6), ColA(Menu::Palette::Muted(), 1.0f), "LICENSE");
            draw_list->AddText(ImVec2(right_align_x, footer_p.y + 20), Menu::Palette::Accent(), "Permanent");
        } else {
            draw_list->AddText(ImVec2(right_align_x, footer_p.y + 6), ColA(Menu::Palette::Muted(), 1.0f), "EXPIRES");
            char expiryBuffer[64];
            snprintf(expiryBuffer, sizeof(expiryBuffer), "%s", Login::LicenseExpiry().c_str( ));
            draw_list->AddText(ImVec2(right_align_x, footer_p.y + 20), Menu::Palette::Text(), expiryBuffer);
        }

        // ==========================================
        // Config/Settings werden weiter oben bereits als vollwertige Seite
        // im Content-Bereich gerendert (siehe display_page < 3 Verzweigung).
        // ==========================================

        if (Config::particles_enabled)
            Particles( );
        ImGui::End( );
        ImGui::PopStyleColor( );
        ImGui::PopStyleVar(2);
    }

    void Shutdown(bool vulkanTextures) {
        Lifecycle::Trace("Menu: stop Java features");
        Menu_Enabled=false;
        SeedCracker_Enabled=false;OreSim_Enabled=false;PlayerESP_Enabled=false;
        AutoArmor_Enabled=false;Refill_Enabled=false;HitEffect_Enabled=false;PredictDoubleHand_Enabled=false;ShieldBreaker_Enabled=false;
        if(auto env=SeedCracker::GetMinecraftJNIEnv()) {
            SeedCracker::RequestClientUpdate(env,false);
            if(OreSim::bridge) {
                env->CallStaticVoidMethod(OreSim::bridge,OreSim::request,0,1,JNI_FALSE,jlong(0),JNI_FALSE);
                if(env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteGlobalRef(OreSim::bridge);OreSim::bridge=nullptr;
            }
            if(PlayerESP::bridge) {env->DeleteGlobalRef(PlayerESP::bridge);PlayerESP::bridge=nullptr;}
            if(PlayerESP::skinBridge) {env->DeleteGlobalRef(PlayerESP::skinBridge);PlayerESP::skinBridge=nullptr;}
        }
        Lifecycle::Trace("Menu: release textures");
        Resources::ReleaseTextures(vulkanTextures);
    }

    void Render( ) {
        Performance::Scope clientTiming(Performance::Client);
        Theme::ApplyStyle();
        Registry::ProcessHotkeys(!Login::IsLoggedIn());
        Registry::CaptureHotkey(); // LÃƒÂ¤uft immer, auch bei geschlossenem MenÃƒÂ¼

        if (!Login::IsLoggedIn()) {
            Login::Render(Menu_Enabled, Particles);
        } else {
            StatusHud::Draw();
            if (Menu_Enabled)
                open_alpha = ImClamp(open_alpha + (2.f * ImGui::GetIO( ).DeltaTime * (toggled ? 1.5f : -1.5f)), 0.f, 1.f);
            else
                open_alpha = 0;

            if (open_alpha > 0.01f) {
                DrawPhantomMenu( );
            }
        }
        { Performance::Scope timing(Performance::OreSim); OreSim::Render(); }
        { Performance::Scope timing(Performance::PlayerESP); PlayerESP::Render(); }
        UtilitySuite::Render();
    }
} // namespace Menu

