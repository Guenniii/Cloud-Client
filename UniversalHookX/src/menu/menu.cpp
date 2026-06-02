#include "menu.hpp"
#include <thread>
#include "../console/console.hpp"
// ImGui
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_impl_win32.h"
#include "../dependencies/imgui/imgui_internal.h"
#include "../dependencies/imgui/imgui_impl_vulkan.h"
#include "../utils/imageloader.hpp"
#include "../dependencies/font/IconsFontAwesome5.h"
#include "../dependencies/font/fa-solid-900.h"
//auth
#include "../auth/Authclient.h"
#include "../auth/HWID.h"
#include "../auth/Hash.h"

//Byte Arrays
#include "../dependencies/images/bytearray.h"
#include "../modules/settings.hpp"


#pragma warning(disable : 4244)

namespace ig = ImGui;

// ===== UI STATE VARIABLES =====
static int m_tabs = 0;
static bool toggled = true;
static float open_alpha = 0.0f;

// Login variables
char username[64];
char password[64];
std::string msg = "";
static bool show_password = false;
static float login_alpha = 0.0f;
static std::string last_logged_user = "";
static bool loginInProgress = false;
static std::string responseText = "";
static std::string g_licenseDays = "PERMANENT";
static std::string g_licenseExpiry = "";
static std::string days = "Days left";
static bool logged_in = false;

//Fonts
ImFont* tab_title;
ImFont* font_icon;
ImFont* poppins;
ImFont* icons;


static MyTextureData logo;
static MyTextureData userlogo;

void PulsingText(const char* text) {
    // Erzeugt einen Wert, der sauber zwischen 0.3 und 1.0 hin und her schwingt
    float frequency = 3.0f; // Wie schnell wird gepulst
    float alpha = 0.65f + 0.35f * sinf(ImGui::GetTime( ) * frequency);

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, alpha));
    ImGui::Text(text);
    ImGui::PopStyleColor( );
}

void Menu::Images()

{
    static bool g_TextureInitialized = false;
    if (!g_TextureInitialized) {
        LoadTextureFromMemory(logo_two, sizeof(logo_two), &logo);
        LoadTextureFromMemory(user, sizeof(user), &userlogo);
        g_TextureInitialized = true;
        LOG("[+] Vulkan: Texture loaded.\n");
    }
}


namespace Menu {

