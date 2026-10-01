#include "module_registry.hpp"
#include "../modules/settings.hpp"
#include <Windows.h>
#include "../input/key_mapping.hpp"
#include "../utils/sdk/java.hpp"
#include "../input/game_screen.hpp"
#include "../input/action_key.hpp"

namespace Menu::Registry {
namespace {
constexpr int MODULE_COUNT = 24;

// Bind card hotkeys to the primary runtime feature, not unused UI flags.

static ModuleData modules[MODULE_COUNT] = {
    {"Auto Armor", 0, 0, &AutoArmor_Enabled, 2, {
        {"Console-Diagnose", SettingType::Checkbox, &AutoArmor_Logs},
        {"Elytra behalten", SettingType::Checkbox, &AutoArmor_KeepElytra},
        {"Inventar-Delay", SettingType::SliderInt, nullptr, &AutoArmor_Delay, 50, 1000, " ms"},
    }},
    {"Refill", 0, 0, &Refill_Enabled, 2, {
        {"Bei Restmenge auffuellen", SettingType::SliderInt, nullptr, &Refill_Threshold, 1, 63, " Items"},
        {"Inventar-Delay", SettingType::SliderInt, nullptr, &Refill_Delay, 50, 1000, " ms"},
    }},
    {"Hit Effect", 0, 0, &HitEffect_Enabled, 0, {
        {"Console-Diagnose", SettingType::Checkbox, &HitEffect_Logs},
        {"Nur bestaetigte Treffer", SettingType::Checkbox, &HitEffect_ConfirmedOnly},
        {"Partikel pro Treffer", SettingType::SliderInt, nullptr, &HitEffect_Count, 4, 32},
        {"Lebensdauer", SettingType::SliderInt, nullptr, &HitEffect_Lifetime, 200, 1500, " ms"},
        {"Partikelgroesse", SettingType::SliderInt, nullptr, &HitEffect_Size, 50, 300, " %"},
        {"Ausbreitung", SettingType::SliderInt, nullptr, &HitEffect_Spread, 25, 200, " %"},
        {"Glow-Staerke", SettingType::SliderInt, nullptr, &HitEffect_Glow, 0, 100, " %"},
    }},
    {"Predict Double Hand", 0, 0, &PredictDoubleHand_Enabled, 0, {
        {"Console-Diagnose", SettingType::Checkbox, &PredictDoubleHand_Logs},
        {"Toedlichen Nahkampfschaden schaetzen", SettingType::Checkbox, &PredictDoubleHand_Melee},
        {"Kristalle an den Fuessen", SettingType::Checkbox, &PredictDoubleHand_Crystal},
        {"Geladene Anker an den Fuessen", SettingType::Checkbox, &PredictDoubleHand_Anchor},
        {"Gefahrenradius", SettingType::SliderInt, nullptr, &PredictDoubleHand_Radius, 1, 6, " m"},
        {"Schadensreserve", SettingType::SliderInt, nullptr, &PredictDoubleHand_Margin, 0, 10, " HP"},
        {"Wenn sicher: Slot zurueck", SettingType::Checkbox, &PredictDoubleHand_Return},
    }},
    {"Auto Shield Breaker", 0, 0, &ShieldBreaker_Enabled, 0, {
        {"Automatisch angreifen (aus: beim Klick)", SettingType::Checkbox, &ShieldBreaker_Automatic},
        {"Angriffsabstand", SettingType::SliderInt, nullptr, &ShieldBreaker_Delay, 50, 1500, " ms"},
    }},

    {"Fake Lag", 0, 0, &FakeLag_Enabled, 2, {
        {"Bewegungs-Delay", SettingType::SliderInt, nullptr, &FakeLag_Delay, 0, 300, " ms"},
        {"Gebundelte Freigabe (aus: gleichmaessiger Delay)", SettingType::Checkbox, &FakeLag_Burst},
        {"Console-Diagnose", SettingType::Checkbox, &FakeLag_Debug},
    }},
    {"Aim Assist", 0, 0, &AimAssist_Enabled, 0, {
        {"Reichweite", SettingType::SliderInt, nullptr, &AimAssist_Range, 1, 6, " m"},
        {"FOV", SettingType::SliderInt, nullptr, &AimAssist_Fov, 10, 180, " deg"},
        {"Drehgeschwindigkeit", SettingType::SliderInt, nullptr, &AimAssist_Speed, 20, 360, " deg/s"},
        {"Smoothness", SettingType::SliderInt, nullptr, &AimAssist_Smoothness, 0, 100, " %"},
        {"Tempo-Variation (0: aus)", SettingType::SliderInt, nullptr, &AimAssist_Variation, 0, 100, " %"},
        {"Nur beim Angriff", SettingType::Checkbox, &AimAssist_OnlyAttack},
        {"Nur mit Nahkampfwaffe", SettingType::Checkbox, &AimAssist_OnlyWeapon},
        {"Nur horizontal", SettingType::Checkbox, &AimAssist_Horizontal},
    }},
    {"Auto Tool", 0, 0, &AutoTool_Enabled, 2, {
        {"Zum vorherigen Slot zurueck", SettingType::Checkbox, &AutoTool_ReturnSlot},
        {"Fast kaputte Werkzeuge meiden", SettingType::Checkbox, &AutoTool_ProtectTools},
    }},
    {"Safe Anchor", 0, 0, &SafeAnchor_Enabled, 0, {
        {"Automatisch (aus: manuell)", SettingType::Checkbox, &SafeAnchor_Automatic},
    //    {"Automatisch befuellen (aus: manuell)", SettingType::Checkbox, &SafeAnchor_AutoCharge},
        {"Glowstone-Schutzblock platzieren", SettingType::Checkbox, &SafeAnchor_Shield},
        {"Automatisch zuenden", SettingType::Checkbox, &SafeAnchor_Detonate},
        {"Console-Logs", SettingType::Checkbox, &SafeAnchor_Logs},
        {"Delay: Anker platzieren", SettingType::SliderInt, nullptr, &SafeAnchor_PlaceDelay, 0, 1000, " ms"},
        {"Delay: Befuellen", SettingType::SliderInt, nullptr, &SafeAnchor_ChargeDelay, 0, 1000, " ms"},
        {"Delay: Schutzblock", SettingType::SliderInt, nullptr, &SafeAnchor_ShieldDelay, 0, 1000, " ms"},
        {"Delay: Zuenden", SettingType::SliderInt, nullptr, &SafeAnchor_DetonateDelay, 0, 1000, " ms"},
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &SafeAnchor_SpoofHotbar},
    }},
    {"Breach Swap", 0, 0, &BreachSwap_Enabled, 0, {
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &BreachSwap_SpoofHotbar},
    }},
    {"Auto Mace", 0, 0, &AutoMace_Enabled, 0, {
        {"Reichweite", SettingType::SliderInt, nullptr, &AutoMace_Range, 1, 6, " m"},
        {"FOV", SettingType::SliderInt, nullptr, &AutoMace_Fov, 10, 180, " deg"},
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &AutoMace_SpoofHotbar},
    }},
    {"Auto Pearl Catch", 0, 0, &AutoPearlCatch_Enabled, 1, {
        {"Verzoegerung", SettingType::SliderInt, nullptr, &AutoPearlCatch_Delay, 50, 500, " ms"},
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &AutoPearlCatch_SpoofHotbar},
    }},
    {"Short Wind Charge", 0, 0, &ShortWindCharge_Enabled, 1, {
        {"Vor dem Wurf springen", SettingType::Checkbox, &ShortWindCharge_Jump},
        {"Wurfverzoegerung", SettingType::SliderInt, nullptr, &ShortWindCharge_ThrowDelay, 0, 300, " ms"},
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &ShortWindCharge_SpoofHotbar},
    }},
    {"Safe Walk", 0, 0, &SafeWalk_Enabled, 1, {
        {"Nur mit Block in der Hand", SettingType::Checkbox, &SafeWalk_OnlyBlocks},
    }},
    {"AntiBot", 0, 0, &AntiBot_Enabled, 2, {
        {"Spielerliste pruefen", SettingType::Checkbox, &AntiBot_TabList},
        {"Profil-UUID pruefen", SettingType::Checkbox, &AntiBot_Profile},
        {"Ungueltige Positions-/Hitbox-Daten", SettingType::Checkbox, &AntiBot_InvalidData},
        {"Spawn-Schonfrist", SettingType::SliderInt, nullptr, &AntiBot_SpawnGrace, 20, 200, " ticks"},
    }},
    {"SilentAim", 0, 0, &Silent_Aim_Enabled, 0, {
        {"Reichweite", SettingType::SliderInt, nullptr, &SilentAim_Range, 1, 6, " m"},
        {"FOV", SettingType::SliderInt, nullptr, &SilentAim_Fov, 10, 180, " deg"},
    }},
    {"KillAura", 0, 0, &CW_Enabled, 0, {
                                              {"Enable CW", SettingType::Checkbox, &CW_Enabled},
                                              {"CW Delay", SettingType::SliderInt, nullptr, &CW_Delay, 0, 4},
                                              {"HitCrystal", SettingType::Checkbox, &HitCrystal_Enabled},
                                          }},
    {"Reach", 0, 0, &Reach_Enabled, 0, {
                                           {"Enable Reach", SettingType::Checkbox, &Reach_Enabled},
                                           {"Range", SettingType::SliderInt, nullptr, &Reach_range, 3, 6},
                                       }},
    {"Speed", 0, 0, &FastPlace_Enabled, 1, {
                                           {"FastPlace", SettingType::Checkbox, &FastPlace_Enabled},
                                           {"AutoSprint", SettingType::Checkbox, &AutoSprint_Enabled},
                                       }},
    {"NoFall", 0, 0, &NoJumpDelay_Enabled, 1, {
                                                  {"NoJumpDelay", SettingType::Checkbox, &NoJumpDelay_Enabled},
                                              }},
    {"AutoTotem", 0, 0, &AutoTotem_Enabled, 2, {
                                               {"AutoTotem", SettingType::Checkbox, &AutoTotem_Enabled},
                                           }},
    {"SeedCracker", 0, 0, &SeedCracker_Enabled, 2, {
                                                       {"SeedCracker", SettingType::Checkbox, &SeedCracker_Enabled},
                                                   }},
    {"OreSim", 0, 0, &OreSim_Enabled, 2, {
        {"Radius (Chunks)", SettingType::SliderInt, nullptr, &OreSim_Radius, 1, 6},
        {"Luft ausblenden", SettingType::Checkbox, &OreSim_AirCheck},
        {"Kohle", SettingType::Checkbox, &OreSim_Ores[0]},
        {"Eisen", SettingType::Checkbox, &OreSim_Ores[1]},
        {"Gold", SettingType::Checkbox, &OreSim_Ores[2]},
        {"Redstone", SettingType::Checkbox, &OreSim_Ores[3]},
        {"Diamant", SettingType::Checkbox, &OreSim_Ores[4]},
        {"Lapislazuli", SettingType::Checkbox, &OreSim_Ores[5]},
        {"Kupfer", SettingType::Checkbox, &OreSim_Ores[6]},
        {"Smaragd", SettingType::Checkbox, &OreSim_Ores[7]},
        {"Quarz", SettingType::Checkbox, &OreSim_Ores[8]},
        {"Antiker Schutt", SettingType::Checkbox, &OreSim_Ores[9]},
    }},
    {"Player ESP", 0, 0, &PlayerESP_Enabled, 2, {
        {"Skin statt Boxen", SettingType::Checkbox, &PlayerESP_Skin},
        {"Glow", SettingType::Checkbox, &PlayerESP_Glow},
        {"Reichweite", SettingType::SliderInt, nullptr, &PlayerESP_Range, 16, 256, " m"},
        {"Linienstaerke", SettingType::SliderInt, nullptr, &PlayerESP_LineWidth, 1, 4, " px"},
    }},
};

}

