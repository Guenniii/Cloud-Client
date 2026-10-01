#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include <Windows.h>
#include <cstdio>

namespace {
class FastPlaceModule final : public RuntimeModule {
public:
    FastPlaceModule() : RuntimeModule("FastPlace", "Utility", FastPlace_Enabled) {}
private:
    void Tick() override {
    if (FastPlace_Enabled) {
        p_jni->p_cminecraft->SetRightClickDelay(0);
    }
    }
};
}

std::unique_ptr<ModuleBase> RuntimeModules::CreateFastPlace() {
    return std::make_unique<FastPlaceModule>();
}
