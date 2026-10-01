#pragma once
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include <cstdio>
#include <chrono>

namespace ManualAttack {
// Accessed only by the SDK worker, including unload after its loop stops.
inline jclass bridge=nullptr;
inline jmethodID publishMethod=nullptr, clearMethod=nullptr;
inline std::chrono::steady_clock::time_point retry{};
inline bool Check(JNIEnv* env) {
    if (!env->ExceptionCheck()) return true;
    env->ExceptionDescribe(); env->ExceptionClear(); return false;
}
inline void Clear(JNIEnv* env) {
    if (env && bridge && clearMethod && !env->ExceptionCheck()) {
        env->CallStaticVoidMethod(bridge,clearMethod); Check(env);
    }
}
inline bool Publish(JNIEnv* env,int id,float yaw,float pitch) {
    if (!env || env->ExceptionCheck()) return false;
    if (!bridge) {
        const auto now=std::chrono::steady_clock::now();
        if (now<retry) return false;
        retry=now+std::chrono::seconds(5);
        JniSafety::LocalFrame frame(env,16);
        if (!frame) { Check(env); return false; }
        if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return false;
        const auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.aim.ManualAttackBridge");
        if (!Check(env) || !cls) { std::printf("[SilentAim] Attack bridge missing; update JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        publishMethod=api.GetStaticMethodID(cls,"publish","(IFF)V");
        clearMethod=api.GetStaticMethodID(cls,"clear","()V");
        if (!publishMethod || !clearMethod) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return false;
    }
    env->CallStaticVoidMethod(bridge,publishMethod,id,yaw,pitch);
    return Check(env);
}
inline void Shutdown(JNIEnv* env) {
    Clear(env);
    if (env && bridge) env->DeleteGlobalRef(bridge);
    bridge=nullptr; publishMethod=clearMethod=nullptr;
}
}
