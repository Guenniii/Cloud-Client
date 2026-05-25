#pragma once
#include "../../dependencies/jni/jni.h"
#include <Windows.h>
#include <cstdio>
#include <string>
#include <thread>

class CMinecraft final {
public:
    CMinecraft(JavaVM* p_jvm) : p_jvm(p_jvm) {
        p_jvm->AttachCurrentThread((void**)&p_env, nullptr);
        Init( );
    }

    ~CMinecraft( ) {
        // Aufräumen der Globalen Referenzen
        if (p_env) {
            if (c_minecraft)
                p_env->DeleteGlobalRef(c_minecraft);
            if (c_player)
                p_env->DeleteGlobalRef(c_player);
            if (c_items)
                p_env->DeleteGlobalRef(c_items);
            if (c_item_stack)
                p_env->DeleteGlobalRef(c_item_stack);
            if (c_multiplayer_mode)
                p_env->DeleteGlobalRef(c_multiplayer_mode);
            if (c_inv_screen)
                p_env->DeleteGlobalRef(c_inv_screen);
            if (c_abstract_menu)
                p_env->DeleteGlobalRef(c_abstract_menu);
            if (c_click_type)
                p_env->DeleteGlobalRef(c_click_type);
            if (c_gui)
                p_env->DeleteGlobalRef(c_gui);
            if (c_vec2)
                p_env->DeleteGlobalRef(c_vec2);
            if (c_input)
                p_env->DeleteGlobalRef(c_input);
            if (c_inventory)
                p_env->DeleteGlobalRef(c_inventory);
            if (o_pickup)
                p_env->DeleteGlobalRef(o_pickup);
            if (o_totem)
                p_env->DeleteGlobalRef(o_totem);
            if (o_obsidian)
                p_env->DeleteGlobalRef(o_obsidian);
            if (o_main_hand)
                p_env->DeleteGlobalRef(o_main_hand);
            if (c_sword_item)
                p_env->DeleteGlobalRef(c_sword_item);
            if (c_interaction_hand)
                p_env->DeleteGlobalRef(c_interaction_hand);
        }
    }

    bool m_initialized = false;

    bool IsInitialized( ) const {
        return m_initialized && class_instance != nullptr;
    }

