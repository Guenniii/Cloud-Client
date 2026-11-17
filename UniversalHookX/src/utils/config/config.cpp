#include "../theme/theme.hpp"
#include <charconv>
#include "config.hpp"
#include "../../modules/color_rgb.hpp"
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

    static bool SaveInternal(const std::string& name, ModuleData* modules, int module_count, bool overwrite, bool includeSeed) {
        if (!ValidName(name) || !modules)
            { profile_status="Invalid profile name."; return false; }

        EnsureConfigDir( );
        const auto destination=std::filesystem::path(ConfigDir()) / (name+".cfg");
        const auto temporary=std::filesystem::path(ConfigDir()) / (name+".cfg.tmp");
        std::ofstream file(temporary,std::ios::trunc);
        if (!file.is_open( )) { profile_status="Could not open profile file."; return false; }

        file << "profile_version=2\n";
        file << "ui.menu_size=" << menu_size_percent.load() << "\n";
        file << "ui.font_size=" << font_size_percent.load() << "\n";
        file << "hud.enabled=" << (hud_enabled ? 1 : 0) << "\n";
        file << "hud.x=" << hud_x << "\n";
        file << "hud.y=" << hud_y << "\n";
        file << "watermark.enabled=" << (watermark_enabled ? 1 : 0) << "\n";
        file << "watermark.x=" << watermark_x << "\n";
        file << "watermark.y=" << watermark_y << "\n";
        // A seed taken from the SeedCracker belongs to one world and is not reused by a profile.
    const bool persist_seed = includeSeed && OreSim::hasManualSeed && !OreSim::seedFromCracker;
    file << "oresim.seed=" << (persist_seed ? std::to_string(OreSim::manualSeed) : "none") << "\n";
        file << "silent_aim=" << (Silent_Aim_Enabled ? 1 : 0) << "\n";
        file << "studio.gradient=" << (Theme::gradient_enabled ? 1 : 0) << "\n";
        file << "studio.secondary=" << Theme::secondary_rgb.load() << "\n";
        file << "studio.strength=" << Theme::gradient_strength.load() << "\n";
        file << "studio.animate=" << (Theme::animated_accent ? 1 : 0) << "\n";
        file << "studio.period=" << Theme::animation_period.load() << "\n";
        file << "studio.player_colors=" << (Theme::player_colors_enabled ? 1 : 0) << "\n";
        file << "studio.friend=" << Theme::friend_rgb.load() << "\n";
        file << "studio.enemy=" << Theme::enemy_rgb.load() << "\n";
        file << "theme.custom_accent=" << (Theme::custom_accent_enabled ? 1 : 0) << "\n";
        file << "theme.custom_background=" << (Theme::custom_background_enabled ? 1 : 0) << "\n";
        file << "theme.background_rgb=" << Theme::custom_background_rgb.load() << "\n";
        file << "theme.accent_rgb=" << Theme::custom_accent_rgb.load() << "\n";
        file << "theme.menu_glow=" << Theme::menu_glow_percent.load() << "\n";
        file << "theme.esp_glow=" << Theme::esp_glow_percent.load() << "\n";
        file << "theme=" << std::clamp(Theme::selected,0,Theme::Count-1) << "\n";
        file << "menu_keybind=" << menu_keybind << "\n";
        file << "particles=" << (particles_enabled ? 1 : 0) << "\n";
        file << "ui.hotkey_toasts=" << (hotkey_toasts ? 1 : 0) << "\n";
        file << "ui.menu_style=" << menu_style << "\n";

        for (int i = 0; i < module_count; i++) {
            file << "module." << modules[i].name << ".enabled=" << (*modules[i].enabled ? 1 : 0) << "\n";
            file << "module." << modules[i].name << ".favorite=" << (modules[i].favorite ? 1 : 0) << "\n";
            file << "module." << modules[i].name << ".hotkey=" << modules[i].hotkey << "\n";

            file << "module." << modules[i].name << ".hotkey_mode=" << static_cast<int>(modules[i].hotkey_mode) << "\n";

            for (auto& s : modules[i].settings_list) {
                if (s.type == SettingType::Checkbox && s.b_val)
                    file << "module." << modules[i].name << "." << s.name << "=" << (*s.b_val ? 1 : 0) << "\n";
                else if ((s.type == SettingType::SliderInt || s.type == SettingType::ColorRGB) && s.i_val)
                    file << "module." << modules[i].name << "." << s.name << "=" << *s.i_val << "\n";
            }
        }

        file.flush();
        bool good=static_cast<bool>(file);
        file.close();
        if(!good || file.fail() || !MoveFileExW(temporary.c_str(),destination.c_str(),(overwrite ? MOVEFILE_REPLACE_EXISTING : 0) | MOVEFILE_WRITE_THROUGH)) {
            std::error_code ec; std::filesystem::remove(temporary,ec);
            profile_status="Save failed; existing profiles were preserved.";
            return false;
        }
        active_profile=name;
        profile_status="Saved: "+name;
        RefreshList();
        return true;
    }

    void Save(const std::string& name, ModuleData* modules, int module_count) {
        SaveInternal(name,modules,module_count,true,true);
    }

    bool SaveNewProfile(const std::string& name, ModuleData* modules, int module_count, bool includeSeed) {
        return SaveInternal(name,modules,module_count,false,includeSeed);
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

        // Hotkeys belong to the profile: keys it does not set are cleared, not kept.
        for (int i = 0; i < module_count; ++i) {
            modules[i].hotkey = 0;
            modules[i].hotkey_mode = Input::HotkeyMode::Toggle;
            modules[i].favorite = false;
        }
        hotkey_toasts=true;
        hud_enabled=false;watermark_enabled=true;watermark_x=-1;watermark_y=20;
        Theme::gradient_enabled=false;Theme::animated_accent=false;Theme::player_colors_enabled=false;
        Theme::secondary_rgb=0xF459D5;Theme::gradient_strength=18;Theme::animation_period=8;
        Theme::friend_rgb=0x4BDBB7;Theme::enemy_rgb=0xF4597B;
        Theme::custom_accent_enabled = false;
        Theme::custom_accent_rgb = 0x6F42FF;
        Theme::custom_background_enabled = false;
        Theme::custom_background_rgb = 0xF8F9FC;
        Theme::menu_glow_percent = Theme::esp_glow_percent = 100;
        menu_size_percent = 100;
        font_size_percent = 100;
        menu_style = 2; // profiles without the key (older builds) fall back to Orbit
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

            if (key == "ui.menu_size" || key == "ui.font_size") {
                int value = 100;
                if (ParseNumber(val, value)) {
                    if (key == "ui.menu_size") menu_size_percent = std::clamp(value, 80, 120);
                    else font_size_percent = std::clamp(value, 90, 115);
                }
                continue;
            }
            if (key == "watermark.enabled") {
                if(val=="0"||val=="1")watermark_enabled=val=="1";
                continue;
            }
            if (key == "watermark.x"||key == "watermark.y") {
                int position=0;
                if(ParseNumber(val,position))(key=="watermark.x"?watermark_x:watermark_y)=std::clamp(position,key=="watermark.x"?-1:0,32768);
                continue;
            }
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
            if (key.rfind("studio.",0)==0) {
                int value=0;
                if(ParseNumber(val,value)) {
                    if(value==0 || value==1) {
                        if(key=="studio.gradient") Theme::gradient_enabled=value!=0;
                        if(key=="studio.animate") Theme::animated_accent=value!=0;
                        if(key=="studio.player_colors") Theme::player_colors_enabled=value!=0;
                    }
                    if(key=="studio.strength") Theme::gradient_strength=std::clamp(value,0,50);
                    if(key=="studio.period") Theme::animation_period=std::clamp(value,2,20);
                    if(value>=0 && value<=0xFFFFFF) {
                        if(key=="studio.secondary") Theme::secondary_rgb=value;
                        if(key=="studio.friend") Theme::friend_rgb=value;
                        if(key=="studio.enemy") Theme::enemy_rgb=value;
                    }
                }
                continue;
            }
            if (key == "theme.custom_background") {
                if (val == "0" || val == "1") Theme::custom_background_enabled = val == "1";
                continue;
            }
            if (key == "theme.background_rgb") {
                int value=0;
                if (ParseNumber(val,value) && value>=0 && value<=0xFFFFFF) Theme::custom_background_rgb=value;
                continue;
            }
            if (key == "theme.custom_accent") {
                if (val == "0" || val == "1") Theme::custom_accent_enabled = val == "1";
                continue;
            }
            if (key == "theme.accent_rgb" || key == "theme.menu_glow" || key == "theme.esp_glow") {
                int value=0;
                if (ParseNumber(val,value)) {
                    if (key == "theme.accent_rgb" && value>=0 && value<=0xFFFFFF) Theme::custom_accent_rgb=value;
                    else if (key == "theme.menu_glow") Theme::menu_glow_percent=std::clamp(value,0,200);
                    else if (key == "theme.esp_glow") Theme::esp_glow_percent=std::clamp(value,0,200);
                }
                continue;
            }
            if (key == "theme") {
                int theme=0;
                auto result=std::from_chars(val.data(),val.data()+val.size(),theme);
                if(result.ec==std::errc{} && result.ptr==val.data()+val.size() && theme>=0 && theme<Theme::Count) Theme::selected=theme;
                continue;
            }
            if (key == "ui.menu_style") {
                int style = 1;
                menu_style = (ParseNumber(val, style) && style >= 0 && style <= 2) ? style : 2; // unknown values -> Orbit
                continue;
            }
            if (key == "ui.hotkey_toasts") {
                if (val == "0" || val == "1") hotkey_toasts = (val == "1");
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

            std::string field = key.substr(p2 + 1);
            std::string mod_name = CurrentModuleName(key.substr(p1 + 1, p2 - p1 - 1), field);

            for (int i = 0; i < module_count; i++) {
                if (mod_name != modules[i].name)
                    continue;

                if (field == "enabled") {
                    if (val == "0" || val == "1") *modules[i].enabled = (val == "1");
                    continue;
                }
                if (field == "favorite") {
                    if (val == "0" || val == "1") modules[i].favorite = val == "1";
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
                    else if (s.type == SettingType::ColorRGB && s.i_val)
                    { int rgb=0; if(ParseNumber(val,rgb) && ColorRGB::Valid(rgb)) *s.i_val=rgb; }
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
