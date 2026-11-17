#pragma once
#include "module_registry.hpp"
#include "../utils/theme/theme.hpp"
#include "../utils/oresim/seed_settings.hpp"
#include <optional>
#include <vector>
#include <array>
#include <cstdio>

namespace Menu::History {
struct Snapshot {
    std::vector<int> modules;
    std::array<int,27> ui;
    long long seed;
    bool hasSeed;
    bool seedFromCracker;
    std::string seedInput, seedError, activeProfile;
    bool operator==(const Snapshot&) const = default;
};
inline Snapshot Capture() {
    Snapshot state;
    for (const auto& module : Registry::GetModules()) {
        state.modules.insert(state.modules.end(), {module.enabled->load(), module.hotkey,
            static_cast<int>(module.hotkey_mode), module.favorite});
        for (const auto& setting : module.settings_list)
            state.modules.push_back(setting.type == SettingType::Checkbox ? setting.b_val->load() : setting.i_val->load());
    }
    state.ui={Theme::selected,Theme::custom_accent_enabled.load(),Theme::custom_accent_rgb.load(),
        Theme::menu_glow_percent.load(),Theme::esp_glow_percent.load(),Config::menu_size_percent.load(),
        Config::font_size_percent.load(),Config::menu_keybind.load(),Config::particles_enabled,
        0 /* was hud_enabled, slot kept */,Config::hud_x,Config::hud_y,Theme::custom_background_enabled.load(),Theme::custom_background_rgb.load(),
        Theme::gradient_enabled.load(),Theme::secondary_rgb.load(),Theme::gradient_strength.load(),Theme::animated_accent.load(),
        Theme::animation_period.load(),Theme::player_colors_enabled.load(),Theme::friend_rgb.load(),Theme::enemy_rgb.load(),Config::watermark_enabled,Config::watermark_x,Config::watermark_y,Config::menu_style,Config::hotkey_toasts};
    state.seed=OreSim::manualSeed;state.hasSeed=OreSim::hasManualSeed;
    state.seedFromCracker=OreSim::seedFromCracker;
    state.seedInput=OreSim::seedInput;state.seedError=OreSim::seedError;state.activeProfile=Config::active_profile;
    return state;
}
inline void Restore(const Snapshot& state) {
    size_t at=0;
    for (auto& module : Registry::GetModules()) {
        *module.enabled=state.modules[at++];module.hotkey=state.modules[at++];
        module.hotkey_mode=static_cast<Input::HotkeyMode>(state.modules[at++]);module.favorite=state.modules[at++];
        for (auto& setting : module.settings_list) {
            if (setting.type==SettingType::Checkbox) *setting.b_val=state.modules[at++]!=0;
            else *setting.i_val=state.modules[at++];
        }
    }
    Theme::selected=state.ui[0];Theme::custom_accent_enabled=state.ui[1]!=0;Theme::custom_accent_rgb=state.ui[2];
    Theme::menu_glow_percent=state.ui[3];Theme::esp_glow_percent=state.ui[4];
    Config::menu_size_percent=state.ui[5];Config::font_size_percent=state.ui[6];Config::menu_keybind=state.ui[7];
    Config::particles_enabled=state.ui[8]!=0;Config::hud_x=state.ui[10];Config::hud_y=state.ui[11];
    Theme::custom_background_enabled=state.ui[12]!=0;Theme::custom_background_rgb=state.ui[13];
    Theme::gradient_enabled=state.ui[14]!=0;Theme::secondary_rgb=state.ui[15];Theme::gradient_strength=state.ui[16];
    Theme::animated_accent=state.ui[17]!=0;Theme::animation_period=state.ui[18];Theme::player_colors_enabled=state.ui[19]!=0;
    Theme::friend_rgb=state.ui[20];Theme::enemy_rgb=state.ui[21];
    Config::watermark_enabled=state.ui[22]!=0;Config::watermark_x=state.ui[23];Config::watermark_y=state.ui[24];
    Config::menu_style=state.ui[25]>=0&&state.ui[25]<=2?state.ui[25]:2;
    Config::hotkey_toasts=state.ui[26]!=0;
    OreSim::manualSeed=state.seed;OreSim::hasManualSeed=state.hasSeed;OreSim::seedError=state.seedError;
    OreSim::seedFromCracker=state.seedFromCracker;
    std::snprintf(OreSim::seedInput,sizeof(OreSim::seedInput),"%s",state.seedInput.c_str());
    Config::active_profile=state.activeProfile;
    Config::capturing_module_key=-1;Config::capturing_menu_key=false;
    Theme::ApplyStyle();
}
inline std::vector<Snapshot> undo;
inline std::optional<Snapshot> baseline;
inline bool skip=false;
inline void BeginFrame() { if(!baseline) baseline=Capture(); }
inline void EndFrame() {
    if(skip) {skip=false;baseline.reset();return;}
    if(!baseline) return;
    auto state=Capture();
    if(ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsAnyItemActive()) return;
    if(!(state==*baseline)) {
        if(undo.size()==32) undo.erase(undo.begin());
        undo.push_back(std::move(*baseline));
    }
    baseline.reset();
}
inline void Suspend() {baseline.reset();}
inline bool CanUndo(){return !undo.empty();}
inline void Undo() {
    if(undo.empty()) return;
    auto state=std::move(undo.back());undo.pop_back();Restore(state);baseline.reset();skip=true;
}
}
