#pragma once
#include "../modules/settings.hpp"
#include "../utils/sdk/java.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include <mutex>
#include <chrono>
#include <cstdio>

namespace AutoTool {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID publish=nullptr,clear=nullptr;
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
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.movement.AutoToolBridge");
        if (!Check(env) || !cls) { std::printf("[AutoTool] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        publish=api.GetStaticMethodID(cls,"publish","(ZZ)V");
        clear=api.GetStaticMethodID(cls,"clear","()V");
        if (!publish || !clear) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return false;
        std::printf("[AutoTool] Automatic tool selection ready.\n");
    }
    env->CallStaticVoidMethod(bridge,publish,static_cast<jboolean>(AutoTool_ReturnSlot.load()),static_cast<jboolean>(AutoTool_ProtectTools.load()));
    return Check(env);
}
inline void Clear(JNIEnv* env,bool shutdown=false) {
    std::lock_guard lock(mutex);
    if (env && bridge) {
        if (!env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,clear); Check(env); }
        if (shutdown) { env->DeleteGlobalRef(bridge); bridge=nullptr; }
    }
}
}
