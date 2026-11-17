#include "module_registry.hpp"
#include "../modules/settings.hpp"
#include <Windows.h>
#include <string_view>
#include <utility>
#include "../input/key_mapping.hpp"
#include "../utils/sdk/java.hpp"
#include "../input/game_screen.hpp"
#include "../input/action_key.hpp"
#include "hotkey_toasts.hpp"

namespace Menu::Registry {
namespace {
constexpr int MODULE_COUNT = 38;

// Bind card hotkeys to the primary runtime feature, not unused UI flags.

static ModuleData modules[MODULE_COUNT] = {
    {"Target HUD",0,0,&TargetHUD_Enabled,3,{
        {"Mini-Ansicht (aus: Modern)",SettingType::Checkbox,&TargetHUD_Mini},
        {"Glow",SettingType::Checkbox,&TargetHUD_Glow},
        {"Ruestung und Entfernung",SettingType::Checkbox,&TargetHUD_Details},
        {"Vorschau im Menue",SettingType::Checkbox,&TargetHUD_Preview},
        {"Nachanzeige",SettingType::SliderInt,nullptr,&TargetHUD_Hold,500,5000," ms"},
        {"Groesse",SettingType::SliderInt,nullptr,&TargetHUD_Scale,75,150," %"},
        {"Position X",SettingType::SliderInt,nullptr,&TargetHUD_X,0,100," %"},
        {"Position Y",SettingType::SliderInt,nullptr,&TargetHUD_Y,0,100," %"},
    }},
    {"Loot Beacon",0,0,&LootBeacon_Enabled,3,{
        {"Totems",SettingType::Checkbox,&LootBeacon_Totems},
        {"Ausruestung / Waffen",SettingType::Checkbox,&LootBeacon_Equipment},
        {"Sonstige Items",SettingType::Checkbox,&LootBeacon_Other},
        {"Glow",SettingType::Checkbox,&LootBeacon_Glow},
        {"Theme-Farbe",SettingType::Checkbox,&LootBeacon_Theme},
        {"Farbe",SettingType::ColorRGB,nullptr,&LootBeacon_Color},
        {"Reichweite",SettingType::SliderInt,nullptr,&LootBeacon_Range,8,128," m"},
        {"Hoehe",SettingType::SliderInt,nullptr,&LootBeacon_Height,1,8," m"},
        {"Breite",SettingType::SliderInt,nullptr,&LootBeacon_Width,1,6," px"},
        {"Deckkraft",SettingType::SliderInt,nullptr,&LootBeacon_Opacity,10,100,"%"},
    }},
    {"Echo Trail",0,0,&EchoTrail_Enabled,3,{
        {"Eigene Spur",SettingType::Checkbox,&EchoTrail_Self},
        {"Glow",SettingType::Checkbox,&EchoTrail_Glow},
        {"Theme-Farben",SettingType::Checkbox,&EchoTrail_Theme},
        {"Farbe",SettingType::ColorRGB,nullptr,&EchoTrail_Color},
        {"Dauer",SettingType::SliderInt,nullptr,&EchoTrail_Duration,500,6000," ms"},
        {"Linienstaerke",SettingType::SliderInt,nullptr,&EchoTrail_Width,1,6," px"},
        {"Reichweite",SettingType::SliderInt,nullptr,&EchoTrail_Range,16,256," m"},
    }},
    {"Item ESP",0,0,&ItemESP_Enabled,3,{
        {"Totems",SettingType::Checkbox,&ItemESP_Totems},
        {"Ausruestung / Waffen",SettingType::Checkbox,&ItemESP_Equipment},
        {"Sonstige Items",SettingType::Checkbox,&ItemESP_Other},
        {"Namen und Stackgroesse",SettingType::Checkbox,&ItemESP_Labels},
        {"Glow",SettingType::Checkbox,&ItemESP_Glow},
        {"Reichweite",SettingType::SliderInt,nullptr,&ItemESP_Range,8,128," m"},
        {"Farbe: Totems",SettingType::ColorRGB,nullptr,&ItemESP_Colors[0]},
        {"Farbe: Ausruestung",SettingType::ColorRGB,nullptr,&ItemESP_Colors[1]},
        {"Farbe: Sonstige",SettingType::ColorRGB,nullptr,&ItemESP_Colors[2]},
    }},
    {"Friends",0,0,&Friends_Enabled,2,{
        {"Freunde schuetzen",SettingType::Checkbox,&Friends_Enabled},
        {"Mit Mittelklick hinzufuegen",SettingType::Checkbox,&Friends_MiddleClick},
        {"ESP-Farbe",SettingType::ColorRGB,nullptr,&Friends_Color},
    }},
    {"Explosion Preview",0,0,&ExplosionPreview_Enabled,3,{
        {"Kristalle",SettingType::Checkbox,&ExplosionPreview_Crystals},
        {"Geladene Anker",SettingType::Checkbox,&ExplosionPreview_Anchors},
        {"Zielschaden anzeigen",SettingType::Checkbox,&ExplosionPreview_Target},
        {"Glow",SettingType::Checkbox,&ExplosionPreview_Glow},
        {"Reichweite",SettingType::SliderInt,nullptr,&ExplosionPreview_Range,4,24," m"},
    }},
    {"Projectile Prediction",0,0,&ProjectilePrediction_Enabled,3,{
        {"Vorschau fuer gehaltenes Item",SettingType::Checkbox,&ProjectilePrediction_Held},
        {"Fliegende Projektile",SettingType::Checkbox,&ProjectilePrediction_Flying},
        {"Aufprallmarker",SettingType::Checkbox,&ProjectilePrediction_Impact},
        {"Aufprallblock hervorheben",SettingType::Checkbox,&ProjectilePrediction_HighlightBlock},
        {"Glow",SettingType::Checkbox,&ProjectilePrediction_Glow},
        {"Reichweite",SettingType::SliderInt,nullptr,&ProjectilePrediction_Range,16,256," m"},
        {"Simulationsdauer",SettingType::SliderInt,nullptr,&ProjectilePrediction_Ticks,20,160," ticks"},
        {"Linienstaerke",SettingType::SliderInt,nullptr,&ProjectilePrediction_LineWidth,1,4," px"},
    }},
    {"Sound ESP",0,0,&SoundESP_Enabled,3,{
        {"Schritte",SettingType::Checkbox,&SoundESP_Steps},
        {"Projektile / Wuerfe",SettingType::Checkbox,&SoundESP_Projectiles},
        {"Explosionen",SettingType::Checkbox,&SoundESP_Explosions},
        {"Richtungsmarker am Bildschirmrand",SettingType::Checkbox,&SoundESP_Directions},
        {"Glow",SettingType::Checkbox,&SoundESP_Glow},
        {"Reichweite",SettingType::SliderInt,nullptr,&SoundESP_Range,8,128," m"},
        {"Anzeigedauer",SettingType::SliderInt,nullptr,&SoundESP_Lifetime,500,5000," ms"},
    }},
    {"Storage ESP",0,0,&StorageESP_Enabled,3,{
        {"Truhen",SettingType::Checkbox,&StorageESP_Chests},
        {"Faesser",SettingType::Checkbox,&StorageESP_Barrels},
        {"Shulker-Boxen",SettingType::Checkbox,&StorageESP_Shulkers},
        {"Endertruhen",SettingType::Checkbox,&StorageESP_EnderChests},
        {"Theme-Farbe statt eigener Farben",SettingType::Checkbox,&StorageESP_ThemeColors},
        {"Farbe: Truhen",SettingType::ColorRGB,nullptr,&StorageESP_Colors[0]},
        {"Farbe: Faesser",SettingType::ColorRGB,nullptr,&StorageESP_Colors[1]},
        {"Farbe: Shulker",SettingType::ColorRGB,nullptr,&StorageESP_Colors[2]},
        {"Farbe: Endertruhen",SettingType::ColorRGB,nullptr,&StorageESP_Colors[3]},
        {"Beschriftungen",SettingType::Checkbox,&StorageESP_Labels},
        {"Glow",SettingType::Checkbox,&StorageESP_Glow},
        {"Reichweite",SettingType::SliderInt,nullptr,&StorageESP_Range,16,128," m"},
        {"Linienstaerke",SettingType::SliderInt,nullptr,&StorageESP_LineWidth,1,4," px"},
    }},

    {"Auto Armor", 0, 0, &AutoArmor_Enabled, 2, {
        {"Console-Diagnose", SettingType::Checkbox, &AutoArmor_Logs},
        {"Elytra behalten", SettingType::Checkbox, &AutoArmor_KeepElytra},
        {"Inventar-Delay", SettingType::SliderInt, nullptr, &AutoArmor_Delay, 50, 1000, " ms"},
    }},
    {"Refill", 0, 0, &Refill_Enabled, 2, {
        {"Bei Restmenge auffuellen", SettingType::SliderInt, nullptr, &Refill_Threshold, 1, 63, " Items"},
        {"Inventar-Delay", SettingType::SliderInt, nullptr, &Refill_Delay, 50, 1000, " ms"},
        {"Totems nachlegen", SettingType::Checkbox, &Refill_Totems},
    }},
    {"Hit Effect", 0, 0, &HitEffect_Enabled, 3, {
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
    {"Trigger Bot", 0, 0, &TriggerBot_Enabled, 0, {
        {"Reaktionszeit", SettingType::SliderInt, nullptr, &TriggerBot_Delay, 0, 500, " ms"},
        {"Nur mit Nahkampfwaffe", SettingType::Checkbox, &TriggerBot_OnlyWeapon},
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
    }, Input::HotkeyMode::Toggle, false, &AutoPearlCatch_Request},
    {"Short Wind Charge", 0, 0, &ShortWindCharge_Enabled, 1, {
        {"Vor dem Wurf springen", SettingType::Checkbox, &ShortWindCharge_Jump},
        {"Wurfverzoegerung", SettingType::SliderInt, nullptr, &ShortWindCharge_ThrowDelay, 0, 300, " ms"},
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &ShortWindCharge_SpoofHotbar},
    }, Input::HotkeyMode::Toggle, false, &ShortWindCharge_Request},
    {"Key Pearl", 0, 0, &KeyPearl_Enabled, 1, {
        {"Hotbar spoofing (aus: normal)", SettingType::Checkbox, &KeyPearl_SpoofHotbar},
    }, Input::HotkeyMode::Toggle, false, &KeyPearl_Request},
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
    {"Auto Crystal", 0, 0, &CW_Enabled, 0, {
                                              {"CW Delay", SettingType::SliderInt, nullptr, &CW_Delay, 0, 4, " ticks"},
                                              {"HitCrystal", SettingType::Checkbox, &HitCrystal_Enabled},
                                              {"HitCrystal: Obsidian-Delay", SettingType::SliderInt, nullptr, &HitCrystal_ObsidianDelay, 25, 250, " ms"},
                                              {"HitCrystal: Kristall-Delay", SettingType::SliderInt, nullptr, &HitCrystal_CrystalDelay, 40, 300, " ms"},
                                              {"HitCrystal: Pause danach", SettingType::SliderInt, nullptr, &HitCrystal_Cooldown, 50, 1000, " ms"},
                                          }},
    {"Reach", 0, 0, &Reach_Enabled, 0, {
                                           {"Enable Reach", SettingType::Checkbox, &Reach_Enabled},
                                           {"Range", SettingType::SliderInt, nullptr, &Reach_range, 3, 6},
                                       }},
    {"FastPlace", 0, 0, &FastPlace_Enabled, 1, {
                                           {"FastPlace", SettingType::Checkbox, &FastPlace_Enabled},
                                       }},
    {"AutoSprint", 0, 0, &AutoSprint_Enabled, 1, {
                                                   {"AutoSprint", SettingType::Checkbox, &AutoSprint_Enabled},
                                               }},

    {"NoJumpDelay", 0, 0, &NoJumpDelay_Enabled, 1, {
                                                  {"NoJumpDelay", SettingType::Checkbox, &NoJumpDelay_Enabled},
                                              }},
    {"AutoTotem", 0, 0, &AutoTotem_Enabled, 2, {
                                               {"AutoTotem", SettingType::Checkbox, &AutoTotem_Enabled},
                                               {"Delay nach Pop", SettingType::SliderInt, nullptr, &AutoTotem_PopDelay, 0, 1000, " ms"},
                                               {"Inventar-Delay", SettingType::SliderInt, nullptr, &AutoTotem_SwapDelay, 0, 500, " ms"},
                                               {"Totem-Suchintervall", SettingType::SliderInt, nullptr, &AutoTotem_RetryInterval, 100, 5000, " ms"},
                                           }},
    {"SeedCracker", 0, 0, &SeedCracker_Enabled, 2, {
                                                       {"Scanradius (Chunks)", SettingType::SliderInt, nullptr, &SeedCracker_Radius, 1, 16, " chunks"},
        {"Seed automatisch an OreSim", SettingType::Checkbox, &SeedCracker_AutoApplySeed},
        {"Shipwreck-Boxen", SettingType::Checkbox, &SeedCracker_Boxes},
        {"Box-Glow", SettingType::Checkbox, &SeedCracker_BoxGlow},
        {"Linienstaerke", SettingType::SliderInt, nullptr, &SeedCracker_BoxWidth, 1, 4, " px"},
                                                   }},
    {"OreSim", 0, 0, &OreSim_Enabled, 3, {
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
    {"Player ESP", 0, 0, &PlayerESP_Enabled, 3, {
        {"Skin statt Boxen", SettingType::Checkbox, &PlayerESP_Skin},
        {"Glow", SettingType::Checkbox, &PlayerESP_Glow},
        {"Reichweite", SettingType::SliderInt, nullptr, &PlayerESP_Range, 16, 256, " m"},
        {"Linienstaerke", SettingType::SliderInt, nullptr, &PlayerESP_LineWidth, 1, 4, " px"},
    }},
    {"Totem Warner", 0, 0, &TotemWarner_Enabled, 3, {
        {"Reichweite", SettingType::SliderInt, nullptr, &TotemWarner_Range, 8, 128, " m"},
    }},
    {"Info HUD", 0, 0, &InfoHUD_Enabled, 3, {
        {"FPS und Uhrzeit", SettingType::Checkbox, &InfoHUD_Clock},
        {"Groesse", SettingType::SliderInt, nullptr, &InfoHUD_Scale, 75, 150, " %"},
    }},
};

// Capture factory settings once, before profiles or runtime actions can change them.
const auto factory_values = [] {
    std::vector<std::vector<int>> values;
    for (const auto& module : modules) {
        std::vector<int> settings;
        for (const auto& setting : module.settings_list)
            settings.push_back(setting.type == SettingType::Checkbox ? setting.b_val->load() : setting.i_val->load());
        values.push_back(std::move(settings));
    }
    return values;
}();

}

std::span<ModuleData> GetModules() { return modules; }

void ResetModule(int index) {
    if (index < 0 || index >= MODULE_COUNT) return;
    auto& module = modules[index];
    *module.enabled = false;
    module.hotkey = 0;
    module.hotkey_mode = Input::HotkeyMode::Toggle;
    for (size_t i = 0; i < module.settings_list.size(); ++i) {
        auto& setting = module.settings_list[i];
        if (setting.type == SettingType::Checkbox) *setting.b_val = factory_values[index][i] != 0;
        else *setting.i_val = factory_values[index][i];
    }
    *module.enabled = false;
    if (Config::capturing_module_key == index) Config::capturing_module_key = -1;
}

const char* Description(const char* name) {
    using namespace std::literals;
    const std::pair<std::string_view, const char*> descriptions[] = {
        {"Target HUD", "Shows the last player you attacked, with live stats until the configured delay expires. Drag the preview while the menu is open."},
        {"Loot Beacon", "Marks selected dropped items with a vertical light beam; works without Item ESP."},
        {"Echo Trail", "Draws fading movement paths behind players; respects the global AntiBot filter."},
        {"Item ESP", "Highlights dropped items, with separate colors for each item group."},
        {"Friends", "Protects saved friends from targeting and gives them a distinct ESP color."},
        {"Explosion Preview", "Shows estimated explosion damage for nearby crystals and charged anchors."},
        {"Projectile Prediction", "Previews projectile paths and their estimated impact point."},
        {"Sound ESP", "Displays nearby sounds and their direction as visual markers."},
        {"Storage ESP", "Highlights storage blocks with configurable colors and range."},
        {"Auto Armor", "Equips better armor from your inventory when its conditions are met."},
        {"Trigger Bot", "Attacks the player under your crosshair once the attack cooldown is charged."},
        {"Refill", "Refills hotbar stacks from matching items in your inventory."},
        {"Hit Effect", "Adds custom glowing particles to hits."},
        {"Predict Double Hand", "Selects a hotbar totem when the configured danger checks trigger."},
        {"Auto Shield Breaker", "Uses an axe against a player who is blocking with a shield."},
        {"Fake Lag", "Delays outgoing movement updates according to its configured limits."},
        {"Aim Assist", "Adjusts your visible aim towards eligible nearby targets."},
        {"Auto Tool", "Selects a suitable tool for the block you are mining."},
        {"Safe Anchor", "Coordinates anchor placement, charging and detonation."},
        {"Breach Swap", "Coordinates the weapon switch following a sword attack."},
        {"Auto Mace", "Coordinates mace attacks while falling towards an eligible target."},
        {"Auto Pearl Catch", "Coordinates the pearl and wind-charge action on its hotkey."},
        {"Short Wind Charge", "Uses a wind charge for the configured jump action."},
        {"Key Pearl", "Throws an ender pearl along your view on its hotkey, without a visible slot change when spoofing is on."},
        {"Totem Warner", "Labels nearby enemy players who hold no totem in either hand."},
        {"Safe Walk", "Helps prevent walking off block edges."},
        {"AntiBot", "Filters suspected bots using the enabled player checks."},
        {"SilentAim", "Adjusts attack targeting towards eligible players."},
        {"Auto Crystal", "Automatically destroys placed crystals."},
        {"Reach", "Controls the configured attack-range options."},
        {"FastPlace", "Removes the right-click delay when placing blocks."},
        {"AutoSprint", "Sprints automatically while moving forward and not sneaking."},
        {"NoJumpDelay", "Removes the delay between jumps."},
        {"AutoTotem", "Maintains totem protection using available inventory items."},
        {"SeedCracker", "Collects structure data, verifies candidate world seeds and hands them to the ore simulation."},
        {"OreSim", "Predicts ore locations using the world seed entered manually."},
        {"Player ESP", "Highlights players using boxes or the configured skin rendering."},
        {"Info HUD", "Lists all other active modules, plus FPS and clock. Drag it while the menu is open; the position is saved in profiles."}
    };
    for (const auto& [key, text] : descriptions) if (key == name) return text;
    return "Configure the behavior of this module below.";
}

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
        if (module.action_request) {
            auto& actionKey=actionKeys[i];
            auto& request=*module.action_request;
            const bool unavailable=blocked || !module.enabled->load() || HasConflict(module.hotkey,i);
            if (unavailable) request=false;
            if (actionKey.Press(module.hotkey,down,unavailable)) request=true;
            continue;
        }
        bool before=module.enabled->load();
        bool after=states[i].Update(module.hotkey,module.hotkey_mode,down,
            blocked || module.hotkey==VK_END || module.hotkey==VK_ESCAPE || module.hotkey==Config::menu_keybind,before);
        if (after!=before) {
            module.enabled->store(after);
            if (module.hotkey_mode==Input::HotkeyMode::Toggle) HotkeyToasts::Push(module.name,after); // Hold would toast on every press/release
        }
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
