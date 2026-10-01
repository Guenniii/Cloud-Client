#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/combat/aim_assist.hpp"
#include "../../input/rotation/player_targeting.hpp"
#include "../../input/game_screen.hpp"
namespace {
class AimAssistModule final : public RuntimeModule {
    std::optional<int> target;
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!AimAssist_Enabled || Silent_Aim_Enabled || Menu_Enabled || process!=GetCurrentProcessId()
            || !env || !mc || Input::GameScreenOpen(env,mc)) {
            target.reset(); AimAssist::Clear(env); return;
        }
        const auto selected=Rotation::FindPlayerTarget(env,mc,std::clamp(AimAssist_Range.load(),1,6),
            std::clamp(AimAssist_Fov.load(),10,180),target);
        if (!selected) { target.reset(); AimAssist::Clear(env); return; }
        if (!AimAssist::Publish(env,selected->id)) { target.reset(); AimAssist::Clear(env); return; }
        target=selected->id;
    }
public:
    AimAssistModule():RuntimeModule("Aim Assist","Combat",AimAssist_Enabled) {}
    ~AimAssistModule() override { AimAssist::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAimAssist() { return std::make_unique<AimAssistModule>(); }
