#include "../theme/theme.hpp"
#include <charconv>
#include "config.hpp"
#include "../oresim/seed_settings.hpp"
#include "../../modules/settings.hpp"

#include <filesystem>
#include <fstream>
#include <windows.h>

namespace Config {

    // ---- Globale App-Settings ----
    std::atomic<int> menu_keybind = VK_DELETE;
    bool particles_enabled = true;
    std::atomic<bool> capturing_menu_key = false;

    // ---- Interner State fuer die Config-Liste ----
    static std::vector<std::string> saved_configs;
    static bool list_loaded = false;

    static std::string ConfigDir( ) {
        return "PhantomUI/configs/";
    }

    static void EnsureConfigDir( ) {
        std::error_code ec;
        std::filesystem::create_directories(ConfigDir( ), ec);
    }

    void RefreshList( ) {
        saved_configs.clear( );
        EnsureConfigDir( );

        std::error_code ec;
        for (auto& entry : std::filesystem::directory_iterator(ConfigDir( ), ec)) {
            if (entry.path( ).extension( ) == ".cfg")
                saved_configs.push_back(entry.path( ).stem( ).string( ));
        }
        std::sort(saved_configs.begin(),saved_configs.end());
        list_loaded = true;
    }

    const std::vector<std::string>& GetList( ) {
        if (!list_loaded)
            RefreshList( );
        return saved_configs;
    }

    static bool ValidName(const std::string& name) {
        return !name.empty() && name.size() <= 64 && name != "." && name != ".."
            && name.find_first_of("/\\:*?\"<>|\r\n") == std::string::npos
            && name.back() != '.' && name.back() != ' ';
    }

    void Save(const std::string& name, ModuleData* modules, int module_count) {
        if (!ValidName(name) || !modules)
            { profile_status="Invalid profile name."; return; }

        EnsureConfigDir( );
        const auto destination=std::filesystem::path(ConfigDir()) / (name+".cfg");
        const auto temporary=std::filesystem::path(ConfigDir()) / (name+".cfg.tmp");
        std::ofstream file(temporary,std::ios::trunc);
        if (!file.is_open( )) { profile_status="Could not open profile file."; return; }

        file << "profile_version=2\n";
        file << "hud.enabled=" << (hud_enabled ? 1 : 0) << "\n";
        file << "hud.x=" << hud_x << "\n";
        file << "hud.y=" << hud_y << "\n";
        file << "oresim.seed=" << (OreSim::hasManualSeed ? std::to_string(OreSim::manualSeed) : "none") << "\n";
        file << "silent_aim=" << (Silent_Aim_Enabled ? 1 : 0) << "\n";
        file << "theme=" << std::clamp(Theme::selected,0,2) << "\n";
        file << "menu_keybind=" << menu_keybind << "\n";
        file << "particles=" << (particles_enabled ? 1 : 0) << "\n";

        for (int i = 0; i < module_count; i++) {
            file << "module." << modules[i].name << ".enabled=" << (*modules[i].enabled ? 1 : 0) << "\n";
            file << "module." << modules[i].name << ".hotkey=" << modules[i].hotkey << "\n";

            file << "module." << modules[i].name << ".hotkey_mode=" << static_cast<int>(modules[i].hotkey_mode) << "\n";

            for (auto& s : modules[i].settings_list) {
                if (s.type == SettingType::Checkbox && s.b_val)
                    file << "module." << modules[i].name << "." << s.name << "=" << (*s.b_val ? 1 : 0) << "\n";
                else if (s.type == SettingType::SliderInt && s.i_val)
                    file << "module." << modules[i].name << "." << s.name << "=" << *s.i_val << "\n";
            }
        }

        file.flush();
        bool good=static_cast<bool>(file);
        file.close();
        if(!good || file.fail() || !MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::error_code ec; std::filesystem::remove(temporary,ec);
            profile_status="Save failed; previous profile was preserved.";
            return;
        }
        active_profile=name;
        profile_status="Saved: "+name;
        RefreshList();
    }

    static bool ParseNumber(const std::string& text, int& value) {
        auto r=std::from_chars(text.data(),text.data()+text.size(),value);
        return r.ec==std::errc{} && r.ptr==text.data()+text.size();
    }

