#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "AutoTotem.hpp"

namespace {
class AutoTotemModule final : public RuntimeModule {
public:
    AutoTotemModule() : RuntimeModule("AutoTotem", "Combat", AutoTotem_Enabled) {}
private:
    std::unique_ptr<AutoTotem> g_auto_totem;
    void Tick() override {
    if (AutoTotem_Enabled) {
        if (!g_auto_totem) {
            g_auto_totem = std::make_unique<AutoTotem>(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));
        }
        g_auto_totem->Tick( );
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoTotem() {
    return std::make_unique<AutoTotemModule>();
}
