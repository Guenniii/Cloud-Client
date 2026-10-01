#include "SilentAim.hpp"
#include "sdk/jni_safety.hpp"
#include "../input/rotation/controller.hpp"
#include "../input/rotation/ownership.hpp"
#include <mutex>

namespace SilentAim {
namespace {
std::mutex mutex;
Rotation::Controller controller;
Rotation::Ownership<Owner> ownership;
bool failed(JNIEnv* env) {
    if (!env->ExceptionCheck()) return false;
    env->ExceptionClear();
    return true;
}
void release(Owner owner) {
    if (ownership.Release(owner)) controller.Reset();
}
}

void beginUpdate() {
    std::lock_guard lock(mutex);
    ownership.Begin();
}

void reset(Owner owner) {
    std::lock_guard lock(mutex);
    release(owner);
}

bool apply(JNIEnv* env, jobject minecraft, const SilentAimRequest& request, Owner owner, float* appliedYaw, float* appliedPitch) {
    std::lock_guard lock(mutex);
    if (!env || !minecraft || env->ExceptionCheck()) {
        release(owner);
        return false;
    }
    if (!std::isfinite(request.yaw) || !std::isfinite(request.pitch)
        || !std::isfinite(request.stiffness) || !std::isfinite(request.damping)
        || !std::isfinite(request.maxYawStepDeg) || !std::isfinite(request.maxPitchStepDeg)) return false;

    JniSafety::LocalFrame frame(env, 16);
    if (!frame) { failed(env); release(owner); return false; }
    JniSafety::Lookup lookup(env);
    const auto mcClass = lookup.GetObjectClass(minecraft);
    const auto playerField = lookup.GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
    const auto player = lookup.GetObjectField(minecraft, playerField);
    const auto playerClass = lookup.GetObjectClass(player);
    const auto yawField = lookup.GetFieldID(playerClass, "yRot", "F");
    const auto pitchField = lookup.GetFieldID(playerClass, "xRot", "F");
    const auto sendPosition = lookup.GetMethodID(playerClass, "sendPosition", "()V");
    if (!player || !yawField || !pitchField || !sendPosition) { release(owner); return false; }
    const float visualYaw = env->GetFloatField(player, yawField);
    if (failed(env)) { release(owner); return false; }
    const float visualPitch = env->GetFloatField(player, pitchField);
    if (failed(env) || !std::isfinite(visualYaw) || !std::isfinite(visualPitch)) { release(owner); return false; }
    if (!ownership.Claim(owner)) return false;
    if (!controller.isActive || ownership.active != owner) controller.Seed(visualYaw, visualPitch);
    ownership.active = owner;
    if (!controller.Update(request)) { release(owner); return false; }

    if (appliedYaw) *appliedYaw=controller.serverYaw;
    if (appliedPitch) *appliedPitch=controller.serverPitch;
    // Standalone attacks are dispatched by Fabric on the client thread.
    if (owner==Owner::Standalone) return true;

    if (request.syncVisualHead) {
        const auto headField = lookup.GetFieldID(playerClass, "yHeadRot", "F");
        if (headField) {
            env->SetFloatField(player, headField, controller.serverYaw);
            if (failed(env)) { release(owner); return false; }
        }
    }
    // Always restore the camera angles, including a Java exception during sendPosition.
    bool ok = true;
    env->SetFloatField(player, yawField, controller.serverYaw);
    ok = !failed(env);
    if (ok) {
        env->SetFloatField(player, pitchField, controller.serverPitch);
        ok = !failed(env);
    }
    if (ok) {
        env->CallVoidMethod(player, sendPosition);
        ok = !failed(env);
    }
    env->SetFloatField(player, yawField, visualYaw);
    const bool restoredYaw = !failed(env);
    env->SetFloatField(player, pitchField, visualPitch);
    const bool restoredPitch = !failed(env);
    ok = ok && restoredYaw && restoredPitch;
    if (!ok) release(owner);
    return ok;
}
}
