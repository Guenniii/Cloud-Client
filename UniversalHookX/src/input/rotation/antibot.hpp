#pragma once
#include "../../modules/settings.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include <cstdio>
#include <chrono>
#include <mutex>
#include <atomic>

namespace PlayerFilter {
// Bridge class and method IDs are resolved once; the global ref is released by Shutdown.
inline std::mutex antiBotMutex;
inline std::atomic<jclass> antiBotClass{nullptr};
inline jmethodID antiBotConfigure=nullptr,antiBotIgnore=nullptr;
inline std::chrono::steady_clock::time_point antiBotRetry{};
inline jclass AntiBotClass(JNIEnv* env) {
    if (auto cached=antiBotClass.load()) return cached;
    if (!env) return nullptr;
    std::lock_guard lock(antiBotMutex);
    if (auto cached=antiBotClass.load()) return cached;
    const auto now=std::chrono::steady_clock::now();
    if (now<antiBotRetry) return nullptr;
    antiBotRetry=now+std::chrono::seconds(5);
    if (!SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) return nullptr;
    jclass local=SeedCracker::LoadExtensionClass(env,"phantomui.player.AntiBotBridge");
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    if (!local) return nullptr;
    JniSafety::Lookup api(env);
    const auto configure=api.GetStaticMethodID(local,"configure","(ZZZI)V");
    const auto ignore=api.GetStaticMethodID(local,"shouldIgnore","(Lnet/minecraft/world/entity/player/Player;Z)Z");
    jclass global=nullptr;
    if (configure && ignore) {
        global=static_cast<jclass>(env->NewGlobalRef(local));
        if (env->ExceptionCheck()) { env->ExceptionClear(); global=nullptr; }
    }
    env->DeleteLocalRef(local);
    if (!global) return nullptr;
    antiBotConfigure=configure; antiBotIgnore=ignore;
    antiBotClass.store(global);
    return global;
}
inline void Shutdown(JNIEnv* env) {
    std::lock_guard lock(antiBotMutex);
    auto c=antiBotClass.exchange(nullptr);
    antiBotConfigure=antiBotIgnore=nullptr;
    if (env && c) env->DeleteGlobalRef(c);
}
// Construct inside the caller's JNI local frame. No entity or world cache.
class AntiBot {
    JNIEnv* env;
    jclass bridge=nullptr;
    jmethodID method=nullptr;
public:
    explicit AntiBot(JNIEnv* e):env(e) {
        if (!env || env->ExceptionCheck()) return;
        bridge=AntiBotClass(env);
        if (bridge) {
            env->CallStaticVoidMethod(bridge,antiBotConfigure,static_cast<jboolean>(AntiBot_TabList.load()),
                static_cast<jboolean>(AntiBot_Profile.load()),static_cast<jboolean>(AntiBot_InvalidData.load()),
                static_cast<jint>(AntiBot_SpawnGrace.load()));
            if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
            method=antiBotIgnore;
        }
        if (!method) {
            static std::atomic<long long> last{0};
            const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            auto previous=last.load();
            if (now-previous>5000 && last.compare_exchange_strong(previous,now)) {
                std::printf("[AntiBot] Filter unavailable; update bridge JAR and restart Minecraft. Players remain visible/selectable.\n");
            }
        }
    }
    bool Ignore(jobject player) const {
        if (!method || !player || env->ExceptionCheck()) return false;
        const auto ignored=env->CallStaticBooleanMethod(bridge,method,player,static_cast<jboolean>(AntiBot_Enabled.load()));
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        return ignored==JNI_TRUE;
    }
};
}
