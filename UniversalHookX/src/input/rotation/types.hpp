#pragma once
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
    float yaw = 0.0f;
    float pitch = 0.0f;
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

