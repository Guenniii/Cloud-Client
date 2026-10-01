#pragma once
#include "player_selection.hpp"
#include "antibot.hpp"
#include "../../utils/sdk/jni_safety.hpp"

namespace Rotation {
// All references live in local frames. The only retained identity is an entity ID.
inline std::optional<PlayerTarget> FindPlayerTarget(JNIEnv* env, jobject mc, double range,
                                                    double fov, std::optional<int> previous) {
    if (!env || !mc || env->ExceptionCheck()) return {};
    JniSafety::LocalFrame frame(env,32);
    if (!frame) { env->ExceptionClear(); return {}; }
    auto failed=[&] { if (!env->ExceptionCheck()) return false; env->ExceptionClear(); return true; };
    JniSafety::Lookup api(env);
    const auto mcClass=api.GetObjectClass(mc);
    const auto player=api.GetObjectField(mc,api.GetFieldID(mcClass,"player","Lnet/minecraft/client/player/LocalPlayer;"));
    const auto level=api.GetObjectField(mc,api.GetFieldID(mcClass,"level","Lnet/minecraft/client/multiplayer/ClientLevel;"));
    if (!player || !level) return {};
    const auto playerClass=api.GetObjectClass(player);
    const auto x=api.GetMethodID(playerClass,"getX","()D");
    const auto y=api.GetMethodID(playerClass,"getEyeY","()D");
    const auto z=api.GetMethodID(playerClass,"getZ","()D");
    const auto yaw=api.GetMethodID(playerClass,"getYRot","()F");
    const auto pitch=api.GetMethodID(playerClass,"getXRot","()F");
    const auto alive=api.GetMethodID(playerClass,"isAlive","()Z");
    const auto spectator=api.GetMethodID(playerClass,"isSpectator","()Z");
    const auto visible=api.GetMethodID(playerClass,"hasLineOfSight","(Lnet/minecraft/world/entity/Entity;)Z");
    if (!x || !y || !z || !yaw || !pitch || !visible || !alive || !spectator) return {};
    const bool living=env->CallBooleanMethod(player,alive); if (failed() || !living) return {};
    const bool watching=env->CallBooleanMethod(player,spectator); if (failed() || watching) return {};
    Point eye{}; Angles camera{};
    eye.x=env->CallDoubleMethod(player,x); if (failed()) return {};
    eye.y=env->CallDoubleMethod(player,y); if (failed()) return {};
    eye.z=env->CallDoubleMethod(player,z); if (failed()) return {};
    camera.yaw=env->CallFloatMethod(player,yaw); if (failed()) return {};
    camera.pitch=env->CallFloatMethod(player,pitch); if (failed()) return {};
    const auto levelClass=api.GetObjectClass(level);
    const auto list=api.CallObjectMethod(level,api.GetMethodID(levelClass,"players","()Ljava/util/List;"));
    const auto listClass=api.GetObjectClass(list);
    const auto array=static_cast<jobjectArray>(api.CallObjectMethod(list,api.GetMethodID(listClass,"toArray","()[Ljava/lang/Object;")));
    if (!array) return {};
    const auto count=api.GetArrayLength(array);
    if (count>1024) return {}; // Bound work on the existing SDK worker.
    PlayerFilter::AntiBot antiBot(env);
    std::optional<PlayerTarget> best;
    for (jsize i=0;i<count;++i) {
        JniSafety::LocalFrame itemFrame(env,8);
        if (!itemFrame) { env->ExceptionClear(); return {}; }
        const auto entity=api.GetObjectArrayElement(array,i);
        if (!entity || env->IsSameObject(player,entity)) continue;
        if (antiBot.Ignore(entity)) continue;
        const auto cls=api.GetObjectClass(entity);
        const auto isAlive=api.GetMethodID(cls,"isAlive","()Z");
        const auto isSpectator=api.GetMethodID(cls,"isSpectator","()Z");
        const auto getId=api.GetMethodID(cls,"getId","()I");
        const auto getX=api.GetMethodID(cls,"getX","()D");
        const auto getY=api.GetMethodID(cls,"getEyeY","()D");
        const auto getZ=api.GetMethodID(cls,"getZ","()D");
        if (!isAlive || !isSpectator || !getId || !getX || !getY || !getZ) continue;
        const bool valid=env->CallBooleanMethod(entity,isAlive); if (failed() || !valid) continue;
        const bool spec=env->CallBooleanMethod(entity,isSpectator); if (failed() || spec) continue;
        const int id=env->CallIntMethod(entity,getId); if (failed()) continue;
        Point target{};
        target.x=env->CallDoubleMethod(entity,getX); if (failed()) continue;
        target.y=env->CallDoubleMethod(entity,getY); if (failed()) continue;
        target.z=env->CallDoubleMethod(entity,getZ); if (failed()) continue;
        const auto candidate=ScorePlayer(id,eye,target,camera,range,fov);
        if (!candidate) continue;
        const bool seen=env->CallBooleanMethod(player,visible,entity); if (failed() || !seen) continue;
        ConsiderPlayer(best,*candidate,previous);
    }
    return best;
}
}
