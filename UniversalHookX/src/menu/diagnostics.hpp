#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include "performance.hpp"
#include "module_registry.hpp"
#include "../modules/settings.hpp"
#include "../utils/utils.hpp"
#include "../utils/theme/theme.hpp"
#include "../utils/oresim/seed_settings.hpp"

namespace Menu::Diagnostics {
inline std::string status, lastPath;

inline std::string Report() {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    SYSTEMTIME time; GetLocalTime(&time);
    out << "Fusion diagnostic report v1\n"
        << "Local time: " << time.wYear << '-' << time.wMonth << '-' << time.wDay
        << ' ' << time.wHour << ':' << time.wMinute << ':' << time.wSecond << "\n"
        << "Renderer: " << U::RenderingBackendToStr() << "\n"
        << "Theme: " << Theme::Current().name << "\n"
        << "HUD: " << Config::hud_enabled << " at " << Config::hud_x << ',' << Config::hud_y << "\n"
        << "Menu key (Windows VK): " << Config::menu_keybind.load() << "\n"
        << "OreSim seed configured: " << OreSim::hasManualSeed << "\n\n"
        << "Render-thread preparation timings (ms); no GPU/background job timings.\n"
        << "Client total includes OreSim and Player ESP. Last up to 120 frames.\n";
    const char* labels[]={"Client total","OreSim","Player ESP"};
    out << std::fixed << std::setprecision(6);
    for(int i=0;i<Performance::Count;++i) {
        auto summary=Performance::samples[i].Read();
        out << labels[i] << ": samples=" << Performance::samples[i].size
            << " average=" << summary.average << " peak=" << summary.peak << "\n";
    }
    out << "\nRuntime features (1=enabled):\n";
    struct Entry { const char* name; std::atomic<bool>* value; };
    const Entry entries[]={
        {"Auto Armor",&AutoArmor_Enabled},{"Refill",&Refill_Enabled},{"Hit Effect",&HitEffect_Enabled},{"Predict Double Hand",&PredictDoubleHand_Enabled},{"Auto Shield Breaker",&ShieldBreaker_Enabled},
        {"CW",&CW_Enabled},{"Reach",&Reach_Enabled},{"FastPlace",&FastPlace_Enabled},
        {"AutoSprint",&AutoSprint_Enabled},{"NoJumpDelay",&NoJumpDelay_Enabled},
        {"AutoTotem",&AutoTotem_Enabled},{"HitCrystal",&HitCrystal_Enabled},
        {"Auto Pearl Catch",&AutoPearlCatch_Enabled},{"Short Wind Charge",&ShortWindCharge_Enabled},{"Safe Walk",&SafeWalk_Enabled},{"Auto Mace",&AutoMace_Enabled},{"Fake Lag", &FakeLag_Enabled}, {"Auto Tool",&AutoTool_Enabled},{"Breach Swap",&BreachSwap_Enabled},{"Safe Anchor",&SafeAnchor_Enabled},{"Aim Assist", &AimAssist_Enabled}, {"SilentAim",&Silent_Aim_Enabled},{"SeedCracker",&SeedCracker_Enabled},
        {"OreSim",&OreSim_Enabled},{"Player ESP",&PlayerESP_Enabled}
    };
    for(const auto& entry:entries) out << entry.name << '=' << entry.value->load() << '\n';
    out << "\nModule settings and hotkeys (Windows VK; 0=unbound):\n";
    for(const auto& module:Registry::GetModules()) {
        out << module.name << ": key=" << module.hotkey << " mode="
            << (module.hotkey_mode==Input::HotkeyMode::Hold ? "hold" : "toggle") << '\n';
        for(const auto& setting:module.settings_list) {
            if(setting.type==SettingType::Checkbox && setting.b_val)
                out << "  " << setting.name << '=' << setting.b_val->load() << '\n';
            else if(setting.type==SettingType::SliderInt && setting.i_val)
                out << "  " << setting.name << '=' << setting.i_val->load() << '\n';
        }
    }
    return out.str();
}

// Called only by the Settings button on the render thread.
inline bool Export(const std::filesystem::path& directory="PhantomUI/diagnostics") {
    std::error_code ec;
    const auto folder=std::filesystem::absolute(directory,ec);
    if(ec) { status="Export failed: could not resolve report folder."; return false; }
    std::filesystem::create_directories(folder,ec);
    if(ec) { status="Export failed: could not create report folder."; return false; }
    static unsigned sequence=0;
    const auto destination=folder/("report-"+std::to_string(GetTickCount64())+"-"+std::to_string(++sequence)+".txt");
    const auto temporary=std::filesystem::path(destination.wstring()+L".tmp");
    std::ofstream file(temporary,std::ios::binary | std::ios::trunc);
    if(!file) { status="Export failed: could not write report."; return false; }
    file << Report();
    file.close();
    if(file.fail() || !MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)) {
        std::filesystem::remove(temporary,ec);
        status="Export failed: could not finish report.";
        return false;
    }
    lastPath=destination.string();
    status="Saved: "+lastPath;
    return true;
}
}
