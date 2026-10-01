#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/combat/safe_anchor.hpp"
#include "../../input/game_screen.hpp"
namespace {
class SafeAnchorModule final : public RuntimeModule {
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!SafeAnchor_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || !env || !mc || Input::GameScreenOpen(env,mc)) {
            SafeAnchor::Clear(env); return;
        }
        SafeAnchor::Publish(env);
    }
public:
    SafeAnchorModule():RuntimeModule("Safe Anchor","Combat",SafeAnchor_Enabled) {}
    ~SafeAnchorModule() override { SafeAnchor::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateSafeAnchor() { return std::make_unique<SafeAnchorModule>(); }
