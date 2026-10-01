#include "login.hpp"
#include "../auth/Authclient.h"
#include "../auth/HWID.h"
#include "../auth/Hash.h"
#include "../dependencies/imgui/imgui.h"
#include "../dependencies/imgui/imgui_internal.h"
#include "../utils/lifecycle/lifecycle.hpp"
#include "../utils/theme/theme.hpp"
#include "palette.hpp"
#include "resources.hpp"
#include "widgets.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <future>
#include <memory>

namespace Menu::Login {
    using namespace Menu::Widgets;
    namespace {
        char username[64] = { };
        char password[64] = { };
        static bool show_password = false;
        static float login_alpha = 0.0f;
        static std::string last_logged_user = "Guenni";
        static bool loginInProgress = false;
        static std::string responseText = "";
        static std::string g_licenseDays = "PERMANENT";
        static std::string g_licenseExpiry = "";
        static bool logged_in = false;

        static bool login_success_pending = false;
        static float login_success_timer = 0.0f;
        static constexpr float LOGIN_SUCCESS_HOLD = 0.7f; // grüne Meldung kurz halten

        // Exit-Animation nach Success (Karte raus → Logo Pulse → Fade)
        static bool success_exit = false;
        static float success_exit_t = 0.0f;
        static constexpr float SUCCESS_EXIT_DURATION = 1.1f;

        // Intro-Animation (Logo → Karte)
        static float intro_t = 0.0f;
        static bool intro_done = false;
        static constexpr float INTRO_DURATION = 1.6f;  // Gesamt
        static constexpr float INTRO_LOGO_HOLD = 0.9f; // Logo allein sichtbar

        struct LoginResult {
            std::string user;
            LicenseInfo license;
        };
        static std::future<LoginResult> pendingLogin;