    void SetupImGuiStyle( ) {
        // Eggplant style by yo-ru from ImThemes
        ImGuiStyle& style = ImGui::GetStyle( );
        ImGuiIO& io = ImGui::GetIO( );
        (void)io;

        style.Alpha = 1.0f;
        style.DisabledAlpha = 0.6f;
        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.WindowRounding = 10.0f;
        style.WindowBorderSize = 0;
        style.WindowMinSize = ImVec2(20.0f, 32.0f);
        style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
        style.WindowMenuButtonPosition = ImGuiDir_None;
        style.ChildRounding = 10.0f;
        style.ChildBorderSize = 1.0f;
        style.PopupRounding = 10.0f;
        style.PopupBorderSize = 1.0f;
        style.FramePadding = ImVec2(1, 0);
        style.FrameRounding = 3;
        style.FrameBorderSize = 0.0f;
        style.ItemSpacing = ImVec2(4.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(10.0f, 4.0f);
        style.CellPadding = ImVec2(4.0f, 4.0f);
        style.IndentSpacing = 20.0f;
        style.ColumnsMinSpacing = 4.0f;
        style.ScrollbarSize = 5;
        style.ScrollbarRounding = 3;
        style.GrabMinSize = 10.0f;
        style.GrabRounding = 10.0f;
        style.TabRounding = 6.0f;
        style.TabBorderSize = 0.0f;
        //    style.TabMinWidthForCloseButton = 0.0f;
        style.ColorButtonPosition = ImGuiDir_Right;
        style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
        style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

        style.Colors[ImGuiCol_Text] = ImVec4(0.9490196f, 0.9490196f, 0.9490196f, 1.0f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.9490196f, 0.9490196f, 0.9490196f, 0.34509805f);
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.47843137f, 0.34901962f, 0.45882353f, 1.0f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.47843137f, 0.34901962f, 0.45882353f, 1.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.47843137f, 0.34901962f, 0.45882353f, 1.0f);
        style.Colors[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.8197425f);
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.3019608f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.4f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.6f);
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 1.0f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.3019608f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.6f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.4f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.6f);
        style.Colors[ImGuiCol_Header] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.4549356f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.7811159f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.9019608f);
        style.Colors[ImGuiCol_Tab] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.8f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.84313726f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.81960785f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.8352941f);
        style.Colors[ImGuiCol_PlotLines] = ImVec4(0.9490196f, 0.9490196f, 0.9490196f, 1.0f);
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.47843137f, 0.34901962f, 0.45882353f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.60784316f, 0.42745098f, 0.5803922f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.60784316f, 0.42745098f, 0.5803922f, 0.639485f);
        style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 1.0f);
        style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.01716739f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.6039216f, 0.41960785f, 0.5764706f, 0.6f);
        style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.60784316f, 0.42745098f, 0.5803922f, 1.0f);
        style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.8f);
        style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.7019608f);
        style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.2f);
        style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.16078432f, 0.10980392f, 0.1764706f, 0.34901962f);

        // Load Fonts
        static const ImWchar ranges[] =
            {
                0x0020,
                0x00FF, // Basic Latin + Latin Supplement
                0x0400,
                0x052F, // Cyrillic + Cyrillic Supplement
                0x2DE0,
                0x2DFF, // Cyrillic Extended-A
                0xA640,
                0xA69F, // Cyrillic Extended-B
                0xE000,
                0xE226, // icons
                0,
            };

        // Font
        ImFontConfig font_config;
        font_config.PixelSnapH = false;
        font_config.OversampleH = 5;
        font_config.OversampleV = 5;
        font_config.RasterizerMultiply = 1.2f;

        static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};

        io.Fonts->AddFontFromMemoryTTF(poppin_font, sizeof(poppin_font), 16, &font_config, ranges);
        tab_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 19.0f, &font_config, ranges);
        font_icon = io.Fonts->AddFontFromMemoryTTF(icon_font, sizeof(icon_font), 25.0f, &font_config, ranges);
        poppins = io.Fonts->AddFontFromMemoryTTF(poppin_font, sizeof(poppin_font), 25.0f, &font_config, ranges);
        

        ImFontConfig icons_config;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = 25.0f;
        icons = io.Fonts->AddFontFromMemoryTTF(new_icons, sizeof(new_icons), 25.0f, &icons_config, icons_ranges);

        io.Fonts->Build( );
    }

        void InitializeContext(HWND hwnd) {
        if (ig::GetCurrentContext( ))
            return;

        ImGui::CreateContext( );
        ImGui_ImplWin32_Init(hwnd);

        ImGuiIO& io = ImGui::GetIO( );
        io.IniFilename = io.LogFilename = nullptr;

         SetupImGuiStyle( );  // Fonts & Style einmalig laden
        

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

    void Decoration( ) {
        auto draw = ImGui::GetWindowDrawList( );
        ImVec2 pos = ImGui::GetWindowPos( );

        draw->AddRectFilled(ImVec2(pos.x, pos.y), ImVec2(pos.x + 161, pos.y + 535), ImColor(32, 32, 32, int(255 * ImGui::GetStyle( ).Alpha)), 10.f, ImDrawCornerFlags_Left); // left bg
        draw->AddRect(ImVec2(pos.x, pos.y), ImVec2(pos.x + 161, pos.y + 535), ImColor(50, 50, 50, int(255 * ImGui::GetStyle( ).Alpha)), 10.f, ImDrawCornerFlags_Left, 1);    // left bg

        draw->AddRectFilled(ImVec2(pos.x + 160, pos.y), ImVec2(pos.x + 838, pos.y + 535), ImColor(26, 26, 26, int(255 * ImGui::GetStyle( ).Alpha)), 10.f, ImDrawCornerFlags_Right); // right bg
        draw->AddRect(ImVec2(pos.x + 160, pos.y), ImVec2(pos.x + 838, pos.y + 535), ImColor(50, 50, 50, int(255 * ImGui::GetStyle( ).Alpha)), 10.f, ImDrawCornerFlags_Right, 1);    // right bg

        ImGui::SetCursorPos(ImVec2(36, -7));
        ImGui::Image((ImTextureID)logo.DS, ImVec2(85, 98)); // logo 2
    }

    void user_info( ) {
        auto draw = ImGui::GetWindowDrawList( );
        ImVec2 pos = ImGui::GetWindowPos( );

        draw->AddRectFilled(ImVec2(pos.x + 9, pos.y + 486), ImVec2(pos.x + 152, pos.y + 523), ImColor(41, 41, 41, int(255 * ImGui::GetStyle( ).Alpha)), 5.f, ImDrawCornerFlags_All); // right bg
        draw->AddRect(ImVec2(pos.x + 9, pos.y + 486), ImVec2(pos.x + 152, pos.y + 523), ImColor(50, 50, 50, int(255 * ImGui::GetStyle( ).Alpha)), 5.f, ImDrawCornerFlags_All, 1);    // right bg

        if (g_licenseDays == "PERMANENT") {
            days = "";
        }
        if (g_licenseDays == "1") {
            days = "Day left";
        }
        draw->AddText(ImVec2(pos.x + 49, pos.y + 488), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), last_logged_user.c_str( ));
        draw->AddText(ImVec2(pos.x + 49, pos.y + 503), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), g_licenseDays.c_str( ));
        draw->AddText(ImVec2(pos.x + 59, pos.y + 503), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), days.c_str( ));
        ImGui::SetCursorPos(ImVec2(15, 490));
        ImGui::Image((ImTextureID)userlogo.DS, ImVec2(28, 28));
        ;
    }


    void login_tab( ) {


        auto draw = ImGui::GetWindowDrawList( );
        ImVec2 pos = ImGui::GetWindowPos( );
        ImVec2 size = ImGui::GetWindowSize( );

        // Background
        draw->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImColor(26, 26, 26, int(255 * login_alpha)), 10.f);
        draw->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImColor(50, 50, 50, int(255 * login_alpha)), 10.f, ImDrawCornerFlags_All, 1);

        // Title
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 40, 40));
        ImGui::Image((ImTextureID)logo.DS, ImVec2(85, 98));
        // Username 

        ImGui::SetCursorPos(ImVec2(size.x / 2 - 120, 150));
        ImGui::SetNextItemWidth(240);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.125f, 0.125f, 0.125f, 1.0f));
        ImGui::InputText("##username", username, sizeof(username), ImGuiInputTextFlags_CharsNoBlank);
        ImGui::PopStyleColor( );

        // Username label
        draw->AddText(poppins, 18, ImVec2(pos.x + size.x / 2 - 120, pos.y + 130), ImColor(150, 150, 150, int(255 * login_alpha)), "Username");

        // Password input
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 120, 210));
        ImGui::SetNextItemWidth(240);

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_Password;
        if (show_password)
            flags = 0;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.125f, 0.125f, 0.125f, 1.0f));
        ImGui::InputText("##password", password, sizeof(password), flags | ImGuiInputTextFlags_CharsNoBlank);
        ImGui::PopStyleColor( );
        // Password label

        draw->AddText(icons, 18, ImVec2(pos.x + size.x / 2 - 120, pos.y + 190), ImColor(150, 150, 150, int(255 * login_alpha)), ICON_FA_USER);
        PulsingText("Password");
        // Show/Hide password checkbox
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 100, 250));
        ImGui::Checkbox("Show Password", &show_password);

        // Login button
        ImGui::SetCursorPos(ImVec2(size.x / 2 - 100, 280));
        ImGui::SetNextItemWidth(200);
        if (loginInProgress) {
            ImGui::TextDisabled("Connecting...");
            
        } else if (ImGui::Button( " Login", ImVec2(200.0f, 40.0f))) {
            
            loginInProgress = true;
            responseText.clear( );
            g_licenseDays.clear( );   // ===== NEU: Alte License-Info löschen
            g_licenseExpiry.clear( ); // =====

            std::thread([&]( ) {
                // Hole die rohe Response vom Server
                std::string rawResponse = LoginRequestWithResponse(
                    username,
                    Sha256(password),
                    GetHWID( ));

                // ===== NEU: Parse die Response mit LicenseInfo =====
                LicenseInfo licenseInfo = ParseLoginResponse(rawResponse);

                if (licenseInfo.success) {
                    responseText = "OK";
                    // Speichere die License-Informationen in globalen Variablen
                    if (licenseInfo.daysLeft == -1)
                        g_licenseDays = "PERMANENT";
                    else
                        g_licenseDays = std::to_string(licenseInfo.daysLeft);

                    g_licenseExpiry = licenseInfo.expiryDate;
                } else {
                    // Bei Fehler: Error-Message anzeigen
                    responseText = licenseInfo.errorMessage;
                }
                // ===== ENDE NEU =====

                loginInProgress = false;
            }).detach( );
        }

        // ===== NEU: Response-Handling mit License-Info Display =====
        if (!responseText.empty( )) {
            ImVec4 color(1.f, 0.5f, 0.f, 1.f);
            std::string msg = responseText;

            if (responseText == "OK") {
                color = ImVec4(0.2f, 1.f, 0.4f, 1.f);
                msg = "Login successful";
                logged_in = true;
                last_logged_user = username;
            } else if (responseText == "FAIL")
                msg = "Invalid credentials";
            else if (responseText == "LICENSE_INVALID")
                msg = "No active license";
            else if (responseText == "HWID_MISMATCH")
                msg = "HWID mismatch";
            else if (responseText == "NO_LICENSE")
                msg = "No license activated";
            else if (responseText == "License expired")
                msg = "Your license has expired";
            else if (responseText == "User not Found")
                msg = "User not found";
            else if (responseText == "This User is banned")
                msg = "This account is banned";

            ImGui::SetCursorPos(ImVec2(size.x / 2 - 55, 340));
            ImGui::TextColored(color, "%s", msg.c_str( ));

            // ===== DISPLAY LICENSE-INFO =====
            if (!g_licenseDays.empty( )) {
                ImGui::Spacing( );
                ImGui::Separator( );
                ImGui::Spacing( );

                // License days
                ImGui::Text("License Information:");
                ImVec4 licenseColor(0.6f, 0.8f, 1.f, 1.f);

                if (g_licenseDays == "PERMANENT") {
                    licenseColor = ImVec4(0.2f, 1.f, 0.4f, 1.f); // Grün für permanent
                    ImGui::TextColored(licenseColor, "Status: PERMANENT");
                } else {
                    ImGui::TextColored(licenseColor, "Days Left: %s", g_licenseDays.c_str( ));
                }

                ImGui::TextColored(licenseColor, "Expires: %s", g_licenseExpiry.c_str( ));
            }
            // ===== ENDE LICENSE-INFO =====
        }
    }

    void test_tab( ) {
        auto draw = ImGui::GetWindowDrawList( );
        ImVec2 pos = ImGui::GetWindowPos( );
        ImVec2 size = ImGui::GetWindowSize( );
        ImGui::SetCursorPos(ImVec2(169, 134));
        ImGui::BeginChild("General", ImVec2(320, 240), true);
        {

            ImGui::Spacing( );
            //    ImGui::SetCursorPosX(10);
            ImGui::Checkbox("Enable CW", &CW_Enabled);
            ImGui::SliderInt("Delay", &CW_Delay, 0, 4);
            ImGui::Checkbox("Enable FastPlace", &FastPlace_Enabled);
            ImGui::Checkbox("Enable Reach", &Reach_Enabled);
            ImGui::SliderInt("Range", &Reach_range, 3, 6);
            ImGui::Checkbox("Enable AutoTotem", &AutoTotem_Enabled);
            ImGui::Checkbox("Enable NoJumpDelay", &NoJumpDelay_Enabled);
            ImGui::Checkbox("Enable AutoSprint", &AutoSprint_Enabled);
            ImGui::Checkbox("Enable HitCrystal", &HitCrystal_Enabled);
            ImGui::Checkbox("Enable SilentAim", &Silent_Aim_Enabled);
        }
        ImGui::EndChild( );
        
        
    }

    void RenderTabs( ) {
        auto draw = ImGui::GetWindowDrawList( );
        ImVec2 pos = ImGui::GetWindowPos( );

        draw->AddText(poppins, 17, ImVec2(pos.x + 13, pos.y + 81), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), "Combat");         // right bg
        draw->AddText(poppins, 17, ImVec2(pos.x + 13, pos.y + 210), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), "Visuals");       // right bg
        draw->AddText(poppins, 17, ImVec2(pos.x + 13, pos.y + 348), ImColor(105, 105, 105, int(255 * ImGui::GetStyle( ).Alpha)), "Miscellaneous"); // right bg

        ImGui::SetCursorPos(ImVec2(13, 99));
        if (ImGui::Rendertab(ICON_FA_USER, "Aim Assist", !m_tabs))
            m_tabs = 0;

        ImGui::SetCursorPos(ImVec2(13, 136));
        if (ImGui::Rendertab("e", "Clicker", m_tabs == 1))
            m_tabs = 1;

        ImGui::SetCursorPos(ImVec2(13, 174));
        if (ImGui::Rendertab("a", "Velocity", m_tabs == 2))
            m_tabs = 2;

        ImGui::SetCursorPos(ImVec2(13, 228));
        if (ImGui::Rendertab("x", "Players", m_tabs == 3))
            m_tabs = 3;

        ImGui::SetCursorPos(ImVec2(13, 266));
        if (ImGui::Rendertab("w", "World", m_tabs == 4))
            m_tabs = 4;

        ImGui::SetCursorPos(ImVec2(13, 304));
        if (ImGui::Rendertab("v", "ESP", m_tabs == 5))
            m_tabs = 5;

        ImGui::SetCursorPos(ImVec2(13, 369));
        if (ImGui::Rendertab("s", "Inventory", m_tabs == 6))
            m_tabs = 6;

        ImGui::SetCursorPos(ImVec2(13, 407));
        if (ImGui::Rendertab("z", "Settings", m_tabs == 7))
            m_tabs = 7;

        ImGui::SetCursorPos(ImVec2(13, 445));
        if (ImGui::Rendertab("c", "Configs", m_tabs == 8))
            m_tabs = 8;
        switch (m_tabs) {
            case 0:
                test_tab( );
                break; // aim assist
            case 1:
                break; // Clicker
            case 2:
                break; // ANTI-AIM
            case 3:
                break; // PlAYERS
            case 4:
                break; // WORLD
            case 5:
                break; // VIEW
            case 6:
                break; // Inventory
            case 7:
                break; // Settings
            case 8:
                break; // CONFIGS
        }
    }

    void Render( ) {
    //    if (!Menu_Enabled)
     //       return;

            if (!logged_in) {
            if (Menu_Enabled)
                login_alpha = ImClamp(login_alpha + (2.f * ImGui::GetIO( ).DeltaTime * 1.5f), 0.f, 1.f);
            else
                login_alpha = 0;

                if (login_alpha > 0.01f) {
                    ImGui::SetNextWindowSize(ImVec2(838 * 1.0f, 535 * 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, login_alpha);
                    ImGui::Begin("Login", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
                    {
                        login_tab( );
                        Particles( );
                    }
                    ImGui::End( );
                    ImGui::PopStyleVar( );
                }
            } 
            else {
                if (Menu_Enabled)
                    open_alpha = ImClamp(open_alpha + (2.f * ImGui::GetIO( ).DeltaTime * (toggled ? 1.5f : -1.5f)), 0.f, 1.f);
                else
                    open_alpha = 0;

                if (open_alpha > 0.01f) {
                    ImGui::SetNextWindowSize(ImVec2(838 * 1.0f, 535 * 1.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, open_alpha);
                    ImGui::Begin("Main GUI", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
                    {
                        Decoration( );
                        RenderTabs( );
                        user_info( );
                        Particles( );
                    }
                    ImGui::End( );
                    ImGui::PopStyleVar( );
                }
            }
    }
} // namespace Menu
