#include "../RuntimeModule.hpp"
#include "../runtime_modules.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include <cstdio>
#include "../../input/game_screen.hpp"

namespace {
class KeyPearlModule final : public RuntimeModule {
    jclass bridge=nullptr;
    jmethodID pearl=nullptr;
    bool Check(JNIEnv* env) {
        if (!env->ExceptionCheck()) return true;
        env->ExceptionDescribe(); env->ExceptionClear(); return false;
    }
    bool Load(JNIEnv* env) {
        if (bridge) return true;
        JniSafety::LocalFrame frame(env,16);
        if (!frame) { Check(env); return false; }
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.movement.AutoPearlCatchBridge");
        if (!Check(env) || !cls) { std::printf("[KeyPearl] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        pearl=api.GetStaticMethodID(cls,"pearl","(Z)Z");
        if (!pearl) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        return Check(env) && bridge;
    }
    void Tick() override {
        // consume the request first: the screen check below costs JNI lookups and is only needed for an actual throw
        if (!KeyPearl_Enabled) { KeyPearl_Request=false; return; }
        if (!KeyPearl_Request.exchange(false)) return;
        auto env=p_jni?p_jni->GetEnv():nullptr;
        DWORD process=0; GetWindowThreadProcessId(GetForegroundWindow(),&process);
        if (Menu_Enabled || process!=GetCurrentProcessId()
            || !env || Input::GameScreenOpen(env,p_jni->p_cminecraft->GetInstance())) return;
        if (env->ExceptionCheck() || !Load(env)) return;
        const bool queued=env->CallStaticBooleanMethod(bridge,pearl,static_cast<jboolean>(KeyPearl_SpoofHotbar.load()));
        if (Check(env) && !queued) std::printf("[KeyPearl] Throw not queued: a pearl action is still running.\n");
    }
public:
    KeyPearlModule():RuntimeModule("Key Pearl","Movement",KeyPearl_Enabled) {}
    ~KeyPearlModule() override {
        auto env=p_jni?p_jni->GetEnv():nullptr;
        if (env && bridge) env->DeleteGlobalRef(bridge);
    }
};
}
std::unique_ptr<ModuleBase> RuntimeModules::CreateKeyPearl() { return std::make_unique<KeyPearlModule>(); }
