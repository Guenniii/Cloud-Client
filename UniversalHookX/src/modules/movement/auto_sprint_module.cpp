#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>
#include "Sprint.hpp"

namespace {
class AutoSprintModule final : public RuntimeModule {
public:
    AutoSprintModule() : RuntimeModule("AutoSprint", "Movement", AutoSprint_Enabled) {}
private:
    void Tick() override {
    if (AutoSprint_Enabled) {
        AutoSprint::Run(p_jni->GetEnv( ), p_jni->p_cminecraft.get( ));
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoSprint() {
    return std::make_unique<AutoSprintModule>();
}
