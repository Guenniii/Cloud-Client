#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include <cstdio>
#include "../../input/game_screen.hpp"

namespace {
class AutoPearlCatchModule final : public RuntimeModule {
    jclass bridge=nullptr;
    jmethodID fire=nullptr,cancel=nullptr,advance=nullptr;
    bool Check(JNIEnv* env) {
        if (!env->ExceptionCheck()) return true;
        env->ExceptionDescribe(); env->ExceptionClear(); return false;
    }
    void Cancel(JNIEnv* env) {
        if (env && bridge && !env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,cancel); Check(env); }
    }
    bool Load(JNIEnv* env) {
        if (bridge) return true;
        JniSafety::LocalFrame frame(env,16);
        if (!frame) { Check(env); return false; }
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.movement.AutoPearlCatchBridge");
        if (!Check(env) || !cls) { std::printf("[AutoPearlCatch] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        fire=api.GetStaticMethodID(cls,"fire","(IZ)Z");
        cancel=api.GetStaticMethodID(cls,"cancel","()V");
        advance=api.GetStaticMethodID(cls,"advance","()V");
        if (!fire || !cancel || !advance) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        return Check(env) && bridge;
    }
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!AutoPearlCatch_Enabled || Menu_Enabled || process!=GetCurrentProcessId()
            || !env || Input::GameScreenOpen(env,p_jni->p_cminecraft->GetInstance())) {
            AutoPearlCatch_Request=false; Cancel(env); return;
        }
        if (bridge && env && !env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,advance); Check(env); }
        if (!AutoPearlCatch_Request.exchange(false) || !env || env->ExceptionCheck() || !Load(env)) return;
        const bool queued=env->CallStaticBooleanMethod(bridge,fire,static_cast<jint>(AutoPearlCatch_Delay.load()),static_cast<jboolean>(AutoPearlCatch_SpoofHotbar.load()));
        if (Check(env) && !queued) std::printf("[AutoPearlCatch] Previous action still pending.\n");
    }
public:
    AutoPearlCatchModule():RuntimeModule("Auto Pearl Catch","Movement",AutoPearlCatch_Enabled) {}
    ~AutoPearlCatchModule() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        Cancel(env); if (env && bridge) env->DeleteGlobalRef(bridge);
    }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateAutoPearlCatch() { return std::make_unique<AutoPearlCatchModule>(); }
