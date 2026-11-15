#include "../../utils/sdk/game_runtime.hpp"
#include "HitCrystal.hpp"
#include "../../utils/sdk/CMinecraft.h"
#include "../../utils/lifecycle/lifecycle.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include "../../utils/SilentAim.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../input/rotation/targeting.hpp"
#include "../../input/game_screen.hpp"
#include "../settings.hpp"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HitCrystal::HitCrystal(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

void HitCrystal::Start( ) {
    if (Lifecycle::requested || m_started.exchange(true))
        return;
    Lifecycle::Spawn([this] { Loop( ); });
}

bool HitCrystal::Allowed(JNIEnv* env, jobject mc) {
    if (!HitCrystal_Enabled || Lifecycle::requested || Menu_Enabled || !mc)
        return false;
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow( ), &process);
    return process == GetCurrentProcessId( ) && !Input::GameScreenOpen(env, mc);
}

bool HitCrystal::Wait(JNIEnv* env, jobject mc, DWORD ms) {
    Sleep(ms);
    return Allowed(env, mc);
}

void HitCrystal::Loop( ) {
    JNIEnv* env = nullptr;
    if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env)
        return;
    JniSafety::ScopeExit cleanup{[this, env] {
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
        SilentAim::reset(SilentAim::Owner::HitCrystal);
        p_jvm->DetachCurrentThread( );
    }};
    while (!Lifecycle::requested) {
        Sleep(5);
        JniSafety::LocalFrame frame(env, 64);
        if (!frame)
            break;
        if (!p_mc->IsInitialized( ))
            continue;
        jobject mc = p_mc->GetInstance( );
        if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) && Allowed(env, mc))
            Run(env, mc);
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
    }
}

