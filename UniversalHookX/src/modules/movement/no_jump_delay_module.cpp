#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "NoJumpDelay.hpp"

namespace {
class NoJumpDelayModule final : public RuntimeModule {
public:
    NoJumpDelayModule() : RuntimeModule("NoJumpDelay", "Movement", NoJumpDelay_Enabled) {}
private:
    void Tick() override {
    if (NoJumpDelay_Enabled) { // Angenommen, du hast eine Checkbox/Variable dafür
        // Wir nutzen die statische Methode aus der .hpp
        // Da du p_cminecraft.get() nutzt, um an die Instanz zu kommen:
        NoJumpDelay::Run(p_jni->GetEnv( ), p_jni->p_cminecraft.get( ));
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateNoJumpDelay() {
    return std::make_unique<NoJumpDelayModule>();
}
