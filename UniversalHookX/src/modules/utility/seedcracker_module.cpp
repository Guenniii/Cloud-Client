#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "../../utils/seedcracker/seedcracker_bridge.hpp"

namespace {
class SeedCrackerModule final : public RuntimeModule {
public:
    SeedCrackerModule() : RuntimeModule("SeedCracker", "Utility", SeedCracker_Enabled) {}
private:
    bool seedcracker_initialized = false;
    bool seedcracker_was_enabled = false;
    ULONGLONG last_seed_request = 0;
    ULONGLONG last_seed_init_attempt = 0;
    void Tick() override {
    // =========================================================
    // SeedCracker
    // Modul AUS = nur Scans stoppen, KEIN Reset
    // Reset nur über Panel-Button in menu.cpp
    // =========================================================

    // Wenn nie aktiv war und jetzt aus: nichts tun
    if (!SeedCracker_Enabled && !seedcracker_was_enabled)
        return;

    const ULONGLONG now = GetTickCount64( );
    const bool state_changed = SeedCracker_Enabled != seedcracker_was_enabled;

    // Modul gerade ausgeschaltet: nur Flag, KEIN Reset / kein Client-Reset-Auftrag
    if (!SeedCracker_Enabled) {
        if (state_changed) {
            std::printf("[SeedCracker] Modul deaktiviert – Daten bleiben erhalten "
                        "(Reset nur ueber Panel-Button).\n");
            seedcracker_was_enabled = false;
        }
        return;
    }

    // --- ab hier: Modul ist AN ---
    if (!state_changed && now - last_seed_request < 50)
        return;
    last_seed_request = now;

    JNIEnv* env = SeedCracker::GetMinecraftJNIEnv( );
    if (!env)
        return;

    if (!seedcracker_initialized) {
        if (last_seed_init_attempt != 0 && now - last_seed_init_attempt < 1000)
            return;
        last_seed_init_attempt = now;
        seedcracker_initialized = SeedCracker::Init(
            env,
            "C:\\Users\\okeba\\source\\repos\\UniversalHookX\\"
            "UniversalHookX\\bin\\seedcrackerbridge.jar");
        if (!seedcracker_initialized)
            return;
    }

    // Nur den "enabled"-Pfad: Scans/Tick auf dem Client-Thread.
    // RequestClientUpdate(env, false) wird NICHT mehr aufgerufen (das war der Reset).
    if (SeedCracker::RequestClientUpdate(env, true)) {
        if (state_changed) {
            std::printf("[SeedCracker] Clientthread-Auftrag: aktiviert.\n");
        }
        seedcracker_was_enabled = true;
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateSeedCracker() {
    return std::make_unique<SeedCrackerModule>();
}
