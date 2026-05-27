#define NOMINMAX // ← Verhindert, dass Windows.h std::min und std::max zerschießt
#include "Silentaim.hpp"
#include <windows.h>

namespace SilentAim {
    // Interne Zustände
    float serverYaw = 0.0f, serverPitch = 0.0f;
    float prevServerYaw = 0.0f, prevServerPitch = 0.0f;
    float yawVel = 0.0f, pitchVel = 0.0f;

    float lastTargetYaw = std::nanf("");
    float lastTargetPitch = std::nanf("");
    int reactionLeft = 0;
    bool isActive = false;

    double tremorYawPhaseA = 0.0, tremorYawPhaseB = 0.0;
    double tremorPitchPhaseA = 0.0, tremorPitchPhaseB = 0.0;

    // Profile-Strukturen aus dem Java Code kopiert
    ProfileSettings combatProfile{0.55f, 0.78f, 36.0f, 22.0f, 1.6f, 0.10f, 1, 3};
    ProfileSettings placeProfile{0.26f, 0.86f, 14.0f, 10.0f, 0.8f, 0.06f, 0, 1};
    ProfileSettings preciseProfile{0.34f, 0.88f, 18.0f, 14.0f, 0.9f, 0.05f, 0, 2};

    // Helfer für Zufallswerte
    float randomFloat(float min, float max) {
        static std::random_device rd;
        static std::mt19937 gen(rd( ));
        std::uniform_real_distribution<float> dis(min, max);
        return dis(gen);
    }

    double randomDouble(double min, double max) {
        static std::random_device rd;
        static std::mt19937 gen(rd( ));
        std::uniform_real_distribution<double> dis(min, max);
        return dis(gen);
    }

    // Minecraft mathematischer Helfer: Winkel im Bereich -180 bis 180 halten
    float wrapAngleTo180(float value) {
        value = std::fmod(value + 180.0f, 360.0f);
        if (value < 0.0f)
            value += 360.0f;
        return value - 180.0f;
    }

    // GCD Berechnung (Simulierter Mouse-GCD für Anticheat-Bypass)
    float getMouseGCD( ) {
        return 0.0012f;
    }

    float snapStepToGcd(float rawStep, float remaining) {
        double gcd = (double)getMouseGCD( );
        if (gcd <= 0.0)
            return rawStep;

        long long n = std::llround((double)rawStep / gcd);
        double snapped = (double)n * gcd;

        if ((snapped > 0.0) == (remaining > 0.0) && std::abs(snapped) > std::abs(remaining)) {
            long long k = (long long)std::floor(std::abs(remaining) / gcd);
            snapped = std::copysign((double)k * gcd, (double)remaining);
        }
        return (float)snapped;
    }

    float stepAxis(float err, float capDeg, float minSpd, float stiffness, float damping, bool isYaw) {
        float vel = isYaw ? yawVel : pitchVel;
        float distScaled = (float)std::min(1.0, std::log(1.0 + std::abs(err) / 6.0) / std::log(1.0 + 60.0 / 6.0));
        float effectiveCap = std::max(minSpd, capDeg * (0.20f + 0.80f * distScaled));

        float accel = stiffness * err - damping * vel;
        vel += accel;

        if (vel > effectiveCap)
            vel = effectiveCap;
        if (vel < -effectiveCap)
            vel = -effectiveCap;

        float noise = (randomFloat(-1.0f, 1.0f)) * effectiveCap * 0.02f;
        vel += noise;

        if (isYaw)
            yawVel = vel;
        else
            pitchVel = vel;
        return vel;
    }

    void seedFromPlayer(JNIEnv* env, jobject localPlayer, jfieldID fidYaw, jfieldID fidPitch) {
        serverYaw = env->GetFloatField(localPlayer, fidYaw);
        serverPitch = env->GetFloatField(localPlayer, fidPitch);
        prevServerYaw = serverYaw;
        prevServerPitch = serverPitch;
        yawVel = pitchVel = 0.0f;
    }

