#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/combat/breach_swap.hpp"
#include "../../input/game_screen.hpp"
namespace {
class BreachSwapModule final : public RuntimeModule {
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!BreachSwap_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || !env || !mc || Input::GameScreenOpen(env,mc)) {
            BreachSwap::Clear(env); return;
        }
        BreachSwap::Publish(env);
    }
public:
    BreachSwapModule():RuntimeModule("Breach Swap","Combat",BreachSwap_Enabled) {}
    ~BreachSwapModule() override { BreachSwap::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateBreachSwap() { return std::make_unique<BreachSwapModule>(); }