    bool Init( ) {
        if (m_initialized)
            return true;

        std::printf("[*] CMinecraft: Starte zentrale JNI Initialisierung...\n");

        // Helper Lambda für sicheres Laden von Klassen als GlobalRef
        auto GetClass = [&](const char* name) -> jclass {
            jclass l = p_env->FindClass(name);
            if (p_env->ExceptionCheck( )) {
                p_env->ExceptionClear( );
                std::printf("[-] SDK: Klasse nicht gefunden: %s\n", name);
                return nullptr;
            }
            jclass g = (jclass)p_env->NewGlobalRef(l);
            p_env->DeleteLocalRef(l);
            return g;
        };

        // 1. Klassen laden
        c_minecraft = GetClass("net/minecraft/client/Minecraft");
        c_player = GetClass("net/minecraft/client/player/LocalPlayer");
        c_items = GetClass("net/minecraft/world/item/Items");
        c_item_stack = GetClass("net/minecraft/world/item/ItemStack");
        c_multiplayer_mode = GetClass("net/minecraft/client/multiplayer/MultiPlayerGameMode");
        c_inv_screen = GetClass("net/minecraft/client/gui/screens/inventory/InventoryScreen");
        c_abstract_menu = GetClass("net/minecraft/world/inventory/AbstractContainerMenu");
        c_gui = GetClass("net/minecraft/client/gui/Gui");
        c_vec2 = GetClass("net/minecraft/world/phys/Vec2");
        c_inventory = GetClass("net/minecraft/world/entity/player/Inventory");

        // ClickType Fallback-Logik
        const char* paths[] = {"net/minecraft/world/inventory/ClickType", "net/minecraft/world/inventory/ContainerInput"};
        for (const char* p : paths) {
            c_click_type = GetClass(p);
            if (c_click_type) {
                m_click_path = p;
                break;
            }
        }

        // Input Fallback-Logik
        c_input = GetClass("net/minecraft/client/player/ClientInput");
        if (!c_input)
            c_input = GetClass("net/minecraft/client/player/Input");

        if (!c_minecraft || !c_player || !c_click_type) {
            std::printf("[-] SDK: Kritische Basisklassen fehlen!\n");
            return false;
        }

        // 2. Minecraft Instanz holen
        class_ptr = c_minecraft;
        jfieldID class_instance_field_id = p_env->GetStaticFieldID(c_minecraft, "instance", "Lnet/minecraft/client/Minecraft;");
        if (!class_instance_field_id)
            return false;
        class_instance = p_env->GetStaticObjectField(c_minecraft, class_instance_field_id);
        if (!class_instance)
            return false;

        // 3. Fields (Felder) holen
        f_right_click_delay = p_env->GetFieldID(c_minecraft, "rightClickDelay", "I");
        f_player = p_env->GetFieldID(c_minecraft, "player", "Lnet/minecraft/client/player/LocalPlayer;");
        f_game_mode = p_env->GetFieldID(c_minecraft, "gameMode", "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
        f_gui = p_env->GetFieldID(c_minecraft, "gui", "Lnet/minecraft/client/gui/Gui;");

        // Container / Inventory Menu Fallback
        f_inventory_menu = p_env->GetFieldID(c_player, "inventoryMenu", "Lnet/minecraft/world/inventory/InventoryMenu;");
        if (p_env->ExceptionCheck( )) {
            p_env->ExceptionClear( );
            f_inventory_menu = p_env->GetFieldID(c_player, "containerMenu", "Lnet/minecraft/world/inventory/AbstractContainerMenu;");
        }

        f_container_id = p_env->GetFieldID(c_abstract_menu, "containerId", "I");
        f_totem_static = p_env->GetStaticFieldID(c_items, "TOTEM_OF_UNDYING", "Lnet/minecraft/world/item/Item;");

        // --- HIER WAR DER FEHLER: Jetzt sauber mit std::string gelöst ---
        std::string input_signature = "L";
        input_signature += (c_input ? "net/minecraft/client/player/ClientInput" : "net/minecraft/client/player/Input");
        input_signature += ";";
        f_input = p_env->GetFieldID(c_player, "input", input_signature.c_str( ));

        // noJumpDelay (LivingEntity Fallback)
        f_no_jump_delay = p_env->GetFieldID(c_player, "noJumpDelay", "I");
        if (p_env->ExceptionCheck( )) {
            p_env->ExceptionClear( );
            jclass livingEntityClass = p_env->GetSuperclass(c_player);
            f_no_jump_delay = p_env->GetFieldID(livingEntityClass, "noJumpDelay", "I");
            p_env->DeleteLocalRef(livingEntityClass);
        }

        f_move_vector = p_env->GetFieldID(c_input, "moveVector", "Lnet/minecraft/world/phys/Vec2;");
        f_vec2_y = p_env->GetFieldID(c_vec2, "y", "F");

        // 4. Methods (Methoden) holen
        m_set_screen = p_env->GetMethodID(c_gui, "setScreen", "(Lnet/minecraft/client/gui/screens/Screen;)V");
        m_inv_screen_init = p_env->GetMethodID(c_inv_screen, "<init>", "(Lnet/minecraft/world/entity/player/Player;)V");
        m_stack_is_empty = p_env->GetMethodID(c_item_stack, "isEmpty", "()Z");
        m_stack_get_item = p_env->GetMethodID(c_item_stack, "getItem", "()Lnet/minecraft/world/item/Item;");
        m_get_offhand_item = p_env->GetMethodID(c_player, "getOffhandItem", "()Lnet/minecraft/world/item/ItemStack;");
        m_get_inventory = p_env->GetMethodID(c_player, "getInventory", "()Lnet/minecraft/world/entity/player/Inventory;");
        m_get_item_from_inv = p_env->GetMethodID(c_inventory, "getItem", "(I)Lnet/minecraft/world/item/ItemStack;");
        m_is_shifting = p_env->GetMethodID(c_player, "isShiftKeyDown", "()Z");
        m_set_sprinting = p_env->GetMethodID(c_player, "setSprinting", "(Z)V");

        // Click-Methode Verzweigung
        std::string click_sig = "(IIIL" + m_click_path + ";Lnet/minecraft/world/entity/player/Player;)V";
        m_handle_inv_click = p_env->GetMethodID(c_multiplayer_mode, "handleInventoryMouseClick", click_sig.c_str( ));
        if (p_env->ExceptionCheck( ) || !m_handle_inv_click) {
            p_env->ExceptionClear( );
            m_handle_inv_click = p_env->GetMethodID(c_multiplayer_mode, "handleContainerInput", click_sig.c_str( ));
        }

        // 5. Globale Objekte holen
        jfieldID fid_pickup = p_env->GetStaticFieldID(c_click_type, "PICKUP", ("L" + m_click_path + ";").c_str( ));
        if (fid_pickup) {
            jobject local_pickup = p_env->GetStaticObjectField(c_click_type, fid_pickup);
            if (local_pickup)
                o_pickup = p_env->NewGlobalRef(local_pickup);
        }

        if (f_totem_static) {
            jobject local_totem = p_env->GetStaticObjectField(c_items, f_totem_static);
            if (local_totem)
                o_totem = p_env->NewGlobalRef(local_totem);
        }


        // In Init() nach dem Totem-Block einfügen:

        // Obsidian Singleton
        jfieldID fid_obsidian = p_env->GetStaticFieldID(c_items, "OBSIDIAN", "Lnet/minecraft/world/item/Item;");
        if (fid_obsidian) {
            jobject local_obsidian = p_env->GetStaticObjectField(c_items, fid_obsidian);
            if (local_obsidian)
                o_obsidian = p_env->NewGlobalRef(local_obsidian);
        }

        jfieldID fid_endcrystal = p_env->GetStaticFieldID(c_items, "END_CRYSTAL", "Lnet/minecraft/world/item/Item;");
        if (fid_endcrystal) {
            jobject local_endcrystal = p_env->GetStaticObjectField(c_items, fid_endcrystal);
            if (local_endcrystal)
                o_endcrystal = p_env->NewGlobalRef(local_endcrystal);
        }


        // InteractionHand
        c_interaction_hand = GetClass("net/minecraft/world/InteractionHand");
        if (c_interaction_hand) {
            jobject local_main_hand = p_env->GetStaticObjectField(
                c_interaction_hand,
                p_env->GetStaticFieldID(c_interaction_hand, "MAIN_HAND", "Lnet/minecraft/world/InteractionHand;"));
            if (local_main_hand)
                o_main_hand = p_env->NewGlobalRef(local_main_hand);
        }

        // Methoden
        m_get_main_hand_item = p_env->GetMethodID(c_player, "getMainHandItem", "()Lnet/minecraft/world/item/ItemStack;");
        m_swing_arm = p_env->GetMethodID(c_player, "swing", "(Lnet/minecraft/world/InteractionHand;)V");
        m_attack = p_env->GetMethodID(c_multiplayer_mode, "attack", "(Lnet/minecraft/world/entity/player/Player;Lnet/minecraft/world/entity/Entity;)V");

        m_initialized = true;
        std::printf("[+] CMinecraft: Komplettes SDK erfolgreich zentral initialisiert!\n");
        return true;
    }

    void SetRightClickDelay(int new_delay) {
        JNIEnv* env = nullptr;
        p_jvm->AttachCurrentThread((void**)&env, nullptr);
        if (!f_right_click_delay || !class_instance)
            return;
        env->SetIntField(class_instance, f_right_click_delay, new_delay);
    }

    void StartRightClickLoop( ) {
        std::thread([this]( ) {
            JNIEnv* env = nullptr;
            p_jvm->AttachCurrentThread((void**)&env, nullptr);
            while (true) {
                if (IsInitialized( ) && f_right_click_delay) {
                    env->SetIntField(class_instance, f_right_click_delay, 0);
                }
                Sleep(50);
            }
            p_jvm->DetachCurrentThread( );
        }).detach( );
    }

    // Getters & SDK Cache Variablen
    jobject GetInstance( ) const { return class_instance; }
    JavaVM* GetJVM( ) const { return p_jvm; }
public:
    // Klassen (GlobalRefs)
    jclass c_minecraft = nullptr, c_player = nullptr, c_items = nullptr, c_item_stack = nullptr, c_multiplayer_mode = nullptr, c_click_type = nullptr, c_inv_screen = nullptr, c_abstract_menu = nullptr, c_gui = nullptr, c_vec2 = nullptr, c_input = nullptr, c_inventory = nullptr;

    // Felder (IDs)
    jfieldID f_right_click_delay = nullptr, f_player = nullptr, f_game_mode = nullptr, f_gui = nullptr, f_inventory_menu = nullptr, f_container_id = nullptr, f_totem_static = nullptr, f_no_jump_delay = nullptr, f_input = nullptr, f_move_vector = nullptr, f_vec2_y = nullptr;

    // Methoden (IDs)
    jmethodID m_set_screen = nullptr, m_inv_screen_init = nullptr, m_stack_is_empty = nullptr, m_stack_get_item = nullptr, m_handle_inv_click = nullptr, m_get_offhand_item = nullptr, m_get_inventory = nullptr, m_get_item_from_inv = nullptr, m_is_shifting = nullptr, m_set_sprinting = nullptr;

    // Objekte (GlobalRefs)
    jobject o_totem = nullptr, o_pickup = nullptr;

    jclass c_sword_item = nullptr;
    jclass c_interaction_hand = nullptr;
    jobject o_obsidian = nullptr;
    jobject o_endcrystal = nullptr;
    jobject o_main_hand = nullptr;
    jmethodID m_get_main_hand_item = nullptr;
    jmethodID m_swing_arm = nullptr;
    jmethodID m_attack = nullptr;

private:
    jclass class_ptr = nullptr;
    jobject class_instance = nullptr;
    JNIEnv* p_env = nullptr;
    JavaVM* p_jvm = nullptr;
    std::string m_click_path;
    const char name[10] = {"Minecraft"};
};
