#pragma once

#include <string>
#include <atomic>
#include <vector>
#include "../../input/hotkey_state.hpp"

// ==========================================
// Struct-Definitionen fuer Module & ihre Settings.
// Liegen hier (statt in menu.cpp), damit sowohl menu.cpp als auch
// config.cpp ohne gegenseitige Abhaengigkeit darauf zugreifen koennen.
// ==========================================
enum class SettingType { Checkbox,
                         SliderInt };

struct ModuleSetting {
    const char* name;
    SettingType type;
    std::atomic<bool>* b_val = nullptr;
    std::atomic<int>* i_val = nullptr;
    int i_min = 0;
    int i_max = 100;
    const char* suffix = ""; // z.B. "m", "%%", "ms"
};

struct ModuleData {
    const char* name;
    int hotkey = 0; // Virtual-Key-Code, 0 = kein Hotkey gesetzt
    int active_settings;
    std::atomic<bool>* enabled;
    int category;
    std::vector<ModuleSetting> settings_list;
    Input::HotkeyMode hotkey_mode = Input::HotkeyMode::Toggle;
};

namespace Config {
    inline bool hud_enabled=true;
    inline int hud_x=20, hud_y=20;
    inline std::string active_profile;
    inline std::string profile_status;
    inline std::atomic<int> capturing_module_key{-1};

    // ---- Globale App-Settings (Settings-Tab) ----
    extern std::atomic<int> menu_keybind;        // Taste zum Oeffnen/Schliessen des Menues
    extern bool particles_enabled;  // Partikel-Hintergrund an/aus
    extern std::atomic<bool> capturing_menu_key; // true waehrend auf einen neuen Menue-Hotkey gewartet wird

    // ---- Config-Datei-Verwaltung (Config-Tab) ----

    // Laedt die Liste der vorhandenen .cfg-Dateien aus dem Config-Ordner neu ein.
    void RefreshList( );

    // Gibt die zuletzt geladene Liste der Config-Namen zurueck (laedt beim ersten Aufruf automatisch).
    const std::vector<std::string>& GetList( );

    // Speichert den aktuellen Zustand (Module + App-Settings) unter dem angegebenen Namen.
    void Save(const std::string& name, ModuleData* modules, int module_count);

    // Laedt eine gespeicherte Config und wendet sie auf Module + App-Settings an.
    void Load(const std::string& name, ModuleData* modules, int module_count);

    // Loescht eine gespeicherte Config-Datei.
    void Delete(const std::string& name);

} // namespace Config
