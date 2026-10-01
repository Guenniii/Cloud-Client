#pragma once
#include <Windows.h>
#include "../utils/utils.hpp"
#include "../utils/config/config.hpp"
#include "../utils/theme/theme.hpp"
#include "../modules/settings.hpp"
#include <algorithm>

namespace Menu::StatusHud {
inline void Draw() {
    if (!Config::hud_enabled) return;
    struct Entry { const char* name; std::atomic<bool>* enabled; };
    const Entry entries[] = {
        {"Auto Armor",&AutoArmor_Enabled},{"Refill",&Refill_Enabled},{"Hit Effect",&HitEffect_Enabled},{"Predict Double Hand",&PredictDoubleHand_Enabled},{"Auto Shield Breaker",&ShieldBreaker_Enabled},
        {"CW", &CW_Enabled}, {"Reach", &Reach_Enabled},
        {"FastPlace", &FastPlace_Enabled}, {"AutoSprint", &AutoSprint_Enabled},
        {"NoJumpDelay", &NoJumpDelay_Enabled}, {"AutoTotem", &AutoTotem_Enabled},
        {"HitCrystal", &HitCrystal_Enabled}, {"Auto Pearl Catch", &AutoPearlCatch_Enabled}, {"Short Wind Charge", &ShortWindCharge_Enabled}, {"Safe Walk", &SafeWalk_Enabled}, {"Auto Mace", &AutoMace_Enabled}, {"Fake Lag", &FakeLag_Enabled}, {"Auto Tool", &AutoTool_Enabled}, {"Breach Swap", &BreachSwap_Enabled}, {"Safe Anchor", &SafeAnchor_Enabled}, {"Aim Assist", &AimAssist_Enabled}, {"SilentAim", &Silent_Aim_Enabled},
        {"SeedCracker", &SeedCracker_Enabled}, {"OreSim", &OreSim_Enabled},
        {"Player ESP", &PlayerESP_Enabled}
    };
    const char* active[sizeof(entries) / sizeof(entries[0])];
    int count=0;
    for(const auto& entry:entries) if(entry.enabled->load()) active[count++]=entry.name;
    const auto* vp=ImGui::GetMainViewport();
    const float width=(std::min)(220.0f,vp->WorkSize.x);
    const float height=(std::min)(68.0f+ImGui::GetTextLineHeightWithSpacing()*(std::max)(1,count),vp->WorkSize.y);
    const int maxX=(std::max)(0,static_cast<int>(vp->WorkSize.x-width));
    const int maxY=(std::max)(0,static_cast<int>(vp->WorkSize.y-height));
    Config::hud_x=std::clamp(Config::hud_x,0,maxX);
    Config::hud_y=std::clamp(Config::hud_y,0,maxY);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+Config::hud_x,vp->WorkPos.y+Config::hud_y));
    ImGui::SetNextWindowSize(ImVec2(width,height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(12,10));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImGui::ColorConvertU32ToFloat4(Theme::Current().card));
    auto flags=ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if(!Menu_Enabled) flags |= ImGuiWindowFlags_NoInputs;
    if(ImGui::Begin("##status_hud",nullptr,flags)) {
        auto pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##drag",ImVec2((std::max)(1.0f,width-24),20));
        if(Menu_Enabled && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            Config::hud_x=std::clamp(Config::hud_x+static_cast<int>(ImGui::GetIO().MouseDelta.x),0,maxX);
            Config::hud_y=std::clamp(Config::hud_y+static_cast<int>(ImGui::GetIO().MouseDelta.y),0,maxY);
        }
        ImGui::GetWindowDrawList()->AddText(pos,Theme::Current().accent,"FUSION / STATUS");
        ImGui::TextDisabled("%s  |  %d active",U::RenderingBackendToStr(),count);
        ImGui::Separator();
        if(!count) ImGui::TextDisabled("No active modules");
        for(int i=0;i<count;++i) ImGui::TextUnformatted(active[i]);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
}
