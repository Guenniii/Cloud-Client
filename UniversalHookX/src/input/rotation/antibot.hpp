#pragma once
#include "../../modules/settings.hpp"
#include "../../utils/seedcracker/seedcracker_bridge.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include <cstdio>
#include <chrono>

namespace PlayerFilter {
// Construct inside the caller's JNI local frame. No entity or world cache.
class AntiBot {
    JNIEnv* env;
    jclass bridge=nullptr;
    jmethodID method=nullptr;
public:
    explicit AntiBot(JNIEnv* e):env(e) {
        if (!AntiBot_Enabled.load() || !env || env->ExceptionCheck()) return;
        if (SeedCracker::Init(env,"C:\\Users\\okeba\\source\\repos\\UniversalHookX\\UniversalHookX\\bin\\seedcrackerbridge.jar")) {
            bridge=SeedCracker::LoadExtensionClass(env,"phantomui.player.AntiBotBridge");
            if (env->ExceptionCheck()) { env->ExceptionClear(); bridge=nullptr; }
            if (bridge) {
                JniSafety::Lookup api(env);
                auto configure=api.GetStaticMethodID(bridge,"configure","(ZZZI)V");
                if (configure) {
                    env->CallStaticVoidMethod(bridge,configure,static_cast<jboolean>(AntiBot_TabList.load()),
                        static_cast<jboolean>(AntiBot_Profile.load()),static_cast<jboolean>(AntiBot_InvalidData.load()),
                        static_cast<jint>(AntiBot_SpawnGrace.load()));
                    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
                    method=api.GetStaticMethodID(bridge,"shouldIgnore","(Lnet/minecraft/world/entity/player/Player;Z)Z");
                }
            }
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
        const auto ignored=env->CallStaticBooleanMethod(bridge,method,player,JNI_TRUE);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        return ignored==JNI_TRUE;
    }
};
}
