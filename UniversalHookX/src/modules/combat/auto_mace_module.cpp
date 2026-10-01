#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/combat/auto_mace.hpp"
#include "../../input/rotation/player_targeting.hpp"
#include "../../input/game_screen.hpp"

namespace {
class AutoMaceModule final : public RuntimeModule {
    std::optional<int> target;
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!AutoMace_Enabled || Menu_Enabled || process!=GetCurrentProcessId() || Input::GameScreenOpen(env,mc)) {
            target.reset(); AutoMace::Clear(env); return;
        }
        const auto selected=Rotation::FindPlayerTarget(env,mc,std::clamp(AutoMace_Range.load(),1,6),
                                                      std::clamp(AutoMace_Fov.load(),10,180),target);
        if (!selected) { target.reset(); AutoMace::Publish(env,-1); return; }
        if (!AutoMace::Publish(env,selected->id)) { target.reset(); AutoMace::Clear(env); return; }
        target=selected->id;
    }
public:
    AutoMaceModule():RuntimeModule("Auto Mace","Combat",AutoMace_Enabled) {}
    ~AutoMaceModule() override { AutoMace::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoMace() { return std::make_unique<AutoMaceModule>(); }