    void Load(const std::string& name, ModuleData* modules, int module_count) {
        if (!ValidName(name) || !modules)
            return;

        std::ifstream file(ConfigDir( ) + name + ".cfg");
        if (!file.is_open( )) { profile_status="Could not open profile file."; return; }

        for (int i = 0; i < module_count; ++i) modules[i].hotkey_mode = Input::HotkeyMode::Toggle;
        bool seedPresent=false;
        long long profileSeed=0;
        bool invalidSeed=false;
        std::string line;
        while (std::getline(file, line)) {
            // Falls die Datei mit CRLF endet, ein evtl. verbliebenes '\r' entfernen
            if (!line.empty( ) && line.back( ) == '\r')
                line.pop_back( );

            size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            if (val.empty( ))
                continue;

            if (key == "hud.enabled") {
                if(val=="0" || val=="1") hud_enabled=val=="1";
                continue;
            }
            if (key == "hud.x" || key == "hud.y") {
                int position=0;
                if(ParseNumber(val,position)) (key=="hud.x" ? hud_x : hud_y)=std::clamp(position,0,32768);
                continue;
            }
            if (key == "oresim.seed") {
                seedPresent = val != "none" && OreSim::ParseSeed(val,profileSeed);
                invalidSeed = val != "none" && !seedPresent;
                continue;
            }
            if (key == "silent_aim") {
                if(val=="0" || val=="1") Silent_Aim_Enabled=val=="1";
                continue;
            }
            if (key == "menu_keybind") {
                int key=0; if(ParseNumber(val,key) && key>=0 && key<=255) menu_keybind=key;
                continue;
            }
            if (key == "theme") {
                int theme=0;
                auto result=std::from_chars(val.data(),val.data()+val.size(),theme);
                if(result.ec==std::errc{} && result.ptr==val.data()+val.size() && theme>=0 && theme<3) Theme::selected=theme;
                continue;
            }
            if (key == "particles") {
                if (val == "0" || val == "1") particles_enabled = (val == "1");
                continue;
            }

            if (key.rfind("module.", 0) != 0) continue;

            // Format: module.<ModulName>.<Feld>
            size_t p1 = key.find('.');
            size_t p2 = key.find('.', p1 + 1);
            if (p1 == std::string::npos || p2 == std::string::npos)
                continue;

            std::string mod_name = key.substr(p1 + 1, p2 - p1 - 1);
            std::string field = key.substr(p2 + 1);

            for (int i = 0; i < module_count; i++) {
                if (mod_name != modules[i].name)
                    continue;

                if (field == "enabled") {
                    if (val == "0" || val == "1") *modules[i].enabled = (val == "1");
                    continue;
                }
                if (field == "hotkey_mode") {
                    int mode=0;
                    if (ParseNumber(val,mode) && mode>=0 && mode<=1)
                        modules[i].hotkey_mode=static_cast<Input::HotkeyMode>(mode);
                    continue;
                }
                if (field == "hotkey") {
                    int key=0; if(ParseNumber(val,key) && key>=0 && key<=255) modules[i].hotkey=key;
                    continue;
                }

                for (auto& s : modules[i].settings_list) {
                    if (field != s.name)
                        continue;

                    if (s.type == SettingType::Checkbox && s.b_val)
                        { if (val == "0" || val == "1") *s.b_val = (val == "1"); }
                    else if (s.type == SettingType::SliderInt && s.i_val)
                    { int number=0; if(ParseNumber(val,number)) *s.i_val=std::clamp(number,s.i_min,s.i_max); }
                }
            }
        }

        file.close();
        // Legacy profiles have no seed; never reuse another world's seed.
        OreSim::SetProfileSeed(seedPresent,profileSeed);
        capturing_menu_key=false;
        capturing_module_key=-1;
        active_profile=name;
        profile_status="Loaded: "+name+(invalidSeed ? " (invalid seed cleared)" : "");
    }

    void Delete(const std::string& name) {
        if (!ValidName(name)) return;
        std::error_code ec;
        std::filesystem::remove(ConfigDir( ) + name + ".cfg", ec);
        if(ec) { profile_status="Could not delete profile."; return; }
        if(active_profile==name) active_profile.clear();
        profile_status="Deleted: "+name;
        RefreshList();
    }

} // namespace Config
