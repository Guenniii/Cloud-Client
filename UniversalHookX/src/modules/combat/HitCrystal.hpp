#pragma once
#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"
#include <Windows.h>
#include <cstdio>

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

        std::printf("[*] HitCrystal::Run() -> Pruefe Schwert in Hand...\n");
        if (!IsHoldingSword(env, player)) {
            std::printf("[-] HitCrystal::Run() -> Kein Schwert in der Hand. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }
        std::printf("[+] HitCrystal::Run() -> Schwert erkannt.\n");

        std::printf("[*] HitCrystal::Run() -> Suche Obsidian in Hotbar...\n");
        int obsidian_slot = FindObsidianInHotbar(env, player);
        if (obsidian_slot == -1) {
            std::printf("[-] HitCrystal: Kein Obsidian in der Hotbar. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }
        std::printf("[+] HitCrystal: Obsidian gefunden in Slot %d.\n", obsidian_slot);

        std::printf("[*] HitCrystal: Wechsle zu Slot %d...\n", obsidian_slot);
        SwitchToHotbarSlot(env, player, obsidian_slot);
        SwingMainHand(env, player);
        std::printf("[+] HitCrystal: SwingMainHand abgeschlossen.\n");

        // Statt manueller Slot-Speicherung:
        int sword_slot = FindSwordInHotbar(env, player);
        if (sword_slot == -1) {
            std::printf("[-] HitCrystal::Run() -> Kein Schwert in Hotbar. Abbruch.\n");
            env->DeleteLocalRef(player);
            return;
        }
        std::printf("[*] HitCrystal::Run() -> Schwert in Slot %d gefunden.\n", sword_slot);

        // 5. Kurz warten, dann Crystal platzieren
        Sleep(100);
        SwitchAndPlaceCrystal(env, player);
        std::printf("[+] HitCrystal: SwitchAndPlaceCrystal abgeschlossen.\n");
        Sleep(100);
        SwitchToHotbarSlot(env, player, sword_slot);
        Sleep(100);
        ExplodeCrystal(env, player);
        Sleep(1000);
        env->DeleteLocalRef(player);
        std::printf("[+] HitCrystal::Run() -> Abgeschlossen ohne Fehler.\n");
    }
private:
    bool IsHoldingSword(JNIEnv* env, jobject player) {
        std::printf("[*] IsHoldingSword() -> Hole MainHandItem...\n");

        if (!p_mc->m_get_main_hand_item) {
            std::printf("[-] IsHoldingSword() -> m_get_main_hand_item ist null!\n");
            return false;
        }

        jobject stack = env->CallObjectMethod(player, p_mc->m_get_main_hand_item);
        if (!stack) {
            std::printf("[-] IsHoldingSword() -> stack ist null.\n");
            return false;
        }

        jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
        if (empty) {
            std::printf("[-] IsHoldingSword() -> Hand ist leer.\n");
            env->DeleteLocalRef(stack);
            return false;
        }

        jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);
        if (!item) {
            std::printf("[-] IsHoldingSword() -> item ist null.\n");
            env->DeleteLocalRef(stack);
            return false;
        }

        // Ueber getDescriptionId pruefen: enthaelt "_sword"
        jclass item_class = env->GetObjectClass(item);
        jmethodID get_desc = env->GetMethodID(item_class, "getDescriptionId", "()Ljava/lang/String;");
        if (!get_desc) {
            std::printf("[-] IsHoldingSword() -> getDescriptionId nicht gefunden!\n");
            env->DeleteLocalRef(item_class);
            env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            return false;
        }

        jstring desc = (jstring)env->CallObjectMethod(item, get_desc);
        if (!desc) {
            std::printf("[-] IsHoldingSword() -> desc ist null.\n");
            env->DeleteLocalRef(item_class);
            env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            return false;
        }

        const char* desc_cstr = env->GetStringUTFChars(desc, nullptr);
        std::printf("[*] IsHoldingSword() -> Item ID: %s\n", desc_cstr);

        // Pruefen ob "_sword" im Namen vorkommt
        bool result = (strstr(desc_cstr, "_sword") != nullptr);

        env->ReleaseStringUTFChars(desc, desc_cstr);
        env->DeleteLocalRef(desc);
        env->DeleteLocalRef(item_class);
        env->DeleteLocalRef(item);
        env->DeleteLocalRef(stack);

        std::printf("[*] IsHoldingSword() -> Ergebnis: %s\n", result ? "true" : "false");
        return result;
    }

    int FindObsidianInHotbar(JNIEnv* env, jobject player) {
        if (!p_mc->m_get_inventory) {
            std::printf("[-] FindObsidianInHotbar() -> m_get_inventory ist null!\n");
            return -1;
        }

        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv) {
            std::printf("[-] FindObsidianInHotbar() -> inv ist null.\n");
            return -1;
        }

        if (!p_mc->o_obsidian) {
            std::printf("[-] FindObsidianInHotbar() -> o_obsidian ist null! Nicht initialisiert?\n");
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
                std::printf("[+] FindObsidianInHotbar() -> Obsidian in Slot %d gefunden.\n", i);
                found_slot = i;
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
                break;
            }

            if (item)
                env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
        }

        if (found_slot == -1)
            std::printf("[-] FindObsidianInHotbar() -> Kein Obsidian gefunden.\n");

        env->DeleteLocalRef(inv);
        return found_slot;
    }

    void SwitchToHotbarSlot(JNIEnv* env, jobject player, int slot_index) {
        if (slot_index < 0 || slot_index > 8) {
            std::printf("[-] SwitchToHotbarSlot() -> Ungültiger Slot-Index: %d\n", slot_index);
            return;
        }

        if (!p_mc->m_get_inventory) {
            std::printf("[-] SwitchToHotbarSlot() -> m_get_inventory ist null!\n");
            return;
        }

        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv) {
            std::printf("[-] SwitchToHotbarSlot() -> inv ist null.\n");
            return;
        }

        jclass inv_class = env->GetObjectClass(inv);
        if (!inv_class) {
            std::printf("[-] SwitchToHotbarSlot() -> inv_class ist null!\n");
            env->DeleteLocalRef(inv);
            return;
        }

        jfieldID selected_field = env->GetFieldID(inv_class, "selected", "I");
        if (!selected_field) {
            std::printf("[-] SwitchToHotbarSlot() -> Feld 'selected' nicht gefunden! Falscher Obfuscation-Name?\n");
            env->DeleteLocalRef(inv_class);
            env->DeleteLocalRef(inv);
            return;
        }

        env->SetIntField(inv, selected_field, slot_index);
        std::printf("[+] SwitchToHotbarSlot() -> Slot auf %d gesetzt.\n", slot_index);

        env->DeleteLocalRef(inv_class);
        env->DeleteLocalRef(inv);
    }

    void SwingMainHand(JNIEnv* env, jobject player) {
        if (!p_mc->m_swing_arm) {
            std::printf("[-] SwingMainHand() -> m_swing_arm ist null!\n");
            return;
        }
        if (!p_mc->o_main_hand) {
            std::printf("[-] SwingMainHand() -> o_main_hand ist null!\n");
            return;
        }

        env->CallVoidMethod(player, p_mc->m_swing_arm, p_mc->o_main_hand);
        std::printf("[+] SwingMainHand() -> Swing ausgefuehrt.\n");
    }

    void SwitchAndPlaceCrystal(JNIEnv* env, jobject player) {
        std::printf("[*] SwitchAndPlaceCrystal() -> Suche EndCrystal in Hotbar...\n");

        if (!p_mc->o_endcrystal) {
            std::printf("[-] SwitchAndPlaceCrystal() -> o_end_crystal ist null!\n");
            return;
        }

        // 1. EndCrystal in Hotbar suchen
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv) {
            std::printf("[-] SwitchAndPlaceCrystal() -> inv ist null.\n");
            return;
        }

        int crystal_slot = -1;

        for (int i = 0; i <= 8; i++) {
            jobject stack = env->CallObjectMethod(inv, p_mc->m_get_item_from_inv, i);
            if (!stack)
                continue;

            jboolean empty = env->CallBooleanMethod(stack, p_mc->m_stack_is_empty);
            jobject item = env->CallObjectMethod(stack, p_mc->m_stack_get_item);

            if (!empty && item && env->IsSameObject(item, p_mc->o_endcrystal)) {
                crystal_slot = i;
                std::printf("[+] SwitchAndPlaceCrystal() -> EndCrystal in Slot %d gefunden.\n", i);
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
                break;
            }

            if (item)
                env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
        }

        env->DeleteLocalRef(inv);

        if (crystal_slot == -1) {
            std::printf("[-] SwitchAndPlaceCrystal() -> Kein EndCrystal in der Hotbar.\n");
            return;
        }

        // 2. Auf Crystal-Slot wechseln
        SwitchToHotbarSlot(env, player, crystal_slot);
        std::printf("[+] SwitchAndPlaceCrystal() -> Slot auf %d (EndCrystal) gewechselt.\n", crystal_slot);

        // 3. Kurz warten damit der Slot-Wechsel registriert wird
        Sleep(50);

        // 4. Arm-Swing -> platziert den Crystal
        SwingMainHand(env, player);
        std::printf("[+] SwitchAndPlaceCrystal() -> EndCrystal platziert.\n");
    }

    void ExplodeCrystal(JNIEnv* env, jobject player) {
        std::printf("[*] ExplodeCrystal() -> Starte...\n");

        jobject mc_inst = p_mc->GetInstance( );

        // 1. Level holen
        jclass mc_class = env->GetObjectClass(mc_inst);
        jfieldID f_level = env->GetFieldID(mc_class, "level", "Lnet/minecraft/client/multiplayer/ClientLevel;");
        env->DeleteLocalRef(mc_class);

        if (!f_level) {
            std::printf("[-] ExplodeCrystal() -> Feld 'level' nicht gefunden!\n");
            env->ExceptionClear( );
            return;
        }

        jobject level = env->GetObjectField(mc_inst, f_level);
        if (!level) {
            std::printf("[-] ExplodeCrystal() -> level ist null.\n");
            return;
        }

        // 2. hitResult aus Minecraft holen (crosshair target)
        jclass mc_class2 = env->GetObjectClass(mc_inst);
        jfieldID f_hit = env->GetFieldID(mc_class2, "hitResult", "Lnet/minecraft/world/phys/HitResult;");
        env->DeleteLocalRef(mc_class2);

        if (!f_hit) {
            std::printf("[-] ExplodeCrystal() -> Feld 'hitResult' nicht gefunden!\n");
            env->ExceptionClear( );
            env->DeleteLocalRef(level);
            return;
        }

        jobject hit_result = env->GetObjectField(mc_inst, f_hit);
        if (!hit_result) {
            std::printf("[-] ExplodeCrystal() -> hitResult ist null.\n");
            env->DeleteLocalRef(level);
            return;
        }

        // 3. EntityHitResult Klasse holen
        jclass c_entity_hit = env->FindClass("net/minecraft/world/phys/EntityHitResult");
        if (!c_entity_hit) {
            std::printf("[-] ExplodeCrystal() -> EntityHitResult Klasse nicht gefunden!\n");
            env->ExceptionClear( );
            env->DeleteLocalRef(hit_result);
            env->DeleteLocalRef(level);
            return;
        }

        // 4. Ist das Ziel ein EntityHitResult?
        if (!env->IsInstanceOf(hit_result, c_entity_hit)) {
            std::printf("[-] ExplodeCrystal() -> Ziel ist kein Entity.\n");
            env->DeleteLocalRef(c_entity_hit);
            env->DeleteLocalRef(hit_result);
            env->DeleteLocalRef(level);
            return;
        }

        // 5. Entity aus HitResult holen
        jmethodID m_get_entity = env->GetMethodID(c_entity_hit, "getEntity", "()Lnet/minecraft/world/entity/Entity;");
        env->DeleteLocalRef(c_entity_hit);

        if (!m_get_entity) {
            std::printf("[-] ExplodeCrystal() -> getEntity nicht gefunden!\n");
            env->ExceptionClear( );
            env->DeleteLocalRef(hit_result);
            env->DeleteLocalRef(level);
            return;
        }

        jobject entity = env->CallObjectMethod(hit_result, m_get_entity);
        env->DeleteLocalRef(hit_result);

        if (!entity) {
            std::printf("[-] ExplodeCrystal() -> entity ist null.\n");
            env->DeleteLocalRef(level);
            return;
        }

        // 6. Ist es ein EndCrystal?
        jclass c_end_crystal = env->FindClass("net/minecraft/world/entity/boss/enderdragon/EndCrystal");
        if (!c_end_crystal) {
            std::printf("[-] ExplodeCrystal() -> EndCrystal Klasse nicht gefunden!\n");
            env->ExceptionClear( );
            env->DeleteLocalRef(entity);
            env->DeleteLocalRef(level);
            return;
        }

        if (!env->IsInstanceOf(entity, c_end_crystal)) {
            std::printf("[-] ExplodeCrystal() -> Ziel ist kein EndCrystal.\n");
            env->DeleteLocalRef(c_end_crystal);
            env->DeleteLocalRef(entity);
            env->DeleteLocalRef(level);
            return;
        }
        env->DeleteLocalRef(c_end_crystal);
        std::printf("[+] ExplodeCrystal() -> EndCrystal im Fadenkreuz gefunden!\n");

        // 7. GameMode attack() aufrufen
        jobject game_mode = env->GetObjectField(mc_inst, p_mc->f_game_mode);
        if (!game_mode) {
            std::printf("[-] ExplodeCrystal() -> game_mode ist null!\n");
            env->DeleteLocalRef(entity);
            env->DeleteLocalRef(level);
            return;
        }

        if (!p_mc->m_attack) {
            std::printf("[-] ExplodeCrystal() -> m_attack ist null!\n");
            env->DeleteLocalRef(game_mode);
            env->DeleteLocalRef(entity);
            env->DeleteLocalRef(level);
            return;
        }

        env->CallVoidMethod(game_mode, p_mc->m_attack, player, entity);
        std::printf("[+] ExplodeCrystal() -> attack() ausgefuehrt -> Crystal explodiert!\n");

        // 8. Swing
        SwingMainHand(env, player);

        env->DeleteLocalRef(game_mode);
        env->DeleteLocalRef(entity);
        env->DeleteLocalRef(level);
    }


    int FindSwordInHotbar(JNIEnv* env, jobject player) {
        jobject inv = env->CallObjectMethod(player, p_mc->m_get_inventory);
        if (!inv) {
            std::printf("[-] FindSwordInHotbar() -> inv ist null.\n");
            return -1;
        }

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
                        std::printf("[+] FindSwordInHotbar() -> Schwert in Slot %d gefunden.\n", i);
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

        if (found_slot == -1)
            std::printf("[-] FindSwordInHotbar() -> Kein Schwert in der Hotbar.\n");

        env->DeleteLocalRef(inv);
        return found_slot;
    }

private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
};
