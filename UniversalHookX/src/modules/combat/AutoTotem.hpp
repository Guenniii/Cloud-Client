#pragma once
#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <Windows.h>
#include <chrono>
#include <cstdio>

class AutoTotem final {
public:
    AutoTotem(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

    void Tick( ) {
        if (!p_mc->IsInitialized( ))
            return;

        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK)
            return;

        jobject mc_inst = p_mc->GetInstance( );
        jobject player = env->GetObjectField(mc_inst, p_mc->f_player);
        if (!player)
            return;

        if (!HasOffhandTotem(env, player)) {

            // ===== NEU: Cooldown, falls zuvor kein Totem gefunden wurde =====
            auto now = std::chrono::steady_clock::now( );
            if (m_no_totem_found) {
                auto since_check = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_no_totem_check).count( );
                if (since_check < 1000) { // nur alle 1000ms erneut prüfen, statt jeden Tick zu spammen
                    env->DeleteLocalRef(player);
                    return;
                }
            }

            // ===== NEU: Vorab prüfen, ob überhaupt ein Totem im Inventar existiert =====
            if (!HasTotemInInventory(env, player)) {
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

            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_action).count( ) > 200) {
                OpenInventory(env, mc_inst, player);
                std::printf("[*] AutoTotem: Inventar geöffnet, suche Totem...\n");
                Sleep(100);
                MoveTotem(env, mc_inst, player);
                Sleep(100);
                CloseInventory(env, mc_inst);
                m_is_refilling = false;
            }
        } else {
            m_is_refilling = false;
            m_no_totem_found = false;
        }

        env->DeleteLocalRef(player);
    }
private:
    bool HasOffhandTotem(JNIEnv* env, jobject player) {
        jobject stack = env->CallObjectMethod(player, p_mc->m_get_offhand_item);
        if (!stack)
            return false;

        jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
        jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);

        bool result = (!empty && item && env->IsSameObject(item, p_mc->o_totem));

        env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);
        return result;
    }

    void MoveTotem(JNIEnv* env, jobject mc_inst, jobject player) {
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        int totem_slot = -1;

        for (int i = 0; i < 36; i++) {
            jobject s = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!s)
                continue;
            jobject it = env->CallObjectMethod(s, p_mc->m_stack_get_item);
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

        if (totem_slot != -1) {
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
            jint cid = env->GetIntField(menu, p_mc->f_container_id);

            int server_slot = (totem_slot < 9) ? totem_slot + 36 : totem_slot;
            int offhand_slot = 45;

            env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, server_slot, 0, p_mc->o_pickup, player);
            env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, offhand_slot, 0, p_mc->o_pickup, player);
            env->CallVoidMethod(gm, p_mc->m_handle_inv_click, cid, server_slot, 0, p_mc->o_pickup, player);

            std::printf("[SUCCESS] Totem aus Slot %d (Server: %d) verschoben!\n", totem_slot, server_slot);

            env->DeleteLocalRef(gm);
            env->DeleteLocalRef(menu);
        }
        env->DeleteLocalRef(inv);
    }

    void OpenInventory(JNIEnv* env, jobject mc_inst, jobject playerObj) {
        jobject guiObj = env->GetObjectField(mc_inst, p_mc->f_gui);
        jobject invScreenObj = env->NewObject(p_mc->c_inv_screen, p_mc->m_inv_screen_init, playerObj);

        if (p_mc->m_set_screen && guiObj) {
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
        bool found = false;

        for (int i = 0; i < 36; i++) {
            jobject s = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!s)
                continue;

            jboolean empty = env->CallBooleanMethod(s, p_mc->m_stack_is_empty);
            if (!empty) {
                jobject it = env->CallObjectMethod(s, p_mc->m_stack_get_item);
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
    std::chrono::steady_clock::time_point m_last_action;
    std::chrono::steady_clock::time_point m_last_no_totem_check; // NEU
};
