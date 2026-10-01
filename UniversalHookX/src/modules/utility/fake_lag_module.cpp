#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/fake_lag.hpp"
#include "../../input/game_screen.hpp"
namespace {
class FakeLagModule final : public RuntimeModule {
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!FakeLag_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || !env || !mc || Input::GameScreenOpen(env,mc)) {
            FakeLag::Clear(env); return;
        }
        FakeLag::Publish(env);
    }
public:
    FakeLagModule():RuntimeModule("Fake Lag","Utility",FakeLag_Enabled) {}
    ~FakeLagModule() override { FakeLag::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateFakeLag() { return std::make_unique<FakeLagModule>(); }