void HitCrystal::Run(JNIEnv* env, jobject mc_inst) {
    JniSafety::ScopeExit resetRotation{[] { SilentAim::reset(SilentAim::Owner::HitCrystal); }};
    auto softLog = [this](const char* msg) {
        const ULONGLONG now = GetTickCount64( );
        if (now - m_lastLog < 500)
            return;
        m_lastLog = now;
        std::printf("[-] HitCrystal: %s\n", msg);
    };
    auto failed = [env] {
        if (!env->ExceptionCheck( ))
            return false;
        env->ExceptionClear( );
        return true;
    };

    jobject player = env->GetObjectField(mc_inst, p_mc->f_player);
    if (failed( ) || !player) {
        softLog("player ist null. Abbruch.");
        return;
    }

    // 1. Waffe pruefen
    if (!IsHoldingSword(env, player)) {
        softLog("Kein Schwert in der Hand. Abbruch.");
        return;
    }

    // 2. Obsidian suchen
    int obsidian_slot = FindObsidianInHotbar(env, player);
    if (obsidian_slot == -1) {
        softLog("Kein Obsidian in der Hotbar. Abbruch.");
        return;
    }

    // 3. Schwert-Slot fuer spaeter merken
    int sword_slot = FindSwordInHotbar(env, player);
    if (sword_slot == -1) {
        softLog("Kein Schwert in Hotbar. Abbruch.");
        return;
    }
    if (failed( ))
        return;
    // Bei Abbruch zurueck aufs Schwert; nach dem geplanten Wechsel nicht erneut (User darf danach frei wechseln).
    bool restoreSword = true;
    JniSafety::ScopeExit backToSword{[this, env, player, sword_slot, &restoreSword] {
        if (!restoreSword)
            return;
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
        SwitchToHotbarSlot(env, player, sword_slot);
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
    }};

    // PHASE 1 - AIM AUF OBSIDIAN & PLATZIEREN
    std::printf("[*] HitCrystal: Wechsle zu Obsidian und aime auf den Block...\n");
    SwitchToHotbarSlot(env, player, obsidian_slot);
    if (failed( ))
        return;

    Vector3d obsidianTarget = GetObsidianPlaceTarget(env, mc_inst, player);
    if (failed( ) || !ApplySilentAim(env, mc_inst, player, obsidianTarget))
        return;

    const DWORD obsDelay = (DWORD)std::clamp(HitCrystal_ObsidianDelay.load( ), 25, 250);
    const DWORD crysDelay = (DWORD)std::clamp(HitCrystal_CrystalDelay.load( ), 40, 300);
    if (!Wait(env, mc_inst, obsDelay))
        return;
    SwingMainHand(env, player); // Platziert Obsidian / Crystal-Basis
    if (failed( ))
        return;
    std::printf("[+] HitCrystal: Obsidian-Aim & Placement abgeschlossen.\n");

    // 4. Crystal ausruesten und platzieren
    if (!Wait(env, mc_inst, obsDelay))
        return;
    SwitchAndPlaceCrystal(env, player);
    if (failed( ) || !Wait(env, mc_inst, crysDelay))
        return;

    // PHASE 2 - EXAKT AUF CENTER VOM CRYSTAL AIMEN
    std::printf("[*] HitCrystal: Suche platzierten Crystal fuer Center-Aim...\n");
    jobject crystalEntity = FindNearestCrystalEntity(env, mc_inst, player);
    if (failed( ))
        return;

    if (crystalEntity) {
        Vector3d crystalPos = GetEntityPosition(env, crystalEntity);
        // Ein EndCrystal ist 2.0 Bloecke hoch, die Mitte liegt also bei +1.0 Y
        Vector3d crystalCenter = {crystalPos.x, crystalPos.y + 1.0, crystalPos.z};

        std::printf("[*] HitCrystal: Aime silent auf Crystal-Center (X: %.2f, Y: %.2f, Z: %.2f)\n", crystalCenter.x, crystalCenter.y, crystalCenter.z);
        if (!ApplySilentAim(env, mc_inst, player, crystalCenter))
            return;

        Sleep(20); // Warten bis Rotation greift
        SwitchToHotbarSlot(env, player, sword_slot);
        if (failed( ))
            return;
        restoreSword = false;

        // Explodiere direkt das gefundene Entity, statt das traege Client-'hitResult' zu nutzen
        ForceExplodeCrystalEntity(env, mc_inst, player, crystalEntity);
        if (failed( ))
            return;
    } else {
        std::printf("[-] HitCrystal: Kein Crystal zum Sprengen gefunden.\n");
        SwitchToHotbarSlot(env, player, sword_slot);
        if (failed( ))
            return;
        restoreSword = false;
    }

    SilentAim::reset(SilentAim::Owner::HitCrystal);

    if (!Wait(env, mc_inst, (DWORD)std::clamp(HitCrystal_Cooldown.load( ), 50, 1000)))
        return;
    std::printf("[+] HitCrystal::Run() -> Abgeschlossen.\n");
}

bool HitCrystal::ApplySilentAim(JNIEnv* env, jobject mc_inst, jobject player, Vector3d targetPos) {
    Vector3d eyePos = GetEyePosition(env, player);

    const auto angles = Rotation::LookAt({eyePos.x, eyePos.y, eyePos.z},
                                         {targetPos.x, targetPos.y, targetPos.z});
    if (!angles)
        return false;
    SilentAimRequest req;
    req.yaw = angles->yaw;
    req.pitch = angles->pitch;
    req.profile = Profile::COMBAT;
    req.syncVisualHead = true;

    // Standalone kann den Claim kurz halten (Update alle 5 ms).
    for (int i = 0; i < 5; i++) {
        if (SilentAim::apply(env, mc_inst, req, SilentAim::Owner::HitCrystal))
            return true;
        Sleep(5);
    }
    const ULONGLONG now = GetTickCount64( );
    if (now - m_lastLog >= 500) {
        m_lastLog = now;
        std::printf("[-] HitCrystal: SilentAim-Claim fehlgeschlagen. Abbruch.\n");
    }
    return false;
}

