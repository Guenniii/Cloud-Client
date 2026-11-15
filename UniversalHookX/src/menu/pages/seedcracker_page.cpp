#include "../pages.hpp"
#include "../widgets.hpp"
#include "../palette.hpp"
#include "../../dependencies/imgui/imgui_internal.h"
#include <cstdio>
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include "../../modules/settings.hpp"
#include "../../utils/seedcracker/seed_handoff.hpp"

namespace Menu::Pages {
using namespace Menu::Widgets;

static constexpr int SC_TARGET_WRACKS = 9;
static constexpr double SC_TARGET_BITS = 77.0;

void DrawSeedCrackerPage(ImDrawList* draw_list, ImVec2 area_min, float area_w, float area_h, float alpha) {
    ImVec2 area_max = ImVec2(area_min.x + area_w, area_min.y + area_h);

    // Karten-Hintergrund
    draw_list->AddRectFilled(area_min, area_max, ColA(Menu::Palette::Card(), alpha), 12.0f);
    draw_list->AddRect(area_min, area_max, ColA(Menu::Palette::Border(), alpha), 12.0f, 0, 1.0f);

    ImGui::SetCursorScreenPos(area_min);
    ImGui::BeginChild("SeedCrackerContent",ImVec2(area_w,area_h),false,ImGuiWindowFlags_AlwaysVerticalScrollbar|ImGuiWindowFlags_NoBackground);
    area_min=ImGui::GetCursorScreenPos();area_w=ImGui::GetContentRegionAvail().x;
    area_max=ImVec2(area_min.x+area_w,area_min.y+area_h);
    draw_list=ImGui::GetWindowDrawList();

    // Titel
    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 18), ColA(Menu::Palette::Text(), alpha), "SeedCracker");
