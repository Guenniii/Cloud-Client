#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include <cstdio>

namespace {
class ShortWindChargeModule final : public RuntimeModule {
    jclass bridge=nullptr;
    jmethodID fire=nullptr,cancel=nullptr;
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
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.movement.ShortWindChargeBridge");
        if (!Check(env) || !cls) { std::printf("[ShortWindCharge] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        fire=api.GetStaticMethodID(cls,"fire","(ZZI)Z");
        cancel=api.GetStaticMethodID(cls,"cancel","()V");
        if (!fire || !cancel) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        return Check(env) && bridge;
    }
    void Tick() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (!ShortWindCharge_Enabled || Menu_Enabled || process!=GetCurrentProcessId()) {
            ShortWindCharge_Request=false; Cancel(env); return;
        }
        if (!ShortWindCharge_Request.exchange(false) || !env || env->ExceptionCheck() || !Load(env)) return;
        const bool queued=env->CallStaticBooleanMethod(bridge,fire,static_cast<jboolean>(ShortWindCharge_SpoofHotbar.load()),
            static_cast<jboolean>(ShortWindCharge_Jump.load()), static_cast<jint>(ShortWindCharge_ThrowDelay.load()));
        if (Check(env) && !queued) std::printf("[ShortWindCharge] Previous action still pending.\n");
    }
public:
    ShortWindChargeModule():RuntimeModule("Short Wind Charge","Movement",ShortWindCharge_Enabled) {}
    ~ShortWindChargeModule() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        Cancel(env); if (env && bridge) env->DeleteGlobalRef(bridge);
    }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateShortWindCharge() { return std::make_unique<ShortWindChargeModule>(); }
