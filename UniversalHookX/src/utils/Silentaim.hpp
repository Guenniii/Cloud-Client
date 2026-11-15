#pragma once
#include "../dependencies/jni/jni.h"
#include "../input/rotation/types.hpp"
namespace SilentAim {
enum class Owner { None, HitCrystal, Standalone, StandaloneHit }; // StandaloneHit: Standalone's sendPosition hit, distinct from the HitCrystal thread
// One owner per module update. HitCrystal runs on its own thread and may fail to claim while Standalone holds the update.
void beginUpdate();
bool apply(JNIEnv* env, jobject minecraft, const SilentAimRequest& request, Owner owner, float* appliedYaw=nullptr, float* appliedPitch=nullptr);
void reset(Owner owner);
}
