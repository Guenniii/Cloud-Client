#include "CFabric.h"
#include "jni_refs.hpp"
#include "../lifecycle/lifecycle.hpp"
#include <Windows.h>
#include <cstdio>
#include <thread>

CFabric::CFabric(JavaVM* p_jvm) : p_jvm(p_jvm) {
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_8;
        args.name = const_cast<char*>("Fabric-SDK-Thread");
        args.group = nullptr;

        if (!p_jvm || p_jvm->AttachCurrentThread((void**)&p_env, &args) != JNI_OK || !p_env) return;
        Init( );
    }

CFabric::~CFabric( ) {
        std::printf("[*] CFabric: Bereinige globale Fabric-JNI-Referenzen...\n");
        if (p_env) {
            if (class_instance)
                p_env->DeleteGlobalRef(class_instance);
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
            if (o_endcrystal)
                p_env->DeleteGlobalRef(o_endcrystal);
            if (o_main_hand)
                p_env->DeleteGlobalRef(o_main_hand);
            if (c_interaction_hand)
                p_env->DeleteGlobalRef(c_interaction_hand);
            if (class_loader_obj)
                p_env->DeleteGlobalRef(class_loader_obj);
        }
    }

bool CFabric::IsInitialized( ) const {
        return m_initialized && class_instance != nullptr;
    }

jclass CFabric::FindClassWithLoader(const char* name) {
        if (!class_loader_obj || !find_class_method) {
            return p_env->FindClass(name);
        }

        std::string dot_name = name;
        for (char& c : dot_name) {
            if (c == '/')
                c = '.';
        }

        jstring jname = p_env->NewStringUTF(dot_name.c_str( ));
        jclass loaded_class = (jclass)p_env->CallObjectMethod(class_loader_obj, find_class_method, jname);
        p_env->DeleteLocalRef(jname);

        if (p_env->ExceptionCheck( )) {
            p_env->ExceptionClear( );
            return nullptr;
        }
        return loaded_class;
    }

