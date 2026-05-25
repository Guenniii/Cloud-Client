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
            if (!m_is_refilling) {
                m_is_refilling = true;
                m_last_action = std::chrono::steady_clock::now( );
            }

            auto now = std::chrono::steady_clock::now( );
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




private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
    bool m_is_refilling = false;
    std::chrono::steady_clock::time_point m_last_action;
};
