#pragma once
#include "../../modules/settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include <mutex>
#include <chrono>
#include <cstdio>

namespace SafeAnchor {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID publish=nullptr,clear=nullptr,attack=nullptr,drain=nullptr;
inline auto retry=std::chrono::steady_clock::time_point{};
inline bool Check(JNIEnv* env) {
    if (!env->ExceptionCheck()) return true;
    env->ExceptionDescribe(); env->ExceptionClear(); return false;
}
inline bool Publish(JNIEnv* env) {
    std::lock_guard lock(mutex);
    if (!env || env->ExceptionCheck()) return false;
    if (!bridge) {
        auto now=std::chrono::steady_clock::now();
        if (now<retry) return false;
        retry=now+std::chrono::seconds(5);
        JniSafety::LocalFrame frame(env,16);
        if (!frame) { Check(env); return false; }
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.combat.SafeAnchorBridge");
        if (!Check(env) || !cls) { std::printf("[SafeAnchor] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        publish=api.GetStaticMethodID(cls,"publish","(ZZZZZZIIII)V");
        clear=api.GetStaticMethodID(cls,"clear","()V");
        attack=api.GetStaticMethodID(cls,"useClick","()Z");
        drain=api.GetStaticMethodID(cls,"drainLogs","()Ljava/lang/String;");
        if (!publish || !clear || !attack || !drain) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return false;
        if (SafeAnchor_Logs) std::printf("[SafeAnchor] Anchor protection ready.\n");
    }
    env->CallStaticVoidMethod(bridge,publish,static_cast<jboolean>(SafeAnchor_Automatic.load()),static_cast<jboolean>(SafeAnchor_SpoofHotbar.load()),
        static_cast<jboolean>(SafeAnchor_AutoCharge.load()),static_cast<jboolean>(SafeAnchor_Shield.load()),
        static_cast<jboolean>(SafeAnchor_Detonate.load()),static_cast<jboolean>(SafeAnchor_Logs.load()),
        SafeAnchor_PlaceDelay.load(),SafeAnchor_ChargeDelay.load(),SafeAnchor_ShieldDelay.load(),SafeAnchor_DetonateDelay.load());
    if (!Check(env)) return false;
    auto logs=static_cast<jstring>(env->CallStaticObjectMethod(bridge,drain));
    if (Check(env) && logs) {
        const char* text=env->GetStringUTFChars(logs,nullptr);
        if (text) { if (SafeAnchor_Logs) std::printf("%s",text); env->ReleaseStringUTFChars(logs,text); }
        env->DeleteLocalRef(logs);
    }
    return Check(env);
}
inline void Clear(JNIEnv* env,bool shutdown=false) {
    std::lock_guard lock(mutex);
    if (env && bridge) {
        if (!env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,clear); Check(env); }
        if (shutdown) { env->DeleteGlobalRef(bridge); bridge=nullptr; }
    }
}
inline bool TryClick() {
    std::lock_guard lock(mutex);
    if (!SafeAnchor_Enabled || !bridge || !p_jni || !p_jni->GetJVM()) return false;
    JNIEnv* env=nullptr;
    if (p_jni->GetJVM()->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK || !env || env->ExceptionCheck()) return false;
    const bool consumed=env->CallStaticBooleanMethod(bridge,attack);
    return Check(env) && consumed;
}
}
