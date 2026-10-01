#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/auto_tool.hpp"
#include "../../input/game_screen.hpp"
namespace {
class AutoToolModule final : public RuntimeModule {
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!AutoTool_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || !env || !mc || Input::GameScreenOpen(env,mc)) {
            AutoTool::Clear(env); return;
        }
        AutoTool::Publish(env);
    }
public:
    AutoToolModule():RuntimeModule("Auto Tool","Utility",AutoTool_Enabled) {}
    ~AutoToolModule() override { AutoTool::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoTool() { return std::make_unique<AutoToolModule>(); }
