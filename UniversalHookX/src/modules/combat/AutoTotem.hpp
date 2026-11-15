#pragma once
#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include "../settings.hpp"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cstdio>

class AutoTotem final {
public:
    AutoTotem(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

    void Tick( ) {
        if (!p_mc->IsInitialized( ) || !Ready( ))
            return;

        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK)
            return;

        jobject mc_inst = p_mc->GetInstance( );
        jobject player = env->GetObjectField(mc_inst, p_mc->f_player);
        if (!player)
            return;

        const bool has_offhand = HasOffhandTotem(env, player);
        if (Failed(env)) { env->DeleteLocalRef(player); return; }
        if (!has_offhand) {

            // ===== NEU: Cooldown, falls zuvor kein Totem gefunden wurde =====
            auto now = std::chrono::steady_clock::now( );
            if (m_no_totem_found) {
                auto since_check = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_no_totem_check).count( );
                if (since_check < std::clamp(AutoTotem_RetryInterval.load( ), 100, 5000)) { // nur alle 1000ms erneut prüfen, statt jeden Tick zu spammen
                    env->DeleteLocalRef(player);
                    return;
                }
            }

            // ===== NEU: Vorab prüfen, ob überhaupt ein Totem im Inventar existiert =====
            const bool in_inventory = HasTotemInInventory(env, player);
            if (Failed(env)) { env->DeleteLocalRef(player); return; }
            if (!in_inventory) {
                std::printf("[-] AutoTotem: Kein Totem im Inventar gefunden, warte...\n");
                m_no_totem_found = true;
                m_last_no_totem_check = now;
                env->DeleteLocalRef(player);
                return;
            }

            m_no_totem_found = false; // Totem vorhanden, Cooldown zurücksetzen

            if (!m_is_refilling) {
                m_is_refilling = true;
                m_last_action = now;
            }

            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_action).count( ) > std::clamp(AutoTotem_PopDelay.load( ), 0, 1000)) {
                OpenInventory(env, mc_inst, player);
                std::printf("[*] AutoTotem: Inventar geöffnet, suche Totem...\n");
                Sleep((DWORD)std::clamp(AutoTotem_SwapDelay.load( ), 0, 500));
                if (!Failed(env))
                    MoveTotem(env, mc_inst, player);
                Failed(env);
                Sleep((DWORD)std::clamp(AutoTotem_SwapDelay.load( ), 0, 500));
                CloseInventory(env, mc_inst);
                Failed(env);
                m_is_refilling = false;
            }
        } else {
            m_is_refilling = false;
            m_no_totem_found = false;
        }

        env->DeleteLocalRef(player);
    }
