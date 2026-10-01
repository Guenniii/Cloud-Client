#pragma once
#include "../dependencies/jni/jni.h"
#include "../input/rotation/types.hpp"
namespace SilentAim {
enum class Owner { None, HitCrystal, Standalone };
// One owner per module update. HitCrystal is dispatched first by ModuleManager.
void beginUpdate();
bool apply(JNIEnv* env, jobject minecraft, const SilentAimRequest& request, Owner owner, float* appliedYaw=nullptr, float* appliedPitch=nullptr);
void reset(Owner owner);
}
