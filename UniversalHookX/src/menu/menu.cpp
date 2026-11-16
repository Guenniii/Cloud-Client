#include "../utils/lifecycle/lifecycle.hpp"
#include "../utils/theme/theme.hpp"
#include "../utils/esp/echo_trail.hpp"
#include "menu.hpp"
#include "../input/utility_suite.hpp"
#include "../input/visual_modules.hpp"
#include "../input/friends.hpp"
#include "status_hud.hpp"
#include "target_hud.hpp"
#include "performance.hpp"
#include "login.hpp"
#include "../input/key_mapping.hpp"
#include "resources.hpp"
#include "module_registry.hpp"
#include "history.hpp"
#include "widgets.hpp"
#include "navigation_layout.hpp"
#include "palette.hpp"
#include "pages.hpp"
#include "clickgui.hpp"
#include "orbit.hpp"
#include "module_view.hpp"
#include "key_names.hpp"
#include "../base.hpp"
#include "../console/console.hpp"
#include "../utils/config/config.hpp"
#include <cmath>
#include <algorithm>
#include <cctype>
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
#include "../utils/seedcracker/shipwreck_esp.hpp"
#include "../utils/oresim/oresim.hpp"
#include "../utils/esp/player_esp.hpp"

#include "../modules/settings.hpp"

#pragma warning(disable : 4244)
#pragma warning(disable : 4005)

namespace ig = ImGui;
using namespace Menu::Widgets;
using Menu::GetKeyName;
using namespace Menu::ModuleView; // shared categories/filter/helpers, see module_view.hpp

// ===== UI STATE VARIABLES =====
static bool toggled = true;
static float open_alpha = 0.0f;

// SettingType / ModuleSetting / ModuleData sind jetzt in config.hpp definiert,
// damit config.cpp sie ebenfalls verwenden kann.

// Persistente Expand-States (auÃƒÅ¸erhalb der Funktion, damit sie ÃƒÂ¼ber Frames erhalten bleiben)
static std::unordered_map<int, bool> card_expanded;
static std::unordered_map<int, float> card_expand_anim;

// --- Icon Placeholders ---
// Replace these with your actual icon font definitions (e.g., FontAwesome)
#define ICON_FA_PUZZLE "P"
#define ICON_FA_SAVE "D"
#define ICON_FA_SLIDERS "O"
#define ICON_FA_CHEVRON_D "v"
#define ICON_FA_USER "U"
#define ICON_FA_CLOCK "C"


// ZeilenhÃƒÂ¶he je nach Setting-Typ (fÃƒÂ¼r die Expand-Animation)
static float SettingRowHeight(const ModuleSetting& s) {
    return s.type == SettingType::SliderInt ? 52.0f : 32.0f;
}

// Index des Moduls, das gerade auf einen Tastendruck wartet (-1 = keine Aufnahme aktiv)
static auto& capturing_module = Config::capturing_module_key;

