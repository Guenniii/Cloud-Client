#pragma once
#include "../../dependencies/jni/jni.h"

class CMinecraft;

class HitCrystal final {
public:
    HitCrystal(JavaVM* jvm, CMinecraft* mc);

    void Run( );
private:
    struct Vector3d {
        double x;
        double y;
        double z;
    };

    // Hilfsfunktion: Berechnet die Winkel und wendet den Silent Aim an
    void ApplySilentAim(JNIEnv* env, jobject mc_inst, jobject player, Vector3d targetPos);

    // Hilfsfunktion: Sucht das nächste Crystal-Entity aus der Welt
    jobject FindNearestCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player);

    // Ermittelt die ungefähre Position des Bodens/Obsidians vor dem Spieler
    Vector3d GetObsidianPlaceTarget(JNIEnv* env, jobject mc_inst, jobject player);

    // Umgeht den hitResult-Bug: Attackiert das Entity direkt per Paket/GameMode
    void ForceExplodeCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player, jobject crystalEntity);

    Vector3d GetEyePosition(JNIEnv* env, jobject entity);

    Vector3d GetEntityPosition(JNIEnv* env, jobject entity);

    // --- DEINE UNVERÄNDERTEN HELFERMETHODEN (IsHoldingSword, FindObsidian, etc.) ---
    bool IsHoldingSword(JNIEnv* env, jobject player);

    int FindObsidianInHotbar(JNIEnv* env, jobject player);

    void SwitchToHotbarSlot(JNIEnv* env, jobject player, int slot_index);

    void SwingMainHand(JNIEnv* env, jobject player);

    void SwitchAndPlaceCrystal(JNIEnv* env, jobject player);

    int FindSwordInHotbar(JNIEnv* env, jobject player);
private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
};
