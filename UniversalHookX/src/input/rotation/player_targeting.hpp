#pragma once
#include "player_selection.hpp"
#include "antibot.hpp"
#include "../../utils/sdk/jni_safety.hpp"

namespace Rotation {
// Resolved once on the SDK worker thread (not thread-safe); retried next tick while incomplete.
struct TargetIds {
    jfieldID player=nullptr,level=nullptr;
    jmethodID x=nullptr,y=nullptr,z=nullptr,yaw=nullptr,pitch=nullptr,alive=nullptr,spectator=nullptr,visible=nullptr,id=nullptr,players=nullptr,toArray=nullptr;
    bool ready=false;
};
inline TargetIds targetIds;
// All references live in local frames. The only retained identity is an entity ID.
inline std::optional<PlayerTarget> FindPlayerTarget(JNIEnv* env, jobject mc, double range,
                                                    double fov, std::optional<int> previous, std::string* diagnosis=nullptr) {
    if(diagnosis)*diagnosis="JNI context unavailable";
    if (!env || !mc || env->ExceptionCheck()) return {};
    JniSafety::LocalFrame frame(env,32);
    if (!frame) { env->ExceptionClear(); return {}; }
    auto failed=[&] { if (!env->ExceptionCheck()) return false; env->ExceptionClear(); return true; };
    JniSafety::Lookup api(env);
    if (!targetIds.ready) {
        TargetIds t;
        const auto mcClass=api.GetObjectClass(mc);
        t.player=api.GetFieldID(mcClass,"player","Lnet/minecraft/client/player/LocalPlayer;");
        t.level=api.GetFieldID(mcClass,"level","Lnet/minecraft/client/multiplayer/ClientLevel;");
        const auto playerClass=api.FindClass("net/minecraft/world/entity/player/Player");
        t.x=api.GetMethodID(playerClass,"getX","()D");
        t.y=api.GetMethodID(playerClass,"getEyeY","()D");
        t.z=api.GetMethodID(playerClass,"getZ","()D");
        t.yaw=api.GetMethodID(playerClass,"getYRot","()F");
        t.pitch=api.GetMethodID(playerClass,"getXRot","()F");
        t.alive=api.GetMethodID(playerClass,"isAlive","()Z");
        t.spectator=api.GetMethodID(playerClass,"isSpectator","()Z");
        t.visible=api.GetMethodID(playerClass,"hasLineOfSight","(Lnet/minecraft/world/entity/Entity;)Z");
        t.id=api.GetMethodID(playerClass,"getId","()I");
        t.players=api.GetMethodID(api.FindClass("net/minecraft/client/multiplayer/ClientLevel"),"players","()Ljava/util/List;");
        t.toArray=api.GetMethodID(api.FindClass("java/util/List"),"toArray","()[Ljava/lang/Object;");
        t.ready=t.player && t.level && t.x && t.y && t.z && t.yaw && t.pitch && t.alive && t.spectator && t.visible && t.id && t.players && t.toArray;
        if (!t.ready) {
            if(diagnosis)*diagnosis="Missing player method: "+std::string(!t.player?"player field":!t.level?"level field":!t.x?"getX":!t.y?"getEyeY":!t.z?"getZ":!t.yaw?"getYRot":!t.pitch?"getXRot":!t.visible?"hasLineOfSight":!t.alive?"isAlive":!t.spectator?"isSpectator":!t.id?"getId":!t.players?"players":"toArray");
            return {};
        }
        targetIds=t;
    }
    const auto& ids=targetIds;
    const auto player=api.GetObjectField(mc,ids.player);
    const auto level=api.GetObjectField(mc,ids.level);
    if(diagnosis)*diagnosis="No active player/world";
    if (!player || !level) return {};
    const auto x=ids.x,y=ids.y,z=ids.z,yaw=ids.yaw,pitch=ids.pitch,alive=ids.alive,spectator=ids.spectator,visible=ids.visible;
    const bool living=env->CallBooleanMethod(player,alive); if (failed() || !living) return {};
    const bool watching=env->CallBooleanMethod(player,spectator); if (failed() || watching) return {};
    Point eye{}; Angles camera{};
    eye.x=env->CallDoubleMethod(player,x); if (failed()) return {};
    eye.y=env->CallDoubleMethod(player,y); if (failed()) return {};
    eye.z=env->CallDoubleMethod(player,z); if (failed()) return {};
    camera.yaw=env->CallFloatMethod(player,yaw); if (failed()) return {};
    camera.pitch=env->CallFloatMethod(player,pitch); if (failed()) return {};
    const auto list=api.CallObjectMethod(level,ids.players);
    const auto array=static_cast<jobjectArray>(api.CallObjectMethod(list,ids.toArray));
    if(diagnosis)*diagnosis="World player list lookup failed";
    if (!array) return {};
    const auto count=api.GetArrayLength(array);
    if (count>1024) return {}; // Bound work on the existing SDK worker.
    PlayerFilter::AntiBot antiBot(env);
    std::optional<PlayerTarget> best;
    int filtered=0,outside=0,occluded=0;
    for (jsize i=0;i<count;++i) {
        JniSafety::LocalFrame itemFrame(env,8);
        if (!itemFrame) { env->ExceptionClear(); return {}; }
        const auto entity=api.GetObjectArrayElement(array,i);
        if (!entity || env->IsSameObject(player,entity)) continue;
        if (antiBot.Ignore(entity)) {++filtered;continue;}
        const auto isAlive=ids.alive,isSpectator=ids.spectator,getId=ids.id,getX=ids.x,getY=ids.y,getZ=ids.z;
        const bool valid=env->CallBooleanMethod(entity,isAlive); if (failed() || !valid) continue;
        const bool spec=env->CallBooleanMethod(entity,isSpectator); if (failed() || spec) continue;
        const int id=env->CallIntMethod(entity,getId); if (failed()) continue;
        Point target{};
        target.x=env->CallDoubleMethod(entity,getX); if (failed()) continue;
        target.y=env->CallDoubleMethod(entity,getY); if (failed()) continue;
        target.z=env->CallDoubleMethod(entity,getZ); if (failed()) continue;
        const auto candidate=ScorePlayer(id,eye,target,camera,range,fov);
        if (!candidate) {++outside;continue;}
        const bool seen=env->CallBooleanMethod(player,visible,entity); if (failed() || !seen) {++occluded;continue;}
        ConsiderPlayer(best,*candidate,previous);
    }
    if(diagnosis)*diagnosis=best?"Target acquired":"No eligible target: players="+std::to_string(count)+", filtered="+std::to_string(filtered)+", outside range/FOV="+std::to_string(outside)+", occluded="+std::to_string(occluded);
    return best;
}
}
