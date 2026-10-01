#pragma once
#include "../../modules/settings.hpp"
#include "../../utils/sdk/java.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include "../rotation/antibot.hpp"
#include <mutex>
#include <chrono>
#include <cstdio>

namespace BreachSwap {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID publish=nullptr,clear=nullptr,attack=nullptr;
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
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.combat.BreachSwapBridge");
        if (!Check(env) || !cls) { std::printf("[BreachSwap] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        publish=api.GetStaticMethodID(cls,"publish","(ZZ)V");
        clear=api.GetStaticMethodID(cls,"clear","()V");
        attack=api.GetStaticMethodID(cls,"attackClick","()Z");
        if (!publish || !clear || !attack) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return false;
        std::printf("[BreachSwap] Sword-to-Breach swap ready.\n");
    }
    { JniSafety::LocalFrame frame(env,16); if (frame) { PlayerFilter::AntiBot filter(env); } }
    env->CallStaticVoidMethod(bridge,publish,static_cast<jboolean>(AntiBot_Enabled.load()),static_cast<jboolean>(BreachSwap_SpoofHotbar.load()));
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
    if (!BreachSwap_Enabled || !bridge || !p_jni || !p_jni->GetJVM()) return false;
    JNIEnv* env=nullptr;
    if (p_jni->GetJVM()->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)!=JNI_OK || !env || env->ExceptionCheck()) return false;
    const bool consumed=env->CallStaticBooleanMethod(bridge,attack);
    return Check(env) && consumed;
}
}