std::span<ModuleData> GetModules() { return modules; }

bool HasConflict(int key, int exceptModule) {
    if (!key) return false;
    if (key == VK_END || key == VK_ESCAPE || key == Config::menu_keybind) return true;
    for (int i=0; i<MODULE_COUNT; ++i)
        if (i!=exceptModule && modules[i].hotkey==key) return true;
    return false;
}

// On the render thread, use its JNIEnv rather than the SDK worker's JNIEnv.
static bool GameScreenOpen() {
    if (!p_jni || !p_jni->p_cminecraft || !p_jni->GetJVM()) return true;
    JNIEnv* env=nullptr;
    if (p_jni->GetJVM()->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6)!=JNI_OK || !env) return true;
    if (env->ExceptionCheck()) return true;
    jobject mc=p_jni->p_cminecraft->GetInstance();
    if (!mc) return true;
    return Input::GameScreenOpen(env, mc);
}

void ProcessHotkeys(bool blocked) {
    static Input::HotkeyState states[MODULE_COUNT];
    static Input::ActionKey actionKeys[MODULE_COUNT];
    bool hasBinding=false;
    for(const auto& module:modules) hasBinding |= module.hotkey!=0;
    DWORD process=0;
    GetWindowThreadProcessId(GetForegroundWindow(),&process);
    blocked = blocked || process!=GetCurrentProcessId() || Menu_Enabled
        || Config::capturing_menu_key || Config::capturing_module_key>=0
        || ImGui::GetIO().WantTextInput || (hasBinding && GameScreenOpen());
    for (int i=0; i<MODULE_COUNT; ++i) {
        auto& module=modules[i];
        bool down=module.hotkey>0 && (GetAsyncKeyState(module.hotkey)&0x8000)!=0;
        if (module.enabled==&ShortWindCharge_Enabled || module.enabled==&AutoPearlCatch_Enabled) {
            auto& actionKey=actionKeys[i];
            auto& request=module.enabled==&ShortWindCharge_Enabled ? ShortWindCharge_Request : AutoPearlCatch_Request;
            const bool unavailable=blocked || !module.enabled->load() || HasConflict(module.hotkey,i);
            if (unavailable) request=false;
            if (actionKey.Press(module.hotkey,down,unavailable)) request=true;
            continue;
        }
        bool before=module.enabled->load();
        bool after=states[i].Update(module.hotkey,module.hotkey_mode,down,
            blocked || module.hotkey==VK_END || module.hotkey==VK_ESCAPE || module.hotkey==Config::menu_keybind,before);
        if (after!=before) module.enabled->store(after);
    }
}

void CaptureHotkey() {
    if (!Menu_Enabled) { Config::capturing_menu_key=false; Config::capturing_module_key=-1; return; }
    int index=Config::capturing_module_key;
    bool menu=Config::capturing_menu_key;
    if (!menu && (index<0 || index>=MODULE_COUNT)) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
        Config::capturing_menu_key=false; Config::capturing_module_key=-1; return;
    }
    for (int vk=8; vk<255; ++vk) {
        if (vk==VK_END || vk==VK_ESCAPE) continue;
        auto key=Input::VkToImGuiKey(vk);
        if (key==ImGuiKey_None || !ImGui::IsKeyPressed(key,false)) continue;
        if (menu) Config::menu_keybind=vk;
        else modules[index].hotkey=vk==VK_BACK ? 0 : vk;
        Config::capturing_menu_key=false; Config::capturing_module_key=-1;
        break;
    }
}
}
