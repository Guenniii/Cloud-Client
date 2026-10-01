#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "HitCrystal.hpp"

namespace {
class HitCrystalModule final : public RuntimeModule {
public:
    HitCrystalModule() : RuntimeModule("HitCrystal", "Combat", HitCrystal_Enabled) {}
private:
    std::unique_ptr<HitCrystal> g_sword_obsidian;
    void Tick() override {
    if (HitCrystal_Enabled) {
        if (!g_sword_obsidian) {
            g_sword_obsidian = std::make_unique<HitCrystal>(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }

        if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
            g_sword_obsidian->Run( );
        }
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateHitCrystal() {
    return std::make_unique<HitCrystalModule>();
}
