#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "Reach.hpp"

namespace {
class ReachModule final : public RuntimeModule {
public:
    ReachModule() : RuntimeModule("Reach", "Combat", Reach_Enabled) {}
private:
    std::unique_ptr<Reach> g_reach;
    ULONGLONG g_last_reach_write = 0;
    bool g_reach_was_enabled = false;
    void Tick() override {
    if (Reach_Enabled) {
        if (!g_reach) {
            g_reach = std::make_unique<Reach>(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }

        // Original einmalig sichern, bevor überhaupt geschrieben wird
        g_reach->CaptureOriginal( );

        const ULONGLONG now = GetTickCount64( );
        if (now - g_last_reach_write >= 150) {
            g_reach->SetReach(Reach_range);
            g_last_reach_write = now;
        }

        g_reach_was_enabled = true;
    } else {
        if (g_reach_was_enabled && g_reach) {
            printf("[*] Reach disabled, restoring default...\n");
            g_reach->RestoreDefault( );
            g_reach_was_enabled = false;
        }
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateReach() {
    return std::make_unique<ReachModule>();
}
