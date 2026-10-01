#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include <chrono>
#include <cstdio>

namespace {
class SafeWalkModule final : public RuntimeModule {
    jclass bridge=nullptr;
    jmethodID update=nullptr, clear=nullptr;
    std::chrono::steady_clock::time_point next{}, retry{};
    bool sentEnabled=false;
    bool Check(JNIEnv* env) {
        if (!env->ExceptionCheck()) return true;
        env->ExceptionDescribe(); env->ExceptionClear(); return false;
    }
    bool Load(JNIEnv* env) {
        if (bridge) return true;
        const auto now=std::chrono::steady_clock::now();
        if (now<retry) return false;
        retry=now+std::chrono::seconds(5);
        JniSafety::LocalFrame frame(env,16);
        if (!frame) { Check(env); return false; }
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.movement.SafeWalkBridge");
        if (!Check(env) || !cls) { std::printf("[SafeWalk] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        update=api.GetStaticMethodID(cls,"update","(ZZ)V");
        clear=api.GetStaticMethodID(cls,"clear","()V");
        if (!update || !clear) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        return Check(env) && bridge;
    }
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        if (!env || env->ExceptionCheck()) return;
        DWORD process=0;
        GetWindowThreadProcessId(GetForegroundWindow(),&process);
        const bool enabled=SafeWalk_Enabled && !Menu_Enabled && process==GetCurrentProcessId();
        if (!enabled && !bridge) return;
        const auto now=std::chrono::steady_clock::now();
        if (enabled==sentEnabled && now<next) return;
        next=now+std::chrono::milliseconds(50);
        if (!Load(env)) return;
        env->CallStaticVoidMethod(bridge,update,static_cast<jboolean>(enabled),static_cast<jboolean>(SafeWalk_OnlyBlocks.load()));
        if (Check(env)) sentEnabled=enabled;
    }
public:
    SafeWalkModule():RuntimeModule("Safe Walk","Movement",SafeWalk_Enabled) {}
    ~SafeWalkModule() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        if (env && bridge) {
            if (!env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,clear); Check(env); }
            env->DeleteGlobalRef(bridge);
        }
    }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateSafeWalk() { return std::make_unique<SafeWalkModule>(); }