    void aim(JNIEnv* env, jobject minecraftInstance, SilentAimRequest& req) {
        isActive = true;

        jclass mcClass = env->GetObjectClass(minecraftInstance);
        jfieldID playerField = env->GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        jobject localPlayer = env->GetObjectField(minecraftInstance, playerField);
        if (!localPlayer)
            return;

        jclass playerClass = env->GetObjectClass(localPlayer);
        jfieldID fidYaw = env->GetFieldID(playerClass, "yRot", "F");
        jfieldID fidPitch = env->GetFieldID(playerClass, "xRot", "F");

        static bool wasActive = false;
        if (!wasActive) {
            seedFromPlayer(env, localPlayer, fidYaw, fidPitch);
            wasActive = true;
        }

        ProfileSettings p = combatProfile;
        if (req.profile == Profile::PLACE)
            p = placeProfile;
        if (req.profile == Profile::PRECISE)
            p = preciseProfile;

        float targetYaw = req.yaw;
        float targetPitch = std::clamp(req.pitch, -89.5f, 89.5f);

        if (!req.disableReaction && !std::isnan(lastTargetYaw)) {
            float jumpYaw = std::abs(wrapAngleTo180(targetYaw - lastTargetYaw));
            float jumpPit = std::abs(targetPitch - lastTargetPitch);
            float jump = std::max(jumpYaw, jumpPit * 1.5f);
            if (jump > 30.0f && reactionLeft <= 0) {
                reactionLeft = p.reactionTicksMin + (rand( ) % (std::max(1, p.reactionTicksMax - p.reactionTicksMin + 1)));
            }
        }
        lastTargetYaw = targetYaw;
        lastTargetPitch = targetPitch;

        if (reactionLeft > 0) {
            reactionLeft--;
            yawVel *= 0.5f;
            pitchVel *= 0.5f;
            return;
        }

        float effStiffness = req.stiffness > 0.0f ? req.stiffness : p.stiffness;
        float effDamping = req.damping > 0.0f ? req.damping : p.damping;

        float yawErr = wrapAngleTo180(targetYaw - serverYaw);
        float yawCap = req.maxYawStepDeg > 0.0f ? req.maxYawStepDeg : p.yawCapDeg;
        float yawStep = stepAxis(yawErr, yawCap, p.minSpeedDeg, effStiffness, effDamping, true);

        float pitErr = targetPitch - serverPitch;
        float pitCap = req.maxPitchStepDeg > 0.0f ? req.maxPitchStepDeg : p.pitchCapDeg;
        float pitStep = stepAxis(pitErr, pitCap, p.minSpeedDeg, effStiffness, effDamping, false);

        float worst = std::max(std::abs(yawErr), std::abs(pitErr));
        float closeness = 0.0f;
        if (!req.disableTremor && worst < 6.0f) {
            if (worst <= 0.6f)
                closeness = 1.0f;
            else {
                float t = (6.0f - worst) / (6.0f - 0.6f);
                closeness = t * t * (3.0f - 2.0f * t);
            }
        }

        if (closeness > 0.0f) {
            tremorYawPhaseA += 0.18 + randomDouble(0.0, 0.04);
            tremorYawPhaseB += 0.061 + randomDouble(0.0, 0.012);
            tremorPitchPhaseA += 0.155 + randomDouble(0.0, 0.035);
            tremorPitchPhaseB += 0.047 + randomDouble(0.0, 0.010);

            float yawTremor = (float)(std::sin(tremorYawPhaseA) * 0.62 + std::sin(tremorYawPhaseB) * 0.38) * p.tremorAmpDeg * closeness;
            float pitTremor = (float)(std::sin(tremorPitchPhaseA) * 0.55 + std::sin(tremorPitchPhaseB) * 0.45) * p.tremorAmpDeg * 0.7f * closeness;

            yawStep += yawTremor;
            pitStep += pitTremor;
        }

        yawStep = snapStepToGcd(yawStep, yawErr);
        pitStep = snapStepToGcd(pitStep, pitErr);

        prevServerYaw = serverYaw;
        prevServerPitch = serverPitch;

        serverYaw += yawStep;
        serverPitch = std::clamp(serverPitch + pitStep, -89.5f, 89.5f);

        if (req.syncVisualHead) {
            jfieldID fidHead = env->GetFieldID(playerClass, "yHeadRot", "F");
            if (fidHead) {
                env->SetFloatField(localPlayer, fidHead, serverYaw);
            }
        }

        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mcClass);
    }

    void doSilentRotationJNI(JNIEnv* env, jobject minecraftInstance) {
        if (!isActive)
            return;

        jclass mcClass = env->GetObjectClass(minecraftInstance);
        jfieldID playerField = env->GetFieldID(mcClass, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        jobject localPlayer = env->GetObjectField(minecraftInstance, playerField);
        if (!localPlayer)
            return;

        jclass playerClass = env->GetObjectClass(localPlayer);
        jfieldID fidYaw = env->GetFieldID(playerClass, "yRot", "F");
        jfieldID fidPitch = env->GetFieldID(playerClass, "xRot", "F");
        jmethodID sendPositionMethod = env->GetMethodID(playerClass, "sendPosition", "()V");

        if (fidYaw && fidPitch && sendPositionMethod) {
            float clientYaw = env->GetFloatField(localPlayer, fidYaw);
            float clientPitch = env->GetFloatField(localPlayer, fidPitch);

            env->SetFloatField(localPlayer, fidYaw, serverYaw);
            env->SetFloatField(localPlayer, fidPitch, serverPitch);

            env->CallVoidMethod(localPlayer, sendPositionMethod);

            env->SetFloatField(localPlayer, fidYaw, clientYaw);
            env->SetFloatField(localPlayer, fidPitch, clientPitch);
        }

        env->DeleteLocalRef(playerClass);
        env->DeleteLocalRef(localPlayer);
        env->DeleteLocalRef(mcClass);
    }

    void reset( ) {
        isActive = false;
        lastTargetYaw = std::nanf("");
        lastTargetPitch = std::nanf("");
        reactionLeft = 0;
    }
} // namespace SilentAim