bool CFabric::Init( ) {
        if (!p_env) return false;
        if (m_initialized)
            return true;

        std::printf("[*] CFabric: Starte Fabric-JNI Initialisierung über Thread-ClassLoader...\n");

        // --- SCHRITT 1: ClassLoader aus den aktiven Java-Threads extrahieren ---
        jclass thread_class = p_env->FindClass("java/lang/Thread");
        jmethodID get_threads = p_env->GetStaticMethodID(thread_class, "getAllStackTraces", "()Ljava/util/Map;");
        jobject map_obj = p_env->CallStaticObjectMethod(thread_class, get_threads);

        jclass map_class = p_env->FindClass("java/util/Map");
        jmethodID key_set = p_env->GetMethodID(map_class, "keySet", "()Ljava/util/Set;");
        jobject set_obj = p_env->CallObjectMethod(map_obj, key_set);

        jclass set_class = p_env->FindClass("java/util/Set");
        jmethodID to_array = p_env->GetMethodID(set_class, "toArray", "()[Ljava/lang/Object;");
        jobjectArray thread_array = (jobjectArray)p_env->CallObjectMethod(set_obj, to_array);

        jsize length = p_env->GetArrayLength(thread_array);
        jobject local_loader = nullptr;

        for (int i = 0; i < length; i++) {
            jobject thread = p_env->GetObjectArrayElement(thread_array, i);
            jmethodID get_loader = p_env->GetMethodID(thread_class, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
            jobject loader = p_env->CallObjectMethod(thread, get_loader);

            if (loader) {
                local_loader = loader;
                p_env->DeleteLocalRef(thread);
                break;
            }
            p_env->DeleteLocalRef(thread);
        }

        if (local_loader) {
            class_loader_obj = p_env->NewGlobalRef(local_loader);
            jclass loader_class = p_env->FindClass("java/lang/ClassLoader");
            find_class_method = p_env->GetMethodID(loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
            p_env->DeleteLocalRef(local_loader);
        } else {
            std::printf("[-] CFabric: Kein gültiger ClassLoader in den Threads gefunden!\n");
            return false;
        }

        auto GetClass = [&](const char* name) -> jclass {
            jclass l = FindClassWithLoader(name);
            if (!l) {
                std::printf("[-] CFabric SDK: Klasse nicht gefunden: %s\n", name);
                return nullptr;
            }
            jclass g = (jclass)p_env->NewGlobalRef(l);
            p_env->DeleteLocalRef(l);
            return g;
        };

        // --- SCHRITT 2: Klassen laden (Hier mit Fabric Intermediary Mappings!) ---
        c_minecraft = GetClass("net/minecraft/class_310");        // Minecraft
        c_player = GetClass("net/minecraft/client/player/LocalPlayer");           // LocalPlayer
        c_items = GetClass("net/minecraft/class_1792");           // Items
        c_item_stack = GetClass("net/minecraft/class_1799");      // ItemStack
        c_multiplayer_mode = GetClass("net/minecraft/class_636"); // MultiPlayerGameMode
        c_inv_screen = GetClass("net/minecraft/class_490");       // InventoryScreen
        c_abstract_menu = GetClass("net/minecraft/class_1703");   // AbstractContainerMenu
        c_gui = GetClass("net/minecraft/class_329");              // Gui
        c_vec2 = GetClass("net/minecraft/class_241");             // Vec2
        c_inventory = GetClass("net/minecraft/class_1661");       // Inventory

        // Fabric ClickType / ContainerInput Weiche
        const char* paths[] = {"net/minecraft/class_1713", "net/minecraft/world/inventory/ClickType"};
        for (const char* p : paths) {
            c_click_type = GetClass(p);
            if (c_click_type) {
                m_click_path = p;
                break;
            }
        }

        c_input = GetClass("net/minecraft/class_744"); // ClientInput / Input
        if (!c_input)
            c_input = GetClass("net/minecraft/class_743");

        if (!c_minecraft || !c_player || !c_click_type) {
            std::printf("[-] CFabric SDK: Kritische Fabric-Basisklassen fehlen!\n");
            return false;
        }

        // --- SCHRITT 3: Minecraft Instanz holen ---
        class_ptr = c_minecraft;
        // Feldnamen müssen für reines Fabric ggf. angepasst werden (hier als "field_1724" oder "instance" je nach Fabric-Injektor)
        jfieldID class_instance_field_id = p_env->GetStaticFieldID(c_minecraft, "field_1724", "Lnet/minecraft/class_310;");
        if (!class_instance_field_id) {
            p_env->ExceptionClear( );
            class_instance_field_id = p_env->GetStaticFieldID(c_minecraft, "instance", "Lnet/minecraft/class_310;");
        }
        if (!class_instance_field_id)
            return false;

        class_instance = JniRefs::PromoteLocal(p_env, p_env->GetStaticObjectField(c_minecraft, class_instance_field_id));
        if (!class_instance)
            return false;

        // --- SCHRITT 4: Fields holen (Intermediary + Fallbacks) ---
        f_right_click_delay = p_env->GetFieldID(c_minecraft, "field_1762", "I");                 // rightClickDelay
        f_player = p_env->GetFieldID(c_minecraft, "field_1724", "Lnet/minecraft/class_746;");    // player
        f_game_mode = p_env->GetFieldID(c_minecraft, "field_1761", "Lnet/minecraft/class_636;"); // gameMode
        f_gui = p_env->GetFieldID(c_minecraft, "field_1705", "Lnet/minecraft/class_329;");       // gui

        f_inventory_menu = p_env->GetFieldID(c_player, "field_3931", "Lnet/minecraft/class_1723;"); // inventoryMenu
        if (p_env->ExceptionCheck( )) {
            p_env->ExceptionClear( );
            f_inventory_menu = p_env->GetFieldID(c_player, "field_3931", "Lnet/minecraft/class_1703;"); // containerMenu
        }

        f_container_id = p_env->GetFieldID(c_abstract_menu, "field_7763", "I");                        // containerId
        f_totem_static = p_env->GetStaticFieldID(c_items, "field_8281", "Lnet/minecraft/class_1792;"); // TOTEM_OF_UNDYING

        std::string input_signature = "L" + std::string(m_click_path.find("class_") != std::string::npos ? "net/minecraft/class_744" : "net/minecraft/class_743") + ";";
        f_input = p_env->GetFieldID(c_player, "field_3914", input_signature.c_str( )); // input

        f_no_jump_delay = p_env->GetFieldID(c_player, "field_6234", "I"); // noJumpDelay
        if (p_env->ExceptionCheck( )) {
            p_env->ExceptionClear( );
            jclass livingEntityClass = p_env->GetSuperclass(c_player);
            f_no_jump_delay = p_env->GetFieldID(livingEntityClass, "field_6234", "I");
            p_env->DeleteLocalRef(livingEntityClass);
        }

        f_move_vector = p_env->GetFieldID(c_input, "field_3905", "Lnet/minecraft/class_241;"); // moveVector
        f_vec2_y = p_env->GetFieldID(c_vec2, "field_1334", "F");                               // y

        // --- SCHRITT 5: Methods holen ---
        m_set_screen = p_env->GetMethodID(c_gui, "method_1740", "(Lnet/minecraft/class_437;)V");               // setScreen
        m_inv_screen_init = p_env->GetMethodID(c_inv_screen, "<init>", "(Lnet/minecraft/class_1657;)V");       // <init>(Player)
        m_stack_is_empty = p_env->GetMethodID(c_item_stack, "method_7960", "()Z");                             // isEmpty
        m_stack_get_item = p_env->GetMethodID(c_item_stack, "method_7909", "()Lnet/minecraft/class_1792;");    // getItem
        m_get_offhand_item = p_env->GetMethodID(c_player, "method_6079", "()Lnet/minecraft/class_1799;");      // getOffhandItem
        m_get_inventory = p_env->GetMethodID(c_player, "method_31548", "()Lnet/minecraft/class_1661;");        // getInventory
        m_get_item_from_inv = p_env->GetMethodID(c_inventory, "method_5438", "(I)Lnet/minecraft/class_1799;"); // getItem(slot)
        m_is_shifting = p_env->GetMethodID(c_player, "method_5715", "()Z");                                    // isShiftKeyDown
        m_set_sprinting = p_env->GetMethodID(c_player, "method_5728", "(Z)V");                                 // setSprinting

        std::string click_sig = "(IIIL" + m_click_path + ";Lnet/minecraft/class_1657;)V";
        m_handle_inv_click = p_env->GetMethodID(c_multiplayer_mode, "method_2906", click_sig.c_str( )); // handleInventoryMouseClick

        // --- SCHRITT 6: Globale Singletons (Enums / Objekte) holen ---
        jfieldID fid_pickup = p_env->GetStaticFieldID(c_click_type, "field_7790", ("L" + m_click_path + ";").c_str( )); // PICKUP
        if (fid_pickup) {
            jobject local_pickup = p_env->GetStaticObjectField(c_click_type, fid_pickup);
            if (local_pickup)
                o_pickup = JniRefs::PromoteLocal(p_env, local_pickup);
        }

        if (f_totem_static) {
            jobject local_totem = p_env->GetStaticObjectField(c_items, f_totem_static);
            if (local_totem)
                o_totem = JniRefs::PromoteLocal(p_env, local_totem);
        }

        jfieldID fid_obsidian = p_env->GetStaticFieldID(c_items, "field_8038", "Lnet/minecraft/class_1792;"); // OBSIDIAN
        if (fid_obsidian) {
            jobject local_obsidian = p_env->GetStaticObjectField(c_items, fid_obsidian);
            if (local_obsidian)
                o_obsidian = JniRefs::PromoteLocal(p_env, local_obsidian);
        }

        jfieldID fid_endcrystal = p_env->GetStaticFieldID(c_items, "field_8314", "Lnet/minecraft/class_1792;"); // END_CRYSTAL
        if (fid_endcrystal) {
            jobject local_endcrystal = p_env->GetStaticObjectField(c_items, fid_endcrystal);
            if (local_endcrystal)
                o_endcrystal = JniRefs::PromoteLocal(p_env, local_endcrystal);
        }

        c_interaction_hand = GetClass("net/minecraft/class_1268"); // InteractionHand
        if (c_interaction_hand) {
            jobject local_main_hand = p_env->GetStaticObjectField(
                c_interaction_hand,
                p_env->GetStaticFieldID(c_interaction_hand, "field_5808", "Lnet/minecraft/class_1268;")); // MAIN_HAND
            if (local_main_hand)
                o_main_hand = JniRefs::PromoteLocal(p_env, local_main_hand);
        }

        m_get_main_hand_item = p_env->GetMethodID(c_player, "method_6047", "()Lnet/minecraft/class_1799;");                          // getMainHandItem
        m_swing_arm = p_env->GetMethodID(c_player, "method_6104", "(Lnet/minecraft/class_1268;)V");                                  // swing
        m_attack = p_env->GetMethodID(c_multiplayer_mode, "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V"); // attack

        m_initialized = true;
        std::printf("[+] CFabric: Komplettes SDK erfolgreich über Fabric-Mappings initialisiert!\n");
        return true;
    }

void CFabric::SetRightClickDelay(int new_delay) {
        JNIEnv* env = nullptr;
        if (!p_jvm || p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env) return;
        if (!f_right_click_delay || !class_instance)
            return;
        env->SetIntField(class_instance, f_right_click_delay, new_delay);
    }

void CFabric::StartRightClickLoop( ) {
        Lifecycle::Spawn([this]( ) {
            JNIEnv* env = nullptr;
            if (!p_jvm || p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env) return;
            while (!Lifecycle::requested) {
                if (IsInitialized( ) && f_right_click_delay) {
                    env->SetIntField(class_instance, f_right_click_delay, 0);
                }
                Sleep(50);
            }
            p_jvm->DetachCurrentThread( );
        });
    }

jobject CFabric::GetInstance( ) const { return class_instance; }

JavaVM* CFabric::GetJVM( ) const { return p_jvm; }
