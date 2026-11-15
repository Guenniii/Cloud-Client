#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../../input/combat/aim_assist.hpp"
#include "../../input/rotation/player_targeting.hpp"
#include "../../input/game_screen.hpp"
namespace {
class AimAssistModule final : public RuntimeModule {
    std::optional<int> target;
    std::string lastStatus;
    ULONGLONG nextLog=0;
    void Status(const std::string& message){
        const auto now=GetTickCount64();if(message==lastStatus||now<nextLog)return;
        lastStatus=message;nextLog=now+2000;
        std::printf("[AimAssist] %s\n",message.c_str());
    }
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        auto mc=p_jni && p_jni->p_cminecraft?p_jni->p_cminecraft->GetInstance():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        const char* paused=!AimAssist_Enabled?"Disabled":Silent_Aim_Enabled?"Paused: Silent Aim enabled":
            Menu_Enabled?"Paused: client menu open":process!=GetCurrentProcessId()?"Paused: game not focused":
            !env||!mc?"Paused: JNI/Minecraft unavailable":Input::GameScreenOpen(env,mc)?"Paused: game screen open":nullptr;
        if(paused){
            if(AimAssist_Enabled)Status(paused);else lastStatus.clear();
            target.reset();AimAssist::Clear(env);return;
        }
        std::string diagnosis;
        const auto selected=Rotation::FindPlayerTarget(env,mc,std::clamp(AimAssist_Range.load(),1,6),
            std::clamp(AimAssist_Fov.load(),10,180),target,&diagnosis);
        if (!selected) { Status(diagnosis); target.reset(); AimAssist::Clear(env); return; }
        if (!AimAssist::Publish(env,selected->id)) { Status("Bridge publish failed"); target.reset(); AimAssist::Clear(env); return; }
        Status("Target published: "+std::to_string(selected->id));
        target=selected->id;
    }
public:
    AimAssistModule():RuntimeModule("Aim Assist","Combat",AimAssist_Enabled) {}
    ~AimAssistModule() override { AimAssist::Clear(p_jni?p_jni->GetEnv():nullptr,true); }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAimAssist() { return std::make_unique<AimAssistModule>(); }