        void PollLoginResult( ) {
            if (!pendingLogin.valid( ) || pendingLogin.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return;
            try {
                auto result = pendingLogin.get( );
                if (result.license.success) {
                    responseText = "OK";
                    g_licenseDays = result.license.daysLeft == -1 ? "PERMANENT" : std::to_string(result.license.daysLeft);
                    g_licenseExpiry = result.license.expiryDate;
                    last_logged_user = result.user;
                    login_success_pending = true;
                    login_success_timer = 0.0f;
                } else {
                    responseText = result.license.errorMessage;
                    login_success_pending = false;
                }
            } catch (...) {
                responseText = "Login request failed";
                login_success_pending = false;
            }
            loginInProgress = false;
        }

        static float EaseOutCubic(float t) {
            float u = 1.0f - t;
            return 1.0f - u * u * u;
        }
        static float EaseInOut(float t) {
            return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
        }

        static void DrawFieldBorder(ImDrawList* dl, ImVec2 min, ImVec2 max, bool focused, bool error, float alpha) {
            const ImU32 error_color = IM_COL32(215, 85, 105, 255);
            ImU32 col = error ? error_color : focused ? Menu::Palette::Accent( )
                                                      : Menu::Palette::Border( );
            dl->AddRect(min, max, ColA(col, alpha), 10.0f, 0, focused ? 1.5f : 1.0f);
        }

        // Animiertes Logo (Test: Resources::LogoTexture – später eigenes PNG)
        static void DrawAnimatedLogo(ImDrawList* draw, ImVec2 center, float radius, float alpha, float pulse) {
            // Glow-Ringe
            for (int i = 3; i >= 1; --i) {
                float r = radius + (float)i * 10.0f * pulse;
                float fa = (0.12f / (float)i) * alpha * pulse;
                ImVec4 soft = ImGui::ColorConvertU32ToFloat4(Menu::Palette::Accent( ));
                soft.w = fa;
                draw->AddCircleFilled(center, r, ImGui::ColorConvertFloat4ToU32(soft), 48);
            }
            // Weißer Untergrund-Kreis
            draw->AddCircleFilled(center, radius + 3.0f, ColA(IM_COL32(255, 255, 255, 255), alpha));
            // Texture (Phantom-Logo oder später Winged-A)
            ImTextureID tex = Resources::LogoTexture( );
            if (tex) {
                draw->AddImageRounded(
                    tex,
                    ImVec2(center.x - radius, center.y - radius),
                    ImVec2(center.x + radius, center.y + radius),
                    ImVec2(0, 0), ImVec2(1, 1),
                    ColA(IM_COL32(255, 255, 255, 255), alpha),
                    radius);
            }
            draw->AddCircle(center, radius, ColA(Menu::Palette::Accent( ), alpha), 48, 1.5f);
        }

        void DrawModernLogin( ) {
            ImDrawList* draw = ImGui::GetWindowDrawList( );
            ImVec2 pos = ImGui::GetWindowPos( );
            ImVec2 size = ImGui::GetWindowSize( );
            const float a = login_alpha;
            const float dt = ImGui::GetIO( ).DeltaTime;

            // Intro vorantreiben (nicht während Exit)
            if (!intro_done && !success_exit) {
                intro_t += dt;
                if (intro_t >= INTRO_DURATION)
                    intro_done = true;
            }

            const float card_w = 420.0f;
            const float card_h = 480.0f;
            const float card_x = pos.x + (size.x - card_w) * 0.5f;
            const float card_y = pos.y + (size.y - card_h) * 0.5f;
            ImVec2 cmin(card_x, card_y);
            ImVec2 cmax(card_x + card_w, card_y + card_h);

            // Ziel-Logo in der Karte
            const float logo_r_final = 28.0f;
            ImVec2 logo_final(card_x + card_w * 0.5f, card_y + 52.0f);
            // Start / Exit-Center: groß, mittig
            const float logo_r_start = 72.0f;
            ImVec2 logo_start(pos.x + size.x * 0.5f, pos.y + size.y * 0.42f);

            float card_alpha = 0.0f;
            float logo_r = logo_r_start;
            ImVec2 logo_c = logo_start;
            float logo_alpha = a;
            float pulse = 1.0f;

            if (success_exit) {
                // Exit: Karte ausblenden, Logo zur Mitte + größer + Pulse, dann Fade
                float t = ImSaturate(success_exit_t / SUCCESS_EXIT_DURATION);
                float t1 = EaseInOut(ImSaturate(t / 0.45f));              // Karte raus / Logo wandert
                float t2 = EaseOutCubic(ImSaturate((t - 0.35f) / 0.35f)); // Logo scale
                float t3 = EaseInOut(ImSaturate((t - 0.65f) / 0.35f));    // Fade out

                logo_c.x = logo_final.x + (logo_start.x - logo_final.x) * t1;
                logo_c.y = logo_final.y + (logo_start.y - logo_final.y) * t1;
                logo_r = logo_r_final + (logo_r_start * 1.15f - logo_r_final) * t2;
                pulse = 1.0f + 0.25f * std::sin(success_exit_t * 10.0f) * (1.0f - t3);
                logo_alpha = a * (1.0f - t3);
                card_alpha = a * (1.0f - t1);
            } else if (!intro_done) {
                if (intro_t < INTRO_LOGO_HOLD) {
                    float t = EaseOutCubic(ImSaturate(intro_t / 0.55f));
                    logo_alpha = a * t;
                    logo_r = logo_r_start * (0.55f + 0.45f * t);
                    logo_c = logo_start;
                    pulse = 0.85f + 0.15f * std::sin(intro_t * 6.0f);
                    card_alpha = 0.0f;
                } else {
                    float t = EaseInOut(ImSaturate((intro_t - INTRO_LOGO_HOLD) / (INTRO_DURATION - INTRO_LOGO_HOLD)));
                    logo_c.x = logo_start.x + (logo_final.x - logo_start.x) * t;
                    logo_c.y = logo_start.y + (logo_final.y - logo_start.y) * t;
                    logo_r = logo_r_start + (logo_r_final - logo_r_start) * t;
                    logo_alpha = a;
                    pulse = 1.0f - 0.3f * t;
                    card_alpha = a * t;
                }
            } else {
                logo_c = logo_final;
                logo_r = logo_r_final;
                logo_alpha = a;
                card_alpha = a;
                pulse = 1.0f;
            }

            // Karte (während Intro eingeblendet)
            if (card_alpha > 0.01f) {
                for (int i = 4; i >= 1; --i) {
                    float e = (float)i * 5.0f;
                    float fa = (1.0f - (float)i / 5.0f) * 0.06f * card_alpha;
                    ImVec4 soft = ImGui::ColorConvertU32ToFloat4(Menu::Palette::Accent( ));
                    soft.w = fa;
                    draw->AddRectFilled(ImVec2(cmin.x - e, cmin.y - e), ImVec2(cmax.x + e, cmax.y + e),
                                        ImGui::ColorConvertFloat4ToU32(soft), 18.0f + e);
                }
                draw->AddRectFilled(cmin, cmax, ColA(Menu::Palette::Card( ), card_alpha), 16.0f);
                draw->AddRect(cmin, cmax, ColA(Menu::Palette::Border( ), card_alpha), 16.0f, 0, 1.0f);
            }

            // Logo immer (über der Karte, damit Transition sauber wirkt)
            DrawAnimatedLogo(draw, logo_c, logo_r, logo_alpha, pulse);

            // Rest der UI erst wenn Intro fast/fertig
            if (card_alpha < 0.5f)
                return;

            const float ui_a = card_alpha;

            const char* title = "PHANTOM";
            ImVec2 title_sz = ImGui::CalcTextSize(title);
            draw->AddText(ImVec2(card_x + (card_w - title_sz.x) * 0.5f, card_y + 92.0f),
                          ColA(Menu::Palette::Text( ), ui_a), title);

            const char* sub = "Sign in to continue";
            ImVec2 sub_sz = ImGui::CalcTextSize(sub);
            draw->AddText(ImVec2(card_x + (card_w - sub_sz.x) * 0.5f, card_y + 114.0f),
                          ColA(Menu::Palette::Muted( ), ui_a), sub);

            {
                const char* badge = "BETA";
                ImVec2 bs = ImGui::CalcTextSize(badge);
                float bw = bs.x + 16.0f, bh = 18.0f;
                ImVec2 bmin(card_x + (card_w - bw) * 0.5f, card_y + 138.0f);
                draw->AddRectFilled(bmin, ImVec2(bmin.x + bw, bmin.y + bh), ColA(Menu::Palette::Soft( ), ui_a), bh * 0.5f);
                draw->AddText(ImVec2(bmin.x + 8.0f, bmin.y + 1.0f), ColA(Menu::Palette::Accent( ), ui_a), badge);
            }

            const float field_x = card_x + 40.0f;
            const float field_w = card_w - 80.0f;
            float fy = card_y + 175.0f;

            draw->AddText(ImVec2(field_x, fy), ColA(Menu::Palette::Muted( ), ui_a), "Username");
            fy += 22.0f;

            ImGui::SetCursorScreenPos(ImVec2(field_x, fy));
            ImGui::SetNextItemWidth(field_w);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 11.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ColV(Menu::Palette::Background( ), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ColV(Menu::Palette::Soft( ), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ColV(Menu::Palette::Background( ), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ColV(Menu::Palette::Text( ), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, ColV(Menu::Palette::Muted( ), 1.0f));
            ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ColV(Menu::Palette::Soft( ), 1.0f));

            ImGui::InputTextWithHint("##username", "Enter username", username, sizeof(username),
                                     ImGuiInputTextFlags_CharsNoBlank);
            bool user_focused = ImGui::IsItemActive( );
            ImVec2 user_min = ImGui::GetItemRectMin( );
            ImVec2 user_max = ImGui::GetItemRectMax( );
            DrawFieldBorder(draw, user_min, user_max, user_focused, false, ui_a);

            fy = user_max.y + 18.0f;

            draw->AddText(ImVec2(field_x, fy), ColA(Menu::Palette::Muted( ), ui_a), "Password");
            {
                const char* show_lbl = show_password ? "Hide" : "Show";
                ImVec2 sl = ImGui::CalcTextSize(show_lbl);
                ImVec2 sp(field_x + field_w - sl.x, fy);
                draw->AddText(sp, ColA(Menu::Palette::Accent( ), ui_a), show_lbl);
                ImGui::SetCursorScreenPos(ImVec2(sp.x - 4, sp.y - 2));
                if (ImGui::InvisibleButton("##toggle_pw", ImVec2(sl.x + 8, sl.y + 4)))
                    show_password = !show_password;
            }
            fy += 22.0f;

            ImGui::SetCursorScreenPos(ImVec2(field_x, fy));
            ImGui::SetNextItemWidth(field_w);
            ImGuiInputTextFlags pw_flags = ImGuiInputTextFlags_CharsNoBlank;
            if (!show_password)
                pw_flags |= ImGuiInputTextFlags_Password;
            bool enter_submit = ImGui::InputTextWithHint("##password", "Enter password", password, sizeof(password),
                                                         pw_flags | ImGuiInputTextFlags_EnterReturnsTrue);
            bool pass_focused = ImGui::IsItemActive( );
            ImVec2 pass_min = ImGui::GetItemRectMin( );
            ImVec2 pass_max = ImGui::GetItemRectMax( );
            DrawFieldBorder(draw, pass_min, pass_max, pass_focused, false, ui_a);

            ImGui::PopStyleColor(6);
            ImGui::PopStyleVar(3);

            fy = pass_max.y + 28.0f;

            const float btn_h = 42.0f;
            ImGui::SetCursorScreenPos(ImVec2(field_x, fy));
            bool clicked = false;
            if (loginInProgress || login_success_pending) {
                draw->AddRectFilled(ImVec2(field_x, fy), ImVec2(field_x + field_w, fy + btn_h),
                                    ColA(Menu::Palette::Soft( ), ui_a), 10.0f);
                const char* busy = login_success_pending ? "Success!" : "Connecting...";
                ImVec2 bs = ImGui::CalcTextSize(busy);
                ImU32 busy_col = login_success_pending ? Menu::Palette::Success( ) : Menu::Palette::Muted( );
                draw->AddText(ImVec2(field_x + (field_w - bs.x) * 0.5f, fy + (btn_h - bs.y) * 0.5f),
                              ColA(busy_col, ui_a), busy);
                ImGui::Dummy(ImVec2(field_w, btn_h));
            } else {
                clicked = DrawRoundedButton(draw, "##login_btn", "Login",
                                            ImVec2(field_w, btn_h),
                                            Menu::Palette::Accent( ),
                                            Theme::Current( ).hover,
                                            Theme::Current( ).onAccent,
                                            ui_a, 10.0f, true);
            }

            if ((clicked || enter_submit) && !loginInProgress && !login_success_pending && intro_done) {
                loginInProgress = true;
                responseText.clear( );
                g_licenseDays.clear( );
                g_licenseExpiry.clear( );
                auto task = std::make_shared<std::packaged_task<LoginResult( )>>(
                    [user = std::string(username), pass = std::string(password)] {
                        auto raw = LoginRequestWithResponse(user, Sha256(pass), GetHWID( ));
                        return LoginResult{user, ParseLoginResponse(raw)};
                    });
                pendingLogin = task->get_future( );
                Lifecycle::Spawn([task] { (*task)( ); });
            }

            fy += btn_h + 20.0f;

            if (!responseText.empty( )) {
                bool ok = (responseText == "OK");
                ImU32 status_col = ok ? Menu::Palette::Success( ) : IM_COL32(215, 85, 105, 255);

                std::string msg = responseText;
                if (responseText == "OK")
                    msg = "Login successful";
                else if (responseText == "FAIL")
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

                ImVec2 ms = ImGui::CalcTextSize(msg.c_str( ));
                float pill_w = ms.x + 28.0f;
                float pill_h = 28.0f;
                ImVec2 pmin(card_x + (card_w - pill_w) * 0.5f, fy);
                ImVec4 soft = ImGui::ColorConvertU32ToFloat4(status_col);
                soft.w = 0.12f * ui_a;
                draw->AddRectFilled(pmin, ImVec2(pmin.x + pill_w, pmin.y + pill_h - 10.f),
                                    ImGui::ColorConvertFloat4ToU32(soft), 8.0f);
                draw->AddCircleFilled(ImVec2(pmin.x + 12.0f, pmin.y + pill_h * 0.5f - 5.f), 3.5f, ColA(status_col, ui_a));
                draw->AddText(ImVec2(pmin.x + 22.0f, pmin.y + (pill_h - ms.y) * 0.5f - 5.f),
                              ColA(status_col, ui_a), msg.c_str( ));

                fy += pill_h + 12.0f;

                
            }

            draw->AddLine(ImVec2(cmin.x + 24, cmax.y - 48), ImVec2(cmax.x - 24, cmax.y - 48),
                          ColA(Menu::Palette::Border( ), ui_a));
            const char* foot = "v4.2.1  ·  Secure authentication";
            ImVec2 fs = ImGui::CalcTextSize(foot);
            draw->AddText(ImVec2(card_x + (card_w - fs.x) * 0.5f, cmax.y - 32.0f),
                          ColA(Menu::Palette::Muted( ), ui_a), foot);
        }
    } // namespace

    bool IsLoggedIn( ) { return logged_in; }
    const std::string& UserName( ) { return last_logged_user; }
    const std::string& LicenseDays( ) { return g_licenseDays; }
    const std::string& LicenseExpiry( ) { return g_licenseExpiry; }

    void Render(bool menuEnabled, void (*drawParticles)( )) {
        PollLoginResult( );

        // 1) Success-Meldung kurz halten, dann Exit-Animation starten
        if (login_success_pending && !success_exit && !logged_in) {
            login_success_timer += ImGui::GetIO( ).DeltaTime;
            if (login_success_timer >= LOGIN_SUCCESS_HOLD) {
                login_success_pending = false;
                success_exit = true;
                success_exit_t = 0.0f;
            }
        }

        // 2) Exit-Animation: Karte raus, Logo Pulse, Fade → dann Main-GUI
        if (success_exit && !logged_in) {
            success_exit_t += ImGui::GetIO( ).DeltaTime;
            if (success_exit_t >= SUCCESS_EXIT_DURATION) {
                success_exit = false;
                logged_in = true;
            }
        }

        if (menuEnabled)
            login_alpha = ImClamp(login_alpha + (2.f * ImGui::GetIO( ).DeltaTime * 1.5f), 0.f, 1.f);
        else
            login_alpha = 0.f;

        const bool show_login = !logged_in;

        if (show_login && login_alpha > 0.01f) {
            const float win_w = 460.0f;
            const float win_h = 520.0f;
            ImGui::SetNextWindowSize(ImVec2(win_w, win_h));
            ImGui::SetNextWindowPos(ImVec2(
                                        (GetSystemMetrics(SM_CXSCREEN) - win_w) * 0.5f,
                                        (GetSystemMetrics(SM_CYSCREEN) - win_h) * 0.5f),
                                    ImGuiCond_Once);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, login_alpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::Begin("Login", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
            {
                DrawModernLogin( );
                if (drawParticles)
                    drawParticles( );
            }
            ImGui::End( );
            ImGui::PopStyleColor( );
            ImGui::PopStyleVar(3);
        }
    }
} // namespace Menu::Login