jobject HitCrystal::FindNearestCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player) {
    JniSafety::Lookup api(env);
    jclass mc_class = api.GetObjectClass(mc_inst);
    jfieldID f_level = api.GetFieldID(mc_class, "level", "Lnet/minecraft/client/multiplayer/ClientLevel;");
    if (mc_class)
        env->DeleteLocalRef(mc_class);
    jobject level = api.GetObjectField(mc_inst, f_level);
    if (!level)
        return nullptr;

    jclass level_class = api.GetObjectClass(level);
    jmethodID m_get_entities = api.GetMethodID(level_class, "entitiesForRendering", "()Ljava/lang/Iterable;");
    if (level_class)
        env->DeleteLocalRef(level_class);
    jobject entities_iterable = api.CallObjectMethod(level, m_get_entities);
    env->DeleteLocalRef(level);
    if (!entities_iterable)
        return nullptr;

    jclass iterable_class = api.FindClass("java/lang/Iterable");
    jmethodID m_iterator = api.GetMethodID(iterable_class, "iterator", "()Ljava/util/Iterator;");
    if (iterable_class)
        env->DeleteLocalRef(iterable_class);
    jobject iterator = api.CallObjectMethod(entities_iterable, m_iterator);
    env->DeleteLocalRef(entities_iterable);

    jclass iterator_class = api.FindClass("java/util/Iterator");
    jmethodID m_has_next = api.GetMethodID(iterator_class, "hasNext", "()Z");
    jmethodID m_next = api.GetMethodID(iterator_class, "next", "()Ljava/lang/Object;");
    if (iterator_class)
        env->DeleteLocalRef(iterator_class);

    jclass c_end_crystal = api.FindClass("net/minecraft/world/entity/boss/enderdragon/EndCrystal");
    if (!iterator || !m_has_next || !m_next || !c_end_crystal) {
        if (iterator)
            env->DeleteLocalRef(iterator);
        if (c_end_crystal)
            env->DeleteLocalRef(c_end_crystal);
        return nullptr;
    }
    Vector3d eyePos = GetEyePosition(env, player);

    jobject closestCrystal = nullptr;
    double closestDist = 6.0;

    while (true) {
        const jboolean more = env->CallBooleanMethod(iterator, m_has_next);
        if (env->ExceptionCheck( )) {
            env->ExceptionClear( );
            break;
        }
        if (!more)
            break;
        jobject entity = api.CallObjectMethod(iterator, m_next);
        if (!entity)
            break; // Exception (z.B. ConcurrentModification) oder Ende: nicht weiter iterieren

        if (env->IsInstanceOf(entity, c_end_crystal)) {
            Vector3d crystalPos = GetEntityPosition(env, entity);
            if (Rotation::Nearer({eyePos.x, eyePos.y, eyePos.z},
                                 {crystalPos.x, crystalPos.y, crystalPos.z}, closestDist)) {
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

HitCrystal::Vector3d HitCrystal::GetObsidianPlaceTarget(JNIEnv* env, jobject mc_inst, jobject player) {
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

void HitCrystal::ForceExplodeCrystalEntity(JNIEnv* env, jobject mc_inst, jobject player, jobject crystalEntity) {
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

HitCrystal::Vector3d HitCrystal::GetEyePosition(JNIEnv* env, jobject entity) {
        Vector3d pos = GetEntityPosition(env, entity);
        jclass entityClass = env->GetObjectClass(entity);
        jmethodID m_get_eye_height = env->GetMethodID(entityClass, "getEyeHeight", "()F");
        if (m_get_eye_height) {
            pos.y += env->CallFloatMethod(entity, m_get_eye_height);
        }
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
        env->DeleteLocalRef(entityClass);
        return pos;
    }

HitCrystal::Vector3d HitCrystal::GetEntityPosition(JNIEnv* env, jobject entity) {
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
        if (env->ExceptionCheck( ))
            env->ExceptionClear( );
        env->DeleteLocalRef(entityClass);
        return pos;
    }

bool HitCrystal::IsHoldingSword(JNIEnv* env, jobject player) {
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

int HitCrystal::FindObsidianInHotbar(JNIEnv* env, jobject player) {
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

void HitCrystal::SwitchToHotbarSlot(JNIEnv* env, jobject player, int slot_index) {
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

void HitCrystal::SwingMainHand(JNIEnv* env, jobject player) {
        p_mc->Swing(env,player);
    }

void HitCrystal::SwitchAndPlaceCrystal(JNIEnv* env, jobject player) {
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
        Sleep((DWORD)std::clamp(HitCrystal_CrystalDelay.load( ), 40, 300) / 2);
        SwingMainHand(env, player);
    }

int HitCrystal::FindSwordInHotbar(JNIEnv* env, jobject player) {
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