// "Seiten" im Hauptfenster: 0-3 sind die Modul-Kategorien, 4-8 die Client-Seiten (siehe PAGE_* in module_view.hpp).
// Alles laeuft ueber denselben Crossfade-Mechanismus wie ein normaler Kategorie-Switch.
// Name and logo are customized by the user; preserve the visible branding.
// A stable internal window ID keeps dragging independent of display-name edits.
constexpr const char* MENU_WINDOW_ID = "###client_menu";

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

            // theme accent at half alpha (follows presets, custom and animated accent)
            ImGui::GetWindowDrawList( )->AddCircleFilled(partile_pos[i], partile_radius[i], ColA(Menu::Palette::Accent( ), 0.5f * ImGui::GetStyle( ).Alpha));
        }
    }

    void DrawPhantomMenu( ) {
        auto modules = Registry::GetModules();
        //       auf Namespace-Ebene definiert, siehe oben) =====

        static int active_page = PAGE_COMBAT;   // 0-2 = Kategorie-Tabs, 3 = Config, 4 = Settings
        static int last_category = PAGE_COMBAT; // merkt sich den zuletzt gewaehlten Tab (fuer die Pill-Position)
        static int trans_from = -1, trans_to = -1;
        static float trans_t = -1.0f; // -1 = keine Transition
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const float menu_scale = Config::menu_size_percent.load() / 100.0f;
        const float main_w = (std::min)(960.0f * menu_scale, (std::max)(1.0f, display.x - 32.0f));
        const float main_h = (std::min)(650.0f * menu_scale, (std::max)(1.0f, display.y - 32.0f));
        static char module_search[96] = {};
        static std::string previous_search;
        static bool favorites_only = false;
        static bool active_only = false;
        // Window settings to mimic the borderless rounded design
        ImGui::SetNextWindowSize(ImVec2(main_w, main_h));
        ImVec2 menu_pos((display.x - main_w) * 0.5f, (display.y - main_h) * 0.5f);
        if (const ImGuiWindow* previous = ImGui::FindWindowByName(MENU_WINDOW_ID))
            menu_pos = ImVec2(ImClamp(previous->Pos.x, 0.0f, (std::max)(0.0f, display.x - main_w)),
                              ImClamp(previous->Pos.y, 0.0f, (std::max)(0.0f, display.y - main_h)));
        ImGui::SetNextWindowPos(menu_pos);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 24));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, Menu::Palette::Background());

        ImGui::Begin(MENU_WINDOW_ID, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::SetWindowFontScale(Config::font_size_percent.load() / 100.0f);
        ImDrawList* draw_list = ImGui::GetWindowDrawList( );
        if(Theme::gradient_enabled.load()) {
            const ImVec2 pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
            const int first=draw_list->VtxBuffer.Size;
            const ImU32 top=Theme::Current().background,bottom=Theme::GradientEnd();
            draw_list->AddRectFilled(pos,ImVec2(pos.x+size.x,pos.y+size.y),top,16.0f);
            ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw_list,first,draw_list->VtxBuffer.Size,pos,ImVec2(pos.x,pos.y+size.y),top,bottom);
        }
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
        ImGui::Text("Cloud");
        ImGui::PopStyleColor( );

        ImGui::SetCursorPos(ImVec2(65, 40));
        ImGui::PushStyleColor(ImGuiCol_Text, Menu::Palette::Muted());
        ImGui::Text("Alpha");
        ImGui::PopStyleColor( );

        // ==========================================
        // Tab Bar (Category Selector)
        // ==========================================
        std::array<float,9> tab_widths{},icon_widths{};
        int cat_counts[4] = {};
        ModuleView::CountEnabledPerCategory(modules, cat_counts);
        float count_badge_width[4] = {};
        for(int c=0;c<9;c++){
            const float icon_width=icons->CalcTextSizeA(15,FLT_MAX,0,categories[c].icon).x;
            icon_widths[c]=icon_width+16;
            float padding=20;
            if(categories[c].page<4){
                char count[8];std::snprintf(count,sizeof(count),"%d",cat_counts[categories[c].page]);
                count_badge_width[categories[c].page]=(std::max)(18.f,ImGui::CalcTextSize(count).x+10);
                padding+=8+count_badge_width[categories[c].page];
            }
            tab_widths[c]=icon_width+6+ImGui::CalcTextSize(categories[c].label).x+padding;
        }
        const auto navigation=Navigation::Arrange(tab_widths,window_width-48,icon_widths);
        const float summary_y=68+navigation.height+12,content_y=68+navigation.height+40;
        ImGui::SetCursorPos(ImVec2(24,68));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        ImGui::BeginChild("CategoryNavigation",ImVec2(window_width-48,navigation.height),false,
            ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        draw_list=ImGui::GetWindowDrawList();
        const auto tab_p=ImGui::GetCursorScreenPos();
        const auto& tab_rects=navigation.tabs;
        const float pill_h=36,tab_h=28;
        for(const auto& group:navigation.groups)
            draw_list->AddRectFilled(ImVec2(tab_p.x+group.x,tab_p.y+group.y),
                ImVec2(tab_p.x+group.x+group.w,tab_p.y+group.y+group.h),Menu::Palette::Soft(),18);

        static float highlight_local_x = 0.0f, highlight_local_w = 0.0f;
        static bool highlight_init = false;

        const int selected_page = trans_t >= 0.0f ? trans_to : active_page;
        int selected_tab = 0;
        for (int c = 0; c < 9; ++c) if (categories[c].page == selected_page) selected_tab = c;
        const auto target = tab_rects[selected_tab];
        static int highlight_group = -1;
        static float highlight_local_y=0,last_navigation_width=-1,last_navigation_height=-1;
        const int selected_group = selected_tab < 4 ? 0 : 1;
        if (!highlight_init || highlight_group != selected_group || last_navigation_width!=window_width || last_navigation_height!=navigation.height) {
            highlight_local_x = target.x;
            highlight_local_y = target.y;
            last_navigation_width=window_width;last_navigation_height=navigation.height;
            highlight_local_w = target.w;
            highlight_init = true;
            highlight_group = selected_group;
        }
        highlight_local_y += (target.y-highlight_local_y)*ImSaturate(ImGui::GetIO().DeltaTime*12);
        highlight_local_x += (target.x - highlight_local_x) * ImSaturate(ImGui::GetIO( ).DeltaTime * 12.0f);
        highlight_local_w += (target.w - highlight_local_w) * ImSaturate(ImGui::GetIO( ).DeltaTime * 12.0f);

        float highlight_x = highlight_local_x + tab_p.x;
        const float highlight_y=highlight_local_y+tab_p.y;
        float highlight_w = highlight_local_w;

        draw_list->AddRectFilled(ImVec2(highlight_x, highlight_y), ImVec2(highlight_x + highlight_w, highlight_y + tab_h), Menu::Palette::Card(), 14.0f);

        // Short separators centered in the gaps within each navigation bubble.
        for (int c = 1; c < 9; ++c) {
            if(c==4||tab_rects[c].y!=tab_rects[c-1].y)continue;
            const float separator_x=tab_p.x+tab_rects[c].x-navigation.gap*.5f;
            const float separator_y=tab_p.y+tab_rects[c].y-4;
            draw_list->AddRectFilled(ImVec2(separator_x - 1.0f, separator_y + 8.0f),
                                    ImVec2(separator_x + 1.0f, separator_y + pill_h - 8.0f),
                                    ColA(Menu::Palette::Muted(), 0.85f), 1.0f);
        }

        for (int c = 0; c < 9; c++) {
            bool is_active = (categories[c].page == selected_page);
            ImU32 col = is_active ? Menu::Palette::Accent() : Menu::Palette::Muted();

            float screen_x = tab_rects[c].x + tab_p.x;
            const float tab_y=tab_rects[c].y+tab_p.y;

            const float icon_font_size=15*navigation.iconScale;
            const auto icon_size=icons->CalcTextSizeA(icon_font_size,FLT_MAX,0,categories[c].icon);
            const bool icon_only=navigation.iconsOnly[c];
            const float inset=icon_only?(tab_rects[c].w-icon_size.x)*.5f:navigation.padding;
            draw_list->AddText(icons,icon_font_size,ImVec2(screen_x+inset,tab_y+(tab_h-icon_font_size)*.5f),col,categories[c].icon);
            if(!icon_only)
                draw_list->AddText(ImVec2(screen_x+inset+icon_size.x+6,tab_y+(tab_h-ImGui::GetFontSize())*.5f),col,categories[c].label);

            if (!icon_only && categories[c].page < 4 && cat_counts[categories[c].page] > 0) {
                const float badge_w = count_badge_width[categories[c].page];
                const float badge_x = screen_x + tab_rects[c].w - navigation.padding - badge_w;
                const float badge_y = tab_y + (tab_h - 18.0f) * 0.5f;
                draw_list->AddRectFilled(ImVec2(badge_x, badge_y), ImVec2(badge_x + badge_w, badge_y + 18.0f), Menu::Palette::Accent(), 9.0f);
                char count_buf[8];
                snprintf(count_buf, sizeof(count_buf), "%d", cat_counts[categories[c].page]);
                const ImVec2 count_size = ImGui::CalcTextSize(count_buf);
                draw_list->AddText(ImVec2(badge_x + (badge_w - count_size.x) * 0.5f,
                    badge_y + (18.0f - count_size.y) * 0.5f), Theme::Current().onAccent, count_buf);
            }

            ImGui::SetCursorScreenPos(ImVec2(screen_x, tab_y));
            ImGui::PushID(c);
            ImGui::InvisibleButton("##cat_tab", ImVec2(tab_rects[c].w, tab_h));
            if (ImGui::IsItemClicked( ) && categories[c].page != active_page && trans_t < 0.0f) {
                trans_from = active_page;
                trans_to = categories[c].page;
                trans_t = 0.0f;
            }
            ImGui::PopID( );
        }

        ImGui::SetCursorPos(ImVec2(0,navigation.height-1));
        ImGui::Dummy(ImVec2(1,1));
        ImGui::EndChild();
        ImGui::PopStyleVar();
        draw_list = ImGui::GetWindowDrawList();
        ImVec2 win_pos = ImGui::GetWindowPos();

        // ==========================================
        // 2. MODULES SECTION (Cards)
        // ==========================================

        // ===== Hotkey-Aufnahme: wartet auf die nÃƒÂ¤chste gedrÃƒÂ¼ckte Taste =====
        ImGui::SetCursorPos(ImVec2(24.0f, content_y));

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
                if (trans_to < 4)
                    last_category = trans_to;
            } else {
                trans_t = -1.0f;
                active_page = trans_to;
                if (trans_to < 4)
                    last_category = trans_to;
                display_page = active_page;
                content_alpha = 1.0f;
            }
        }

        if (display_page < 4) {
            const float search_w = (std::min)(240.0f, (std::max)(120.0f, window_width - 576.0f));
            const float filter_w = (std::min)(112.0f, (std::max)(80.0f, (window_width - 24.0f - search_w - 180.0f - 24.0f) / 4.0f));
            ImGui::SetCursorPos(ImVec2(180.0f, 24.0f));
            if (DrawRoundedButton(draw_list, "##favorites_filter", "Favorites", ImVec2(filter_w, 32.0f),
                    favorites_only ? Menu::Palette::Soft() : Menu::Palette::Card(),
                    favorites_only ? Menu::Palette::Accent() : Menu::Palette::Border(),
                    favorites_only ? Menu::Palette::Accent() : Menu::Palette::Muted(), 1.0f, 8.0f))
                favorites_only = !favorites_only;
            ImGui::SetCursorPos(ImVec2(180.0f + filter_w + 8.0f, 24.0f));
            if (DrawRoundedButton(draw_list, "##active_filter", "Only active", ImVec2(filter_w, 32.0f),
                    active_only ? Menu::Palette::Soft() : Menu::Palette::Card(),
                    active_only ? Menu::Palette::Accent() : Menu::Palette::Border(),
                    active_only ? Menu::Palette::Accent() : Menu::Palette::Muted(), 1.0f, 8.0f))
                active_only = !active_only;
            ImGui::SetCursorPos(ImVec2(180.0f + (filter_w + 8.0f) * 2.0f, 24.0f));
            if (DrawRoundedButton(draw_list,"##collapse_all","Collapse",ImVec2(filter_w,32.0f),
                    Menu::Palette::Card(),Menu::Palette::Border(),Menu::Palette::Muted(),1.0f,8.0f)) {
                for(auto& [id, expanded] : card_expanded) expanded=false;
            }
            ImGui::SetCursorPos(ImVec2(180.0f + (filter_w + 8.0f) * 3.0f, 24.0f));
            ImGui::BeginDisabled(!History::CanUndo());
            if (DrawRoundedButton(draw_list,"##undo_menu","Undo",ImVec2(filter_w,32.0f),
                    Menu::Palette::Card(),Menu::Palette::Border(),Menu::Palette::Accent(),1.0f,8.0f)) History::Undo();
            ImGui::EndDisabled();
            ImGui::SetCursorPos(ImVec2(window_width - 24.0f - search_w, 24.0f));
            ImGui::SetNextItemWidth(search_w);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 8.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, Menu::Palette::Card());
            ImGui::PushStyleColor(ImGuiCol_Text, Menu::Palette::Text());
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, Menu::Palette::Muted());
            ImGui::InputTextWithHint("##module_search", "Search modules and settings...", module_search, sizeof(module_search));
            ImGui::PopStyleColor(3); ImGui::PopStyleVar(2);
            // built after the filter buttons above so a click this frame is already reflected
            const auto stats = ModuleView::CountScope(modules, ModuleView::Filter{module_search, favorites_only, active_only, display_page});
            char summary[64]; snprintf(summary, sizeof(summary), "%d / %d modules  -  %d enabled", stats.shown, stats.total, stats.enabled);
            const ImVec2 summary_size = ImGui::CalcTextSize(summary);
            draw_list->AddText(ImVec2(win_pos.x + window_width - 24.0f - summary_size.x, win_pos.y + summary_y),
                              ColA(Menu::Palette::Muted(), content_alpha), summary);
        }
        const ModuleView::Filter filter{module_search, favorites_only, active_only, display_page};
        const char* title = kPageTitles[display_page];
        if (display_page < 4) {
            if (module_search[0]) title = favorites_only ? "Search favorites - all categories" : "Search results - all categories";
            else if (favorites_only) title = "Favorite modules";
        }
        draw_list->AddText(ImVec2(win_pos.x + 24.0f, win_pos.y + summary_y), ColA(Menu::Palette::Label(), content_alpha), title);
        if (display_page >= 4) {
            ImGui::SetCursorPos(ImVec2(window_width - 280.0f,24.0f));
            ImGui::BeginDisabled(!History::CanUndo());
            if(DrawRoundedButton(draw_list,"##undo_page","Undo",ImVec2(84.0f,32.0f),
                    Menu::Palette::Card(),Menu::Palette::Border(),Menu::Palette::Accent(),1.0f,8.0f)) History::Undo();
            ImGui::EndDisabled();
            ImGui::SetCursorPos(ImVec2(window_width - 184.0f, 24.0f));
            if (DrawRoundedButton(draw_list, "##back_modules", "Back to modules", ImVec2(160.0f, 32.0f),
                    Menu::Palette::Soft(), Menu::Palette::Border(), Menu::Palette::Accent(), 1.0f, 8.0f)
                    && trans_t < 0.0f) {
                trans_from=active_page; trans_to=last_category; trans_t=0.0f;
            }
        }
        const bool search_changed = previous_search != module_search;
        previous_search = module_search;
        ImGui::SetCursorPos(ImVec2(24.0f, content_y));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, content_alpha);

        if (display_page < 4) {
            const float cardsHeight = win_pos.y + window_height - 24.0f - footer_reserved_h - 12.0f - ImGui::GetCursorScreenPos().y;
            // Keep a fixed scrollbar gutter so expanding a module never changes card widths.
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,11.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,6.f);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,ColV(Menu::Palette::Soft(),content_alpha*.35f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,ColV(Menu::Palette::Muted(),content_alpha*.55f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,ColV(Menu::Palette::Accent(),content_alpha*.8f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,ColV(Menu::Palette::Accent(),content_alpha));
            ImGui::BeginChild("ModuleCards", ImVec2(window_width - 48.0f, (std::max)(1.0f, cardsHeight)), false, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NoBackground);
            draw_list = ImGui::GetWindowDrawList();
            const std::vector<int> visible_modules = ModuleView::VisibleSorted(modules, filter);
            for (int i : visible_modules) {

                ImGui::PushID(i);

                ImVec2 card_p = ImGui::GetCursorScreenPos( );
                float card_w = ImGui::GetContentRegionAvail().x;
                float header_h = 66.0f;

                bool has_settings = !modules[i].settings_list.empty( );

                // --- Expand-Animation ---
                bool& expanded = card_expanded[i];
                if (search_changed && module_search[0] && !filter.Matches(modules[i].name) && filter.MatchesModule(modules[i])) expanded = true;
                float& anim = card_expand_anim[i];
                float target_h = 0.0f;
                for (auto& s : modules[i].settings_list)
                    target_h += SettingRowHeight(s);
                if (modules[i].enabled == &OreSim_Enabled) target_h += ORESIM_SEED_ROW_HEIGHT;
                if (has_settings)
                    target_h += 56.0f;
                float target_anim = expanded ? target_h : 0.0f;
                anim += (target_anim - anim) * ImSaturate(ImGui::GetIO( ).DeltaTime * 10.0f);

                float card_h = header_h + anim;

                // --- Card Background & Border ---
                draw_list->AddRectFilled(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), ColA(Menu::Palette::Card(), content_alpha), 12.0f);
                draw_list->AddRect(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), ColA(Menu::Palette::Border(), content_alpha), 12.0f, 0, 1.0f);

                if (*modules[i].enabled)
                    draw_list->AddRectFilled(ImVec2(card_p.x, card_p.y + 10), ImVec2(card_p.x + 4, card_p.y + header_h - 10), ColA(Menu::Palette::Accent(), content_alpha), 2.0f);

                const float label_x = card_p.x + 60.0f;
                const ImVec2 icon_min(card_p.x + 16.0f, card_p.y + (header_h - 32.0f) * 0.5f);
                const char* module_icon = ModuleIcon(modules[i]);
                const ImVec2 icon_size = icons->CalcTextSizeA(16.0f, FLT_MAX, 0.0f, module_icon);
                draw_list->AddRectFilled(icon_min, ImVec2(icon_min.x + 32.0f, icon_min.y + 32.0f),
                                        ColA(Menu::Palette::Soft(), content_alpha), 9.0f);
                draw_list->AddText(icons, 16.0f,
                    ImVec2(icon_min.x + (32.0f - icon_size.x) * 0.5f, icon_min.y + (32.0f - icon_size.y) * 0.5f),
                    ColA(*modules[i].enabled ? Menu::Palette::Accent() : Menu::Palette::Label(), content_alpha), module_icon);
                draw_list->AddText(ImVec2(label_x, card_p.y + 12), ColA(Menu::Palette::Text(), content_alpha), modules[i].name);

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
                ImVec2 badge_min = ImVec2(label_x + name_size.x + 10.0f, card_p.y + 11.0f);
                ImVec2 badge_max = ImVec2(badge_min.x + badge_w, badge_min.y + badge_h);

                ImU32 badge_bg = is_capturing ? Menu::Palette::Soft() : Menu::Palette::Background();
                const bool hotkey_conflict = Registry::HasConflict(modules[i].hotkey, i);
                ImU32 badge_text = is_capturing ? Menu::Palette::Accent() : (modules[i].hotkey != 0 ? Menu::Palette::Text() : Menu::Palette::Muted());

                draw_list->AddRectFilled(badge_min, badge_max, ColA(badge_bg, content_alpha), 4.0f);
                if (is_capturing || hotkey_conflict)
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

                if (hotkey_conflict) {
                    draw_list->AddText(ImVec2(badge_max.x + 8.0f, badge_min.y + 1.0f), ColA(Menu::Palette::Accent(), content_alpha), "Key conflict");
                    if (ImGui::IsMouseHoveringRect(badge_min, ImVec2(badge_max.x + 90.0f, badge_max.y))) {
                        ModuleView::DrawConflictTooltip(modules, i);
                    }
                }

                char sub_text[64];
                snprintf(sub_text, sizeof(sub_text), "%d settings", (int)modules[i].settings_list.size( ));
                draw_list->AddText(ImVec2(label_x, card_p.y + 35), ColA(Menu::Palette::Muted(), content_alpha), sub_text);

                ImVec2 sub_size = ImGui::CalcTextSize(sub_text);
                if (has_settings)
                    draw_list->AddText(ImVec2(label_x + sub_size.x + 5, card_p.y + 35), ColA(Menu::Palette::Muted(), content_alpha), expanded ? "^" : ICON_FA_CHEVRON_D);

                if (modules[i].active_settings > 0) {
                    draw_list->AddCircleFilled(ImVec2(label_x + sub_size.x + 20, card_p.y + 42), 2.0f, ColA(Menu::Palette::Muted(), content_alpha));
                    char active_text[64];
                    snprintf(active_text, sizeof(active_text), "%d active", modules[i].active_settings);
                    draw_list->AddText(ImVec2(label_x + sub_size.x + 30, card_p.y + 35), ColA(Menu::Palette::Accent(), content_alpha), active_text);
                }

                // --- Klickbereich: nur der Header klappt die Card auf/zu ---
                float left_w = badge_min.x - card_p.x;
                ImGui::SetCursorScreenPos(card_p);
                if (has_settings && left_w > 1.0f) {
                    if (ImGui::InvisibleButton("##card_click_l", ImVec2(left_w, header_h)))
                        expanded = !expanded;
                }

                float right_start_x = badge_max.x;
                float right_w = (card_p.x + card_w - 100.0f) - right_start_x;
                if (has_settings && right_w > 1.0f) {
                    ImGui::SetCursorScreenPos(ImVec2(right_start_x, card_p.y));
                    if (ImGui::InvisibleButton("##card_click_r", ImVec2(right_w, header_h)))
                        expanded = !expanded;
                }

                // Favorite control has its own hit area, separate from expansion and enable.
                const ImVec2 favorite_pos(card_p.x + card_w - 96.0f, card_p.y + 17.0f);
                ImGui::SetCursorScreenPos(favorite_pos);
                if (ImGui::InvisibleButton("##favorite", ImVec2(28.0f, 32.0f)))
                    modules[i].favorite = !modules[i].favorite;
                draw_list->AddText(icons, 16.0f, ImVec2(favorite_pos.x + 6.0f, favorite_pos.y + 8.0f),
                    ColA(modules[i].favorite || ImGui::IsItemHovered() ? Menu::Palette::Accent() : Menu::Palette::Muted(), content_alpha), ICON_FA_STAR);

                // --- Toggle Switch rechts (Enable/Disable, unabhÃƒÂ¤ngig vom Expand) ---
                float card_toggle_w = 40.0f, card_toggle_h = 22.0f;
                float card_toggle_margin = 20.0f;
                ImGui::SetCursorScreenPos(ImVec2(card_p.x + card_w - card_toggle_margin - card_toggle_w, card_p.y + header_h * 0.5f - card_toggle_h * 0.5f));
                CustomToggle("##mod_toggle", modules[i].enabled, content_alpha, card_toggle_w, card_toggle_h);

                // --- Settings-Inhalt rendern, sobald sichtbar ---
                if (anim > 1.0f) {
                    draw_list->AddLine(ImVec2(card_p.x + 20.0f, card_p.y + header_h),
                                      ImVec2(card_p.x + card_w - 20.0f, card_p.y + header_h),
                                      ColA(Menu::Palette::Border(), content_alpha));
                    draw_list->PushClipRect(card_p, ImVec2(card_p.x + card_w, card_p.y + card_h), true);

                    float sy = card_p.y + header_h + 8.0f;
                    draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(card_p.x + 20.0f, sy + 3.0f),
                        ColA(Menu::Palette::Muted(), content_alpha), Registry::Description(modules[i].name), nullptr, card_w - 164.0f);
                    ImGui::SetCursorScreenPos(ImVec2(card_p.x + card_w - 122.0f, sy));
                    if (DrawRoundedButton(draw_list, "##reset_module", "Reset", ImVec2(102.0f, 28.0f),
                            Menu::Palette::Soft(), Menu::Palette::Border(), Menu::Palette::Accent(), content_alpha, 6.0f)) {
                        ModuleView::ResetModule(modules, i);
                    }
                    sy += 42.0f;
                    float row_w = card_w - 40.0f;
                    if (modules[i].enabled == &OreSim_Enabled) {
                        ImGui::SetCursorScreenPos(ImVec2(card_p.x + 20, sy));
                        DrawOreSimSeedInput(draw_list, ImVec2(card_p.x + 20, sy), row_w, content_alpha);
                        sy += ORESIM_SEED_ROW_HEIGHT;
                    }
                    for (auto& s : modules[i].settings_list) {
                        if (module_search[0] && filter.Matches(s.DisplayName())) {
                            std::string label(s.DisplayName()), query(module_search);
                            auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
                            std::transform(label.begin(), label.end(), label.begin(), lower);
                            std::transform(query.begin(), query.end(), query.begin(), lower);
                            const size_t match = label.find(query);
                            const float label_y = sy + (s.type == SettingType::SliderInt ? 0.0f : 5.0f);
                            const float match_x = card_p.x + 20.0f + ImGui::CalcTextSize(s.DisplayName(), s.DisplayName() + match).x;
                            const float match_w = ImGui::CalcTextSize(s.DisplayName() + match, s.DisplayName() + match + query.size()).x;
                            draw_list->AddRectFilled(ImVec2(match_x - 3.0f, label_y - 2.0f),
                                ImVec2(match_x + match_w + 3.0f, label_y + ImGui::GetFontSize() + 2.0f),
                                ColA(Menu::Palette::Soft(), content_alpha), 4.0f);
                        }
                        ImGui::PushID(s.DisplayName());
                        switch (s.type) {
                            case SettingType::Checkbox:
                                DrawCheckboxRow(draw_list, ImVec2(card_p.x + 20, sy), row_w, s.DisplayName(), s.b_val, content_alpha);
                                break;
                            case SettingType::ColorRGB:
                                DrawColorRow(draw_list, ImVec2(card_p.x + 20, sy), row_w, s.DisplayName(), s.i_val, content_alpha);
                                break;
                            case SettingType::SliderInt:
                                DrawSliderRow(draw_list, ImVec2(card_p.x + 20, sy), row_w, s.DisplayName(), s.i_val, s.i_min, s.i_max, s.suffix, content_alpha);
                                break;
                        }
                        if (ImGui::IsMouseHoveringRect(ImVec2(card_p.x + 20.0f, sy), ImVec2(card_p.x + 20.0f + row_w * 0.45f, sy + 22.0f))) {
                            const char* help = ModuleView::SettingHelp(s);
                            if (help) ImGui::SetTooltip("%s", help);
                        }
                        ImGui::PopID( );
                        sy += SettingRowHeight(s);
                    }

                    draw_list->PopClipRect( );
                }

                ImGui::SetCursorScreenPos(ImVec2(card_p.x, card_p.y + card_h + 14.0f));
                ImGui::PopID( );
            }
            const bool has_match = !visible_modules.empty();
            if (!has_match) {
                ImGui::PushStyleColor(ImGuiCol_Text, Menu::Palette::Muted());
                ImGui::TextUnformatted(favorites_only ? "No matching favorites. Pin modules using the star on their card." :
                    active_only ? "No active modules match the selected filters and search." : "No matching modules or settings.");
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(1,1));
            ImGui::EndChild();
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
            draw_list = ImGui::GetWindowDrawList();
        } else {
            // ==========================================
            // Config- / Settings-Seite (wie eine normale Kategorie-Seite gerendert)
            // ==========================================
            ImVec2 page_area_min = ImGui::GetCursorScreenPos( );
            float page_area_w = window_width - 48.0f;
            float footer_top_y = win_pos.y + window_height - 24.0f - footer_reserved_h;
            float page_area_h = (footer_top_y - 12.0f) - page_area_min.y;

            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,11.f);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,6.f);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,ColV(Menu::Palette::Soft(),content_alpha*.35f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,ColV(Menu::Palette::Muted(),content_alpha*.55f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,ColV(Menu::Palette::Accent(),content_alpha*.8f));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,ColV(Menu::Palette::Accent(),content_alpha));

            ModuleView::DrawClientPage(display_page, draw_list, page_area_min, page_area_w, page_area_h, content_alpha, modules);
            ImGui::PopStyleColor(4);
            ImGui::PopStyleVar(2);
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
        if (Login::HasBeta()) {
            draw_list->AddRectFilled(badge_p2, ImVec2(badge_p2.x + badge_w2, badge_p2.y + badge_h2), Menu::Palette::Soft(), badge_h2 * 0.5f);
            draw_list->AddText(ImVec2(badge_p2.x + badge_padding_x2, badge_p2.y + 0.8f), Menu::Palette::Accent(), "BETA");
        }

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
        // im Content-Bereich gerendert (siehe display_page < 4 Verzweigung).
        // ==========================================

        if (Config::particles_enabled)
            Particles( );
        History::EndFrame();
        ImGui::End( );
        ImGui::PopStyleColor( );
        ImGui::PopStyleVar(2);
    }

    void Shutdown(bool vulkanTextures) {
        Lifecycle::Trace("Menu: stop Java features");
        Menu_Enabled=false;
        TargetHUD_Enabled=false;
        SeedCracker_Enabled=false;OreSim_Enabled=false;PlayerESP_Enabled=false;EchoTrail_Enabled=false;LootBeacon_Enabled=false;
        EchoTrail::Clear();
        ProjectilePrediction_Enabled=false;SoundESP_Enabled=false;StorageESP_Enabled=false;ItemESP_Enabled=false;ExplosionPreview_Enabled=false;
        AutoArmor_Enabled=false;Refill_Enabled=false;HitEffect_Enabled=false;PredictDoubleHand_Enabled=false;ShieldBreaker_Enabled=false;TriggerBot_Enabled=false;
        if(auto env=SeedCracker::GetMinecraftJNIEnv()) {
            ShipwreckESP::Shutdown(env);
            TargetHud::Shutdown(env);
            EchoTrail::Shutdown(env);
            VisualModules::Clear(env,true);
            Friends::Clear(env);
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
        if(Menu_Enabled && Login::IsLoggedIn()) History::BeginFrame(); else History::Suspend();
        Registry::CaptureHotkey(); // LÃƒÂ¤uft immer, auch bei geschlossenem MenÃƒÂ¼

        if (!Login::IsLoggedIn())
            Login::Render(Menu_Enabled, Particles);
        // re-checked: the frame Login::Render flips logged_in already draws the menu (no blank frame after the morph)
        if (Login::IsLoggedIn()) {
            StatusHud::Draw();
            if (Menu_Enabled && Config::menu_style == 2 && Orbit::TakesLoginHandoff())
                open_alpha = 1.0f;
            else if (Menu_Enabled)
                open_alpha = ImClamp(open_alpha + (2.f * ImGui::GetIO( ).DeltaTime * (toggled ? 1.5f : -1.5f)), 0.f, 1.f);
            else
                open_alpha = 0;

            if (open_alpha > 0.01f) {
                // Menu selection (Config::menu_style, saved in the profile): 0 classic, 1 Click GUI, 2 Orbit (default).
                // All three are always compiled; the style is read once per frame, so a change made inside the settings
                // page takes effect on the next frame.
                static void (*const kMenus[])() = { DrawPhantomMenu, ClickGui::Draw, Orbit::Draw };
                const int style = Config::menu_style;
                kMenus[(style >= 0 && style <= 2) ? style : 1]( );
            }
            TargetHud::Render();
        }
        Friends::Sync();Friends::PollMiddleClick();
        { Performance::Scope timing(Performance::OreSim); OreSim::Render(); }
        { Performance::Scope timing(Performance::PlayerESP); PlayerESP::Render(); }
        ShipwreckESP::Render();
        EchoTrail::Render();
        UtilitySuite::Render();
        VisualModules::Render();
    }
} // namespace Menu