//    draw_list->AddText(ImVec2(area_min.x + 20, area_min.y + 40), ColA(Menu::Palette::Muted(), alpha), "Schiffswracks sammeln -> Structure-Seeds -> Weltseed.");

    {
        const int radius=std::clamp(SeedCracker_Radius.load(),1,16);
        char info[128];std::snprintf(info,sizeof(info),"Scan: %d-chunk radius / %d x %d blocks | loaded chunks only",radius,(radius*2+1)*16,(radius*2+1)*16);
        draw_list->AddText(ImVec2(area_min.x+20,area_min.y+42),ColA(Menu::Palette::Label(),alpha),info);
    }

    // Status immer aus JNI lesen (Daten bleiben auch bei Modul-Aus erhalten).
    // Reset nur ueber den Panel-Button weiter unten Ã¢â‚¬â€œ nicht beim Deaktivieren.
    bool enabled = SeedCracker_Enabled;
    JNIEnv* sc_env = SeedCracker::GetMinecraftJNIEnv( );

    SeedCracker::CrackingStatus status{ };
    if (sc_env)
        status = SeedCracker::GetCrackingStatus(sc_env);

    float row_y = area_min.y + 76.0f;
    float row_w = area_w - 40.0f;

    draw_list->AddLine(ImVec2(area_min.x + 20, row_y - 4), ImVec2(area_max.x - 20, row_y - 4), ColA(Menu::Palette::Border(), alpha));

    // Hinweis wenn Modul pausiert (Scans aus, gespeicherte Daten bleiben)
    if (!enabled) {
        draw_list->AddText(ImVec2(area_min.x + 20, row_y + 2), ColA(Menu::Palette::Muted(), alpha),
                           "Module paused - collected data is retained");
        row_y += 28.0f;
    }

    // --- Bit-Fortschrittsbalken (Ziel: SC_TARGET_BITS) ---
    double total_bits =status.liftingBits;
    double wanted = status.wantedBits > 0.0 ? status.wantedBits : SC_TARGET_BITS;
    if (wanted < SC_TARGET_BITS)
        wanted = SC_TARGET_BITS;
    bool enough_bits = total_bits >= SC_TARGET_BITS || (status.wrackCount >= SC_TARGET_WRACKS && total_bits >= 70);

    draw_list->AddText(ImVec2(area_min.x + 20, row_y + 2), ColA(Menu::Palette::Label(), alpha), "Collected bits");
    {
        char bits_buf[64];
        if (enough_bits)
            snprintf(bits_buf, sizeof(bits_buf), "%.1f  (enough)", total_bits);
        else
            snprintf(bits_buf, sizeof(bits_buf), "%.1f / %.0f", total_bits, wanted);
        ImVec2 bits_size = ImGui::CalcTextSize(bits_buf);
        ImU32 bits_col = enough_bits ? ColA(IM_COL32(46, 204, 113, 255), alpha) : ColA(Menu::Palette::Accent(), alpha);
        draw_list->AddText(ImVec2(area_min.x + 20 + row_w - bits_size.x, row_y + 2), bits_col, bits_buf);

        float bar_y = row_y + 22.0f;
        float bar_h = 6.0f;
        float bar_xL = area_min.x + 20;
        float bar_xR = bar_xL + row_w;
        float pct = (float)ImSaturate(total_bits / wanted);

        draw_list->AddRectFilled(ImVec2(bar_xL, bar_y), ImVec2(bar_xR, bar_y + bar_h),
                                 ColA(Menu::Palette::Toggle(), alpha), 3.0f);
        if (pct > 0.0f) {
            ImU32 fill_col = enough_bits ? ColA(IM_COL32(46, 204, 113, 255), alpha)
                                         : ColA(Menu::Palette::Accent(), alpha);
            DrawGlowRect(draw_list, ImVec2(bar_xL, bar_y),
                         ImVec2(bar_xL + pct * (bar_xR - bar_xL), bar_y + bar_h),
                         3.0f, fill_col, alpha * 0.5f, 2);
            draw_list->AddRectFilled(ImVec2(bar_xL, bar_y),
                                     ImVec2(bar_xL + pct * (bar_xR - bar_xL), bar_y + bar_h),
                                     fill_col, 3.0f);
        }
        row_y += 40.0f;
    }

    // --- Schiffswracks: n / Ziel ---
    draw_list->AddLine(ImVec2(area_min.x + 20, row_y), ImVec2(area_max.x - 20, row_y), ColA(Menu::Palette::Border(), alpha));
    row_y += 14.0f;

    draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Shipwrecks");
    {
        char wc_buf[48];
        bool enough_wracks = status.wrackCount >= SC_TARGET_WRACKS || enough_bits;
        if (enough_wracks)
            snprintf(wc_buf, sizeof(wc_buf), "%d  (enough)", status.wrackCount);
        else
            snprintf(wc_buf, sizeof(wc_buf), "%d / %d", status.wrackCount, SC_TARGET_WRACKS);
        ImVec2 wc_size = ImGui::CalcTextSize(wc_buf);
        ImU32 wc_col = enough_wracks ? ColA(IM_COL32(46, 204, 113, 255), alpha) : ColA(Menu::Palette::Text(), alpha);
        draw_list->AddText(ImVec2(area_min.x + 20 + row_w - wc_size.x, row_y), wc_col, wc_buf);
        row_y += 28.0f;
    }

    // --- Structure-Seeds / Biomes (wenn verfuegbar) ---
    if (status.structureSeedCount > 0 || status.biomeCount > 0) {
        draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Structure seeds");
        char ss_buf[32];
        snprintf(ss_buf, sizeof(ss_buf), "%d", status.structureSeedCount);
        ImVec2 ss_size = ImGui::CalcTextSize(ss_buf);
        draw_list->AddText(ImVec2(area_min.x + 20 + row_w - ss_size.x, row_y), ColA(Menu::Palette::Text(), alpha), ss_buf);
        row_y += 24.0f;

        if (status.biomeCount > 0) {
            draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Biome types");
            char bb[48];
            if (status.biomeCount >= 7)
                snprintf(bb, sizeof(bb), "%d  (enough)", status.biomeCount);
            else
                snprintf(bb, sizeof(bb), "%d / 7+", status.biomeCount);
            ImVec2 bs = ImGui::CalcTextSize(bb);
            ImU32 bc = status.biomeCount >= 7 ? ColA(IM_COL32(46, 204, 113, 255), alpha) : ColA(Menu::Palette::Text(), alpha);
            draw_list->AddText(ImVec2(area_min.x + 20 + row_w - bs.x, row_y), bc, bb);
            row_y += 24.0f;
        }
    }

    // --- Status-Badge ---
    const char* state_label = "Waiting";
    ImU32 state_bg = ColA(Menu::Palette::Border(), alpha);
    ImU32 state_text = ColA(Menu::Palette::Muted(), alpha);

    switch (status.state) {
        case SeedCracker::CrackState::COLLECTING:
            state_label = enough_bits ? "Enough data - waiting for reduction..." : "Collecting data...";
            state_bg = ColA(Menu::Palette::Soft(), alpha);
            state_text = ColA(Menu::Palette::Accent(), alpha);
            break;
        case SeedCracker::CrackState::STRUCTURE:
            state_label = "Searching structure seeds...";
            state_bg = ColA(Menu::Palette::Soft(), alpha);
            state_text = ColA(Menu::Palette::Accent(), alpha);
            break;
        case SeedCracker::CrackState::CANDIDATES: {
            static char cand_label[64];
            snprintf(cand_label, sizeof(cand_label), "%d candidates", status.candidates > 0 ? status.candidates : (int)status.candidateSeeds.size( ));
            state_label = cand_label;
            state_bg = ColA(IM_COL32(255, 240, 200, 255), alpha);
            state_text = ColA(IM_COL32(180, 120, 0, 255), alpha);
            break;
        }
        case SeedCracker::CrackState::FAILED:
            state_label = enough_bits ? "Reduction failed - collect more regions" : "More shipwrecks needed";
            state_bg = ColA(IM_COL32(255, 225, 225, 255), alpha);
            state_text = ColA(IM_COL32(200, 50, 50, 255), alpha);
            break;
        case SeedCracker::CrackState::MULTIPLE:
            state_label = "Multiple candidates";
            state_bg = ColA(IM_COL32(255, 240, 200, 255), alpha);
            state_text = ColA(IM_COL32(180, 120, 0, 255), alpha);
            break;
        case SeedCracker::CrackState::FOUND:
            state_label = "Seed found!";
            state_bg = ColA(IM_COL32(200, 245, 215, 255), alpha);
            state_text = ColA(IM_COL32(30, 160, 80, 255), alpha);
            break;
        default:
            if (enough_bits) {
                state_label = "Enough bits / shipwrecks";
                state_bg = ColA(IM_COL32(200, 245, 215, 255), alpha);
                state_text = ColA(IM_COL32(30, 160, 80, 255), alpha);
            }
            break;
    }

    draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Status");
    {
        ImVec2 label_size = ImGui::CalcTextSize(state_label);
        float badge_px = 10.0f, badge_h = 20.0f;
        float badge_w = label_size.x + badge_px * 2.0f;
        ImVec2 badge_min = ImVec2(area_min.x + 20 + row_w - badge_w, row_y - 2.0f);
        draw_list->AddRectFilled(badge_min, ImVec2(badge_min.x + badge_w, badge_min.y + badge_h), state_bg, 6.0f);
        draw_list->AddText(ImVec2(badge_min.x + badge_px, badge_min.y + 2.0f), state_text, state_label);
        row_y += 32.0f;
    }

    // --- Seed / Kandidaten ---
    if (status.hasSeed) {
        draw_list->AddLine(ImVec2(area_min.x + 20, row_y), ImVec2(area_max.x - 20, row_y), ColA(Menu::Palette::Border(), alpha));
        row_y += 14.0f;
        draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "World seed");

        char seed_buf[32];
        snprintf(seed_buf, sizeof(seed_buf), "%lld", status.seed);
        ImVec2 seed_size = ImGui::CalcTextSize(seed_buf);
        DrawGlowRect(draw_list,
                     ImVec2(area_min.x + 20 + row_w - seed_size.x - 12, row_y - 4),
                     ImVec2(area_min.x + 20 + row_w + 4, row_y + seed_size.y + 2),
                     4.0f, Menu::Palette::Accent(), alpha * 0.4f, 2);
        draw_list->AddText(ImVec2(area_min.x + 20 + row_w - seed_size.x, row_y),
                           ColA(Menu::Palette::Accent(), alpha), seed_buf);
        row_y += 30.0f;

        // Send the verified seed straight to the ore simulation.
        const bool already_applied = SeedHandoff::HasApplied() && SeedHandoff::AppliedSeed() == status.seed;
        ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20, row_y));
        if (DrawRoundedButton(draw_list, "##sc_send_to_oresim",
                              already_applied ? "Seed applied to OreSim" : "Send seed to OreSim",
                              ImVec2(row_w, 28.0f),
                              ColA(already_applied ? Menu::Palette::Success() : Menu::Palette::Background(), alpha),
                              ColA(Menu::Palette::Soft(), alpha),
                              ColA(already_applied ? Menu::Palette::Background() : Menu::Palette::Text(), alpha),
                              alpha, 7.0f, already_applied)) {
            SeedHandoff::Apply(status.seed);
        }
        row_y += 36.0f;
    }

    // --- Seed handoff: verified candidates can be sent to the ore simulation ---
    {
        const auto verified = SeedHandoff::Candidates();
        const bool applied = SeedHandoff::HasApplied();
        const long long applied_seed = SeedHandoff::AppliedSeed();

        draw_list->AddLine(ImVec2(area_min.x + 20, row_y), ImVec2(area_max.x - 20, row_y), ColA(Menu::Palette::Border(), alpha));
        row_y += 14.0f;
        draw_list->AddText(ImVec2(area_min.x + 20, row_y), ColA(Menu::Palette::Label(), alpha), "Ore simulation");

        if (applied) {
            char applied_buf[64];
            snprintf(applied_buf, sizeof(applied_buf), "Seed %lld applied", applied_seed);
            ImVec2 applied_size = ImGui::CalcTextSize(applied_buf);
            draw_list->AddText(ImVec2(area_min.x + 20 + row_w - applied_size.x, row_y),
                               ColA(Menu::Palette::Success(), alpha), applied_buf);
        } else if (verified.empty()) {
            ImVec2 hint_size = ImGui::CalcTextSize("No verified seed yet");
            draw_list->AddText(ImVec2(area_min.x + 20 + row_w - hint_size.x, row_y),
                               ColA(Menu::Palette::Muted(), alpha), "No verified seed yet");
        }
        row_y += 24.0f;

        if (!verified.empty()) {
            const int max_rows = 6;
            int shown = 0;
            for (long long seed : verified) {
                if (shown >= max_rows)
                    break;
                char item_id[32];
                snprintf(item_id, sizeof(item_id), "##sc_candidate_%d", shown);
                char label[48];
                snprintf(label, sizeof(label), "%lld", seed);

                const bool is_active = applied && applied_seed == seed;
                const ImU32 bg = is_active ? ColA(Menu::Palette::Success(), alpha)
                                           : ColA(Menu::Palette::Background(), alpha);
                const ImU32 bg_hover = is_active ? ColA(Menu::Palette::Success(), alpha)
                                                : ColA(Menu::Palette::Soft(), alpha);
                const ImU32 text_col = is_active ? ColA(Menu::Palette::Background(), alpha)
                                                 : ColA(Menu::Palette::Text(), alpha);
                const float button_w = row_w * 0.55f;
                ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20, row_y));
                if (DrawRoundedButton(draw_list, item_id, label, ImVec2(button_w, 26.0f),
                                      bg, bg_hover, text_col, alpha, 7.0f, is_active)) {
                    SeedHandoff::Apply(seed);
                }

                const char* hint = is_active ? "in use" : "send to OreSim";
                ImVec2 hint_size = ImGui::CalcTextSize(hint);
                draw_list->AddText(ImVec2(area_min.x + 20 + row_w - hint_size.x, row_y + 6.0f),
                                   ColA(is_active ? Menu::Palette::Success() : Menu::Palette::Muted(), alpha), hint);
                row_y += 30.0f;
                shown++;
            }
            if ((int)verified.size() > shown) {
                char more[32];
                snprintf(more, sizeof(more), "+%d more", (int)verified.size() - shown);
                draw_list->AddText(ImVec2(area_min.x + 28, row_y), ColA(Menu::Palette::Muted(), alpha), more);
                row_y += 18.0f;
            }
        }
    }

    // --- Reset-Button ---
    {
        draw_list->AddLine(ImVec2(area_min.x + 20, row_y), ImVec2(area_max.x - 20, row_y), ColA(Menu::Palette::Border(), alpha));
        row_y += 12.0f;

        ImGui::SetCursorScreenPos(ImVec2(area_min.x + 20, row_y));
        bool do_reset = DrawRoundedButton(draw_list, "##sc_reset", "Reset",
                                          ImVec2(row_w, 28.0f),
                                          ColA(Menu::Palette::Background(), alpha),
                                          ColA(Menu::Palette::Soft(), alpha),
                                          ColA(Menu::Palette::Text(), alpha), alpha, 7.0f);
        if (do_reset && sc_env) {
            SeedCracker::ResetCracking(sc_env);
            SeedHandoff::Clear();
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(area_min.x+20,row_y+48));
    ImGui::Dummy(ImVec2(1,1));
    ImGui::EndChild();
}
}
