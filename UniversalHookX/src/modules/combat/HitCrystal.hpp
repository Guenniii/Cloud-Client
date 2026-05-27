#pragma once
#include "../../dependencies/jni/jni.h"
#include "../../utils/Silentaim.hpp" // <-- Dein Silent Aim Header
#include "../../utils/sdk/CMinecraft.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class HitCrystal final {
public:
    HitCrystal(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

    void Run( ) {
        std::printf("[*] HitCrystal::Run() aufgerufen.\n");

        if (!p_mc->IsInitialized( )) {
            std::printf("[-] HitCrystal::Run() -> CMinecraft nicht initialisiert. Abbruch.\n");
            return;
        }

        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK) {
            std::printf("[-] HitCrystal::Run() -> AttachCurrentThread fehlgeschlagen. Abbruch.\n");
            return;
        }

        jobject mc_inst = p_mc->GetInstance( );
        if (!mc_inst) {
            std::printf("[-] HitCrystal::Run() -> mc_inst ist null. Abbruch.\n");
            return;
        }

        jobject player = env->GetObjectField(mc_inst, p_mc->f_player);
        if (!player) {
            std::printf("[-] HitCrystal::Run() -> player ist null. Abbruch.\n");
            return;
        }

        // 1. Waffe prüfen
        if (!IsHoldingSword(env, player)) {
            std::printf("[-] HitCrystal::Run() -> Kein Schwert in der Hand. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }

        // 2. Obsidian suchen
        int obsidian_slot = FindObsidianInHotbar(env, player);
        if (obsidian_slot == -1) {
            std::printf("[-] HitCrystal: Kein Obsidian in der Hotbar. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }

        // 3. Schwert-Slot für später merken
        int sword_slot = FindSwordInHotbar(env, player);
        if (sword_slot == -1) {
            std::printf("[-] HitCrystal::Run() -> Kein Schwert in Hotbar. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }

        // --- NEU: PHASE 1 - AIM AUF OBSIDIAN & PLATZIEREN ---
        std::printf("[*] HitCrystal: Wechsle zu Obsidian und aime auf den Block...\n");
        SwitchToHotbarSlot(env, player, obsidian_slot);

        // Wir holen uns die Zielkoordinaten für das Obsidian (unter dem zukünftigen Crystal)
        Vector3d obsidianTarget = GetObsidianPlaceTarget(env, mc_inst, player);
        ApplySilentAim(env, mc_inst, player, obsidianTarget);

        Sleep(100);                  // Kurze Verzögerung für die Rotation
        SwingMainHand(env, player); // Platziert Obsidian / Crystal-Basis
        std::printf("[+] HitCrystal: Obsidian-Aim & Placement abgeschlossen.\n");

        // 4. Crystal ausrüsten und platzieren
        Sleep(100);
        SwitchAndPlaceCrystal(env, player);
        Sleep(100);
        // --- NEU: PHASE 2 - EXAKT AUF CENTER VOM CRYSTAL AIMEN ---
        std::printf("[*] HitCrystal: Suche platzierten Crystal für Center-Aim...\n");
        jobject crystalEntity = FindNearestCrystalEntity(env, mc_inst, player);

        if (crystalEntity) {
            // Berechne exakt das CENTER des Crystals (Boden-Y + halbe Höhe)
            Vector3d crystalPos = GetEntityPosition(env, crystalEntity);
            // Ein EndCrystal ist 2.0 Blöcke hoch, die Mitte liegt also bei +1.0 Y
            Vector3d crystalCenter = {crystalPos.x, crystalPos.y + 1.0, crystalPos.z};

            std::printf("[*] HitCrystal: Aime silent auf Crystal-Center (X: %.2f, Y: %.2f, Z: %.2f)\n", crystalCenter.x, crystalCenter.y, crystalCenter.z);
            ApplySilentAim(env, mc_inst, player, crystalCenter);

            Sleep(20); // Warten bis Rotation greift
            SwitchToHotbarSlot(env, player, sword_slot);
            

            // Explodiere direkt das gefundene Entity, statt das träge Client-'hitResult' zu nutzen
            ForceExplodeCrystalEntity(env, mc_inst, player, crystalEntity);
            env->DeleteLocalRef(crystalEntity);
        } else {
            std::printf("[-] HitCrystal: Kein Crystal zum Sprengen gefunden.\n");
            SwitchToHotbarSlot(env, player, sword_slot);
        }

        // Silent Aim zurücksetzen
        SilentAim::reset( );

        Sleep(200);
        env->DeleteLocalRef(player);
        std::printf("[+] HitCrystal::Run() -> Abgeschlossen.\n");
    }
private:
    struct Vector3d {
        double x;
        double y;
        double z;
    };

    // Hilfsfunktion: Berechnet die Winkel und wendet den Silent Aim an
    void ApplySilentAim(JNIEnv* env, jobject mc_inst, jobject player, Vector3d targetPos) {
        Vector3d eyePos = GetEyePosition(env, player);

        double dx = targetPos.x - eyePos.x;
        double dy = targetPos.y - eyePos.y;
        double dz = targetPos.z - eyePos.z;
        double distance2d = std::sqrt(dx * dx + dz * dz);

        float yaw = (float)(std::atan2(dz, dx) * 180.0 / M_PI) - 90.0f;
        float pitch = (float)(-(std::atan2(dy, distance2d) * 180.0 / M_PI));

        SilentAimRequest req;
        req.yaw = yaw;
        req.pitch = pitch;
        req.profile = Profile::COMBAT;
        req.syncVisualHead = true;

        SilentAim::aim(env, mc_inst, req);
        SilentAim::doSilentRotationJNI(env, mc_inst);
    }

    // Hilfsfunktion: Sucht das nächste Crystal-Entity aus der Welt
    jobject FindNearestCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player) {
        jclass mc_class = env->GetObjectClass(mc_inst);
        jfieldID f_level = env->GetFieldID(mc_class, "level", "Lnet/minecraft/client/multiplayer/ClientLevel;");
        env->DeleteLocalRef(mc_class);
        if (!f_level)
            return nullptr;

        jobject level = env->GetObjectField(mc_inst, f_level);
        if (!level)
            return nullptr;

        jclass level_class = env->GetObjectClass(level);
        jmethodID m_get_entities = env->GetMethodID(level_class, "entitiesForRendering", "()Ljava/lang/Iterable;");
        env->DeleteLocalRef(level_class);
        if (!m_get_entities) {
            env->DeleteLocalRef(level);
            return nullptr;
        }

        jobject entities_iterable = env->CallObjectMethod(level, m_get_entities);
        env->DeleteLocalRef(level);
        if (!entities_iterable)
            return nullptr;

        jclass iterable_class = env->FindClass("java/lang/Iterable");
        jmethodID m_iterator = env->GetMethodID(iterable_class, "iterator", "()Ljava/util/Iterator;");
        env->DeleteLocalRef(iterable_class);
        jobject iterator = env->CallObjectMethod(entities_iterable, m_iterator);
        env->DeleteLocalRef(entities_iterable);

        jclass iterator_class = env->FindClass("java/util/Iterator");
        jmethodID m_has_next = env->GetMethodID(iterator_class, "hasNext", "()Z");
        jmethodID m_next = env->GetMethodID(iterator_class, "next", "()Ljava/lang/Object;");
        env->DeleteLocalRef(iterator_class);

        jclass c_end_crystal = env->FindClass("net/minecraft/world/entity/boss/enderdragon/EndCrystal");
        Vector3d eyePos = GetEyePosition(env, player);

        jobject closestCrystal = nullptr;
        double closestDist = 6.0;

        while (env->CallBooleanMethod(iterator, m_has_next)) {
            jobject entity = env->CallObjectMethod(iterator, m_next);
            if (!entity)
                continue;

            if (env->IsInstanceOf(entity, c_end_crystal)) {
                Vector3d crystalPos = GetEntityPosition(env, entity);
                double dx = crystalPos.x - eyePos.x;
                double dy = crystalPos.y - eyePos.y;
                double dz = crystalPos.z - eyePos.z;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

                if (dist < closestDist) {
                    closestDist = dist;
                    if (closestCrystal)
                        env->DeleteLocalRef(closestCrystal);
                    closestCrystal = entity;
                    continue;
                }
            }
            env->DeleteLocalRef(entity);
        }
        env->DeleteLocalRef(iterator);
        env->DeleteLocalRef(c_end_crystal);

        return closestCrystal;
    }

    // Ermittelt die ungefähre Position des Bodens/Obsidians vor dem Spieler
    Vector3d GetObsidianPlaceTarget(JNIEnv* env, jobject mc_inst, jobject player) {
        // Falls ein Crystal in der Nähe existiert, aime unter ihn (auf das Obsidian)
        jobject nearCrystal = FindNearestCrystalEntity(env, mc_inst, player);
        if (nearCrystal) {
            Vector3d cPos = GetEntityPosition(env, nearCrystal);
            env->DeleteLocalRef(nearCrystal);
            return {cPos.x, cPos.y - 0.5, cPos.z}; // Richtet den Aim leicht nach unten aufs Obsidian
        }

        // Ausweichmöglichkeit: Schaut 2 Blöcke vor die Füße des Spielers
        Vector3d playerPos = GetEntityPosition(env, player);
        jclass pClass = env->GetObjectClass(player);
        jfieldID fYaw = env->GetFieldID(pClass, "yRot", "F");
        float yaw = fYaw ? env->GetFloatField(player, fYaw) : 0.0f;
        env->DeleteLocalRef(pClass);

        double radians = yaw * M_PI / 180.0;
        return {playerPos.x - std::sin(radians) * 2.0, playerPos.y, playerPos.z + std::cos(radians) * 2.0};
    }

    // Umgeht den hitResult-Bug: Attackiert das Entity direkt per Paket/GameMode
    void ForceExplodeCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player, jobject crystalEntity) {
        jobject game_mode = env->GetObjectField(mc_inst, p_mc->f_game_mode);
        if (!game_mode)
            return;

        if (p_mc->m_attack) {
            // Führt den serverseitigen Hit direkt auf die Instanz aus
            env->CallVoidMethod(game_mode, p_mc->m_attack, player, crystalEntity);
            std::printf("[+] HitCrystal: Force-Attack auf Crystal-Entity gesendet!\n");
        }
        SwingMainHand(env, player);
        env->DeleteLocalRef(game_mode);
    }

    Vector3d GetEyePosition(JNIEnv* env, jobject entity) {
        Vector3d pos = GetEntityPosition(env, entity);
        jclass entityClass = env->GetObjectClass(entity);
        jmethodID m_get_eye_height = env->GetMethodID(entityClass, "getEyeHeight", "()F");
        if (m_get_eye_height) {
            pos.y += env->CallFloatMethod(entity, m_get_eye_height);
        }
        env->DeleteLocalRef(entityClass);
        return pos;
    }

    Vector3d GetEntityPosition(JNIEnv* env, jobject entity) {
        jclass entityClass = env->GetObjectClass(entity);
        jmethodID m_getX = env->GetMethodID(entityClass, "getX", "()D");
        jmethodID m_getY = env->GetMethodID(entityClass, "getY", "()D");
        jmethodID m_getZ = env->GetMethodID(entityClass, "getZ", "()D");

        Vector3d pos{0.0, 0.0, 0.0};
        if (m_getX && m_getY && m_getZ) {
            pos.x = env->CallDoubleMethod(entity, m_getX);
            pos.y = env->CallDoubleMethod(entity, m_getY);
            pos.z = env->CallDoubleMethod(entity, m_getZ);
        }
        env->DeleteLocalRef(entityClass);
        return pos;
    }

    // --- DEINE UNVERÄNDERTEN HELFERMETHODEN (IsHoldingSword, FindObsidian, etc.) ---
    bool IsHoldingSword(JNIEnv* env, jobject player) {
        if (!p_mc->m_get_main_hand_item)
            return false;
        jobject stack = env->CallObjectMethod(player, p_mc->m_get_main_hand_item);
        if (!stack)
            return false;
        jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
        if (empty) {
            env->DeleteLocalRef(stack);
            return false;
        }
        jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);
        if (!item) {
            env->DeleteLocalRef(stack);
            return false;
        }
        jclass item_class = env->GetObjectClass(item);
        jmethodID get_desc = env->GetMethodID(item_class, "getDescriptionId", "()Ljava/lang/String;");
        if (!get_desc) {
            env->DeleteLocalRef(item_class);
            env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            return false;
        }
        jstring desc = (jstring)env->CallObjectMethod(item, get_desc);
        if (!desc) {
            env->DeleteLocalRef(item_class);
            env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            return false;
        }
        const char* desc_cstr = env->GetStringUTFChars(desc, nullptr);
        bool result = (strstr(desc_cstr, "_sword") != nullptr);
        env->ReleaseStringUTFChars(desc, desc_cstr);
        env->DeleteLocalRef(desc);
        env->DeleteLocalRef(item_class);
        env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
        return result;
    }

    int FindObsidianInHotbar(JNIEnv* env, jobject player) {
        if (!p_mc->m_get_inventory)
            return -1;
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return -1;
        if (!p_mc->o_obsidian) {
            env->DeleteLocalRef(inv);
            return -1;
        }
        int found_slot = -1;
        for (int i = 0; i <= 8; i++) {
            jobject stack = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!stack)
                continue;
            jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
            jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);
            if (!empty && item && env->IsSameObject(item, p_mc->o_obsidian)) {
                found_slot = i;
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
                break;
            }
            if (item)
                env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
        }
        env->DeleteLocalRef(inv);
        return found_slot;
    }

    void SwitchToHotbarSlot(JNIEnv* env, jobject player, int slot_index) {
        if (slot_index < 0 || slot_index > 8)
            return;
        if (!p_mc->m_get_inventory)
            return;
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return;
        jclass inv_class = env->GetObjectClass(inv);
        jfieldID selected_field = env->GetFieldID(inv_class, "selected", "I");
        if (selected_field)
            env->SetIntField(inv, selected_field, slot_index);
        env->DeleteLocalRef(inv_class);
        env->DeleteLocalRef(inv);
    }

    void SwingMainHand(JNIEnv* env, jobject player) {
        if (!p_mc->m_swing_arm || !p_mc->o_main_hand)
            return;
        env->CallVoidMethod(player, p_mc->m_swing_arm, p_mc->o_main_hand);
    }

    void SwitchAndPlaceCrystal(JNIEnv* env, jobject player) {
        if (!p_mc->o_endcrystal)
            return;
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return;
        int crystal_slot = -1;
        for (int i = 0; i <= 8; i++) {
            jobject stack = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!stack)
                continue;
            jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
            jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);
            if (!empty && item && env->IsSameObject(item, p_mc->o_endcrystal)) {
                crystal_slot = i;
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
                break;
            }
            if (item)
                env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
        }
        env->DeleteLocalRef(inv);
        if (crystal_slot == -1)
            return;
        SwitchToHotbarSlot(env, player, crystal_slot);
        Sleep(50);
        SwingMainHand(env, player);
    }

    int FindSwordInHotbar(JNIEnv* env, jobject player) {
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return -1;
        int found_slot = -1;
        for (int i = 0; i <= 8; i++) {
            jobject stack = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!stack)
                continue;
            jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
            jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);
            if (!empty && item) {
                jclass item_class = env->GetObjectClass(item);
                jmethodID get_desc = env->GetMethodID(item_class, "getDescriptionId", "()Ljava/lang/String;");
                jstring desc = get_desc ? (jstring)env->CallObjectMethod(item, get_desc) : nullptr;
                if (desc) {
                    const char* desc_cstr = env->GetStringUTFChars(desc, nullptr);
                    if (strstr(desc_cstr, "_sword")) {
                        found_slot = i;
                    }
                    env->ReleaseStringUTFChars(desc, desc_cstr);
                    env->DeleteLocalRef(desc);
                }
                env->DeleteLocalRef(item_class);
            }
            if (item)
                env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            if (found_slot != -1)
                break;
        }
        env->DeleteLocalRef(inv);
        return found_slot;
    }
private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
};