private:
    // IsInitialized() does not cover these IDs; a null ID crashes the JVM.
    bool Ready( ) {
        const bool ok = p_mc->f_player && p_mc->m_get_offhand_item && p_mc->m_stack_is_empty && p_mc->m_stack_get_item
            && p_mc->o_totem && p_mc->o_pickup && p_mc->m_get_inventory && p_mc->m_get_item_from_inv && p_mc->f_gui
            && p_mc->c_inv_screen && p_mc->m_inv_screen_init && p_mc->m_set_screen && p_mc->f_game_mode
            && p_mc->f_inventory_menu && p_mc->f_container_id && p_mc->m_handle_inv_click;
        if (!ok && !m_logged_missing) {
            std::printf("[-] AutoTotem: SDK-IDs fehlen, Modul inaktiv.\n");
            m_logged_missing = true;
        }
        return ok;
    }

    static bool Failed(JNIEnv* env) {
        if (!env->ExceptionCheck( ))
            return false;
        env->ExceptionClear( );
        return true;
    }

    bool HasOffhandTotem(JNIEnv* env, jobject player) {
        jobject stack = env->CallObjectMethod(player, p_mc->m_get_offhand_item);
        if (!stack)
            return false;

        jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
        jobject item = env->ExceptionCheck( ) ? nullptr : env->CallObjectMethod(stack, p_mc->m_stack_get_item);
        if (env->ExceptionCheck( )) {
            env->DeleteLocalRef(stack);
            return false;
        }

        bool result = (!empty && item && env->IsSameObject(item, p_mc->o_totem));

        env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
        return result;
    }

    void MoveTotem(JNIEnv* env, jobject mc_inst, jobject player) {
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return;
        int totem_slot = -1;

        for (int i = 0; i < 36; i++) {
            jobject s = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (env->ExceptionCheck( ))
                break;
            if (!s)
                continue;
            jobject it = env->CallObjectMethod(s, p_mc->m_stack_get_item);
            if (env->ExceptionCheck( )) {
                env->DeleteLocalRef(s);
                break;
            }
            if (it && env->IsSameObject(it, p_mc->o_totem)) {
                totem_slot = i;
                env->DeleteLocalRef(it);
                env->DeleteLocalRef(s);
                break;
            }
            if (it)
                env->DeleteLocalRef(it);
            env->DeleteLocalRef(s);
        }

        if (totem_slot != -1 && !Failed(env)) {
            jobject current = env->GetObjectField(mc_inst, p_mc->f_gui);
            if (!current) {
                jobject screen = env->NewObject(p_mc->c_inv_screen, p_mc->m_inv_screen_init, player);
                jobject guiObj = env->GetObjectField(mc_inst, p_mc->f_gui);
                if (guiObj) {
                    env->CallVoidMethod(guiObj, p_mc->m_set_screen, screen);
                    env->DeleteLocalRef(guiObj);
                }
                env->DeleteLocalRef(screen);
            } else {
                env->DeleteLocalRef(current);
            }

            jobject gm = env->GetObjectField(mc_inst, p_mc->f_game_mode);
            jobject menu = env->GetObjectField(player, p_mc->f_inventory_menu);

            int server_slot = (totem_slot < 9) ? totem_slot + 36 : totem_slot;
            int offhand_slot = 45;

            if (gm && menu) {
                jint cid = env->GetIntField(menu, p_mc->f_container_id);
                env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, server_slot, 0, p_mc->o_pickup, player);
                if (!Failed(env)) env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, offhand_slot, 0, p_mc->o_pickup, player);
                if (!Failed(env)) env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, server_slot, 0, p_mc->o_pickup, player);
                Failed(env);
            }

            std::printf("[SUCCESS] Totem aus Slot %d (Server: %d) verschoben!\n", totem_slot, server_slot);

            env->DeleteLocalRef(gm);
            env->DeleteLocalRef(menu);
        }
        env->DeleteLocalRef(inv);
    }

    void OpenInventory(JNIEnv* env, jobject mc_inst, jobject playerObj) {
        jobject guiObj = env->GetObjectField(mc_inst, p_mc->f_gui);
        jobject invScreenObj = env->NewObject(p_mc->c_inv_screen, p_mc->m_inv_screen_init, playerObj);
        if (Failed(env))
            invScreenObj = nullptr;

        if (p_mc->m_set_screen && guiObj && invScreenObj) {
            env->CallVoidMethod(guiObj, p_mc->m_set_screen, invScreenObj);
        } else {
            std::printf("[-] Failed to open inventory screen: Gui instance or setScreen method missing.\n");
        }

        env->DeleteLocalRef(invScreenObj);
        env->DeleteLocalRef(guiObj);
    }

    void CloseInventory(JNIEnv* env, jobject mc_inst) {
        jobject guiObj = env->GetObjectField(mc_inst, p_mc->f_gui);

        if (p_mc->m_set_screen && guiObj) {
            env->CallVoidMethod(guiObj, p_mc->m_set_screen, nullptr);
        } else {
            std::printf("[-] Failed to close inventory: Gui instance or setScreen method missing.\n");
        }

        env->DeleteLocalRef(guiObj);
    }

    bool HasTotemInInventory(JNIEnv* env, jobject player) {
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv)
            return false;
        bool found = false;

        for (int i = 0; i < 36; i++) {
            jobject s = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (env->ExceptionCheck( ))
                break;
            if (!s)
                continue;

            jboolean empty = env->CallBooleanMethod(s, p_mc->m_stack_is_empty);
            jobject it = (empty || env->ExceptionCheck( )) ? nullptr : env->CallObjectMethod(s, p_mc->m_stack_get_item);
            if (env->ExceptionCheck( )) {
                env->DeleteLocalRef(s);
                break;
            }
            if (!empty) {
                if (it && env->IsSameObject(it, p_mc->o_totem)) {
                    found = true;
                }
                if (it)
                    env->DeleteLocalRef(it);
            }
            env->DeleteLocalRef(s);

            if (found)
                break;
        }

        env->DeleteLocalRef(inv);
        return found;
    }


private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
    bool m_is_refilling = false;
    bool m_no_totem_found = false; // NEU
    bool m_logged_missing = false;
    std::chrono::steady_clock::time_point m_last_action;
    std::chrono::steady_clock::time_point m_last_no_totem_check; // NEU
};
