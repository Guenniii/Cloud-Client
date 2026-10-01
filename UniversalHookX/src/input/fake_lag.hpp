#pragma once
#include "../modules/settings.hpp"
#include "../utils/sdk/java.hpp"
#include "../utils/sdk/jni_safety.hpp"
#include "../utils/seedcracker/seedcracker_bridge.hpp"
#include <mutex>
#include <chrono>
#include <cstdio>

namespace FakeLag {
inline std::mutex mutex;
inline jclass bridge=nullptr;
inline jmethodID publish=nullptr,clear=nullptr,status=nullptr;
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
        auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.network.FakeLagBridge");
        if (!Check(env) || !cls) { std::printf("[FakeLag] Update bridge JAR and restart Minecraft.\n"); return false; }
        JniSafety::Lookup api(env);
        publish=api.GetStaticMethodID(cls,"publish","(IZ)V");
        status=api.GetStaticMethodID(cls,"status","()Ljava/lang/String;");
        clear=api.GetStaticMethodID(cls,"clear","()V");
        if (!publish || !clear || !status) return false;
        bridge=static_cast<jclass>(env->NewGlobalRef(cls));
        if (!Check(env) || !bridge) return false;
        std::printf("[FakeLag] Bounded movement packet delay ready.\n");
    }
    env->CallStaticVoidMethod(bridge,publish,static_cast<jint>(FakeLag_Delay.load()),static_cast<jboolean>(FakeLag_Burst.load()));
    if (!Check(env)) return false;
    static auto nextStatus=std::chrono::steady_clock::time_point{};
    if (FakeLag_Debug && std::chrono::steady_clock::now()>=nextStatus) {
        nextStatus=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        JniSafety::LocalFrame frame(env,8);
        if (frame) {
            auto text=static_cast<jstring>(env->CallStaticObjectMethod(bridge,status));
            if (Check(env) && text) {
                auto utf=env->GetStringUTFChars(text,nullptr);
                if (utf) { std::printf("[FakeLag] %s\n",utf);env->ReleaseStringUTFChars(text,utf); }
                Check(env);
            }
        } else Check(env);
    }
    return true;
}
inline void Clear(JNIEnv* env,bool shutdown=false) {
    std::lock_guard lock(mutex);
    if (env && bridge) {
        if (!env->ExceptionCheck()) { env->CallStaticVoidMethod(bridge,clear); Check(env); }
        if (shutdown) { env->DeleteGlobalRef(bridge); bridge=nullptr; }
    }
}
}
