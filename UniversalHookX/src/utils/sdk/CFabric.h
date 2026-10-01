#pragma once
#include "../../dependencies/jni/jni.h"
#include <string>

class CFabric final {
public:
    CFabric(JavaVM* p_jvm);

    ~CFabric( );

    bool m_initialized = false;
    jobject class_loader_obj = nullptr;
    jmethodID find_class_method = nullptr;

    bool IsInitialized( ) const;

    // Hilfsfunktion: Lädt Klassen über den Fabric-ClassLoader (konvertiert / zu .)
    jclass FindClassWithLoader(const char* name);

    bool Init( );

    void SetRightClickDelay(int new_delay);

    void StartRightClickLoop( );

    jobject GetInstance( ) const;
    JavaVM* GetJVM( ) const;
public:
    jclass c_minecraft = nullptr, c_player = nullptr, c_items = nullptr, c_item_stack = nullptr, c_multiplayer_mode = nullptr, c_click_type = nullptr, c_inv_screen = nullptr, c_abstract_menu = nullptr, c_gui = nullptr, c_vec2 = nullptr, c_input = nullptr, c_inventory = nullptr;
    jfieldID f_right_click_delay = nullptr, f_player = nullptr, f_game_mode = nullptr, f_gui = nullptr, f_inventory_menu = nullptr, f_container_id = nullptr, f_totem_static = nullptr, f_no_jump_delay = nullptr, f_input = nullptr, f_move_vector = nullptr, f_vec2_y = nullptr;
    jmethodID m_set_screen = nullptr, m_inv_screen_init = nullptr, m_stack_is_empty = nullptr, m_stack_get_item = nullptr, m_handle_inv_click = nullptr, m_get_offhand_item = nullptr, m_get_inventory = nullptr, m_get_item_from_inv = nullptr, m_is_shifting = nullptr, m_set_sprinting = nullptr;
    jobject o_totem = nullptr, o_pickup = nullptr;
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
};
