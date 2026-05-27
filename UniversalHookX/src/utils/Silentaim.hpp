#pragma once
#include <algorithm>
#include <cmath>
#include "../dependencies/jni/jni.h"
#include <random>

enum class Profile {
    COMBAT,
    PLACE,
    PRECISE
};

struct ProfileSettings {
    float stiffness;
    float damping;
    float yawCapDeg;
    float pitchCapDeg;
    float minSpeedDeg;
    float tremorAmpDeg;
    int reactionTicksMin;
    int reactionTicksMax;
};

struct SilentAimRequest {
    float yaw;
    float pitch;
    Profile profile = Profile::COMBAT;
    int priority = 0;
    float maxYawStepDeg = 0.0f;
    float maxPitchStepDeg = 0.0f;
    float stiffness = 0.0f;
    float damping = 0.0f;
    bool syncVisualHead = true;
    bool fixMovement = true;
    bool disableTremor = false;
    bool disableReaction = false;
};

namespace SilentAim {
    void aim(JNIEnv* env, jobject minecraftInstance, SilentAimRequest& req);
    void doSilentRotationJNI(JNIEnv* env, jobject minecraftInstance);
    void reset( );

    // Globale Zustände aus dem Java-Code
    extern float serverYaw, serverPitch;
    extern bool isActive;
} // namespace SilentAim
