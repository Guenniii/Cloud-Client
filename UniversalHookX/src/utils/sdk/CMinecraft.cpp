#include "CMinecraft.h"
#include "jni_refs.hpp"
#include "jni_safety.hpp"
#include "../seedcracker/seedcracker_bridge.hpp"
#include "../lifecycle/lifecycle.hpp"
#include <Windows.h>
#include <cstdio>
#include <thread>

CMinecraft::CMinecraft(JavaVM* p_jvm) : p_jvm(p_jvm) {
        if (!p_jvm || p_jvm->AttachCurrentThread((void**)&p_env, nullptr) != JNI_OK || !p_env) return;
        Init( );
    }

CMinecraft::~CMinecraft( ) {
        JNIEnv* current=nullptr;
        if(p_jvm){auto result=p_jvm->GetEnv(reinterpret_cast<void**>(&current),JNI_VERSION_1_8);
            if(result==JNI_EDETACHED)p_jvm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&current),nullptr);}
        p_env=current;
        // Aufräumen der Globalen Referenzen
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
            if (c_sword_item)
                p_env->DeleteGlobalRef(c_sword_item);
            if (c_interaction_hand)
                p_env->DeleteGlobalRef(c_interaction_hand);
        }
    }

bool CMinecraft::IsInitialized( ) const {
        return m_initialized && class_instance != nullptr;
    }

bool CMinecraft::Init( ) {
    if(!p_env)return false;if(m_initialized)return true;
    if(!GameRuntime::Initialize(p_env)){std::printf("[-] SDK: Minecraft class loader unavailable.\n");return false;}
    JniSafety::Lookup api(p_env);
    auto cls=[&](const char* name){auto local=api.FindClass(name);return static_cast<jclass>(JniRefs::PromoteLocal(p_env,local));};
    c_minecraft=cls("net/minecraft/client/Minecraft");c_player=cls("net/minecraft/client/player/LocalPlayer");
    c_items=cls("net/minecraft/world/item/Items");c_item_stack=cls("net/minecraft/world/item/ItemStack");
    c_multiplayer_mode=cls("net/minecraft/client/multiplayer/MultiPlayerGameMode");
    c_inv_screen=cls("net/minecraft/client/gui/screens/inventory/InventoryScreen");
    c_abstract_menu=cls("net/minecraft/world/inventory/AbstractContainerMenu");c_gui=cls("net/minecraft/client/gui/Gui");
    c_vec2=cls("net/minecraft/world/phys/Vec2");c_inventory=cls("net/minecraft/world/entity/player/Inventory");
    c_click_type=cls("net/minecraft/world/inventory/ContainerInput");m_click_path="net/minecraft/world/inventory/ContainerInput";
    if(!c_click_type){c_click_type=cls("net/minecraft/world/inventory/ClickType");m_click_path="net/minecraft/world/inventory/ClickType";}
    std::string inputPath="net/minecraft/client/player/ClientInput";c_input=cls(inputPath.c_str());
    if(!c_input){inputPath="net/minecraft/client/player/Input";c_input=cls(inputPath.c_str());}
    if(!c_minecraft||!c_player||!c_items||!c_item_stack||!c_multiplayer_mode||!c_click_type){std::printf("[-] SDK: Required runtime classes could not be resolved.\n");return false;}
    class_ptr=c_minecraft;
    auto instance=api.GetStaticMethodID(c_minecraft,"getInstance","()Lnet/minecraft/client/Minecraft;");
    auto local=api.CallStaticObjectMethod(c_minecraft,instance);
    if(!local)local=api.GetStaticObjectField(c_minecraft,api.GetStaticFieldID(c_minecraft,"instance","Lnet/minecraft/client/Minecraft;"));
    class_instance=JniRefs::PromoteLocal(p_env,local);if(!class_instance)return false;
    f_right_click_delay=api.GetFieldID(c_minecraft,"rightClickDelay","I");
    f_player=api.GetFieldID(c_minecraft,"player","Lnet/minecraft/client/player/LocalPlayer;");
    f_game_mode=api.GetFieldID(c_minecraft,"gameMode","Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
    f_gui=api.GetFieldID(c_minecraft,"gui","Lnet/minecraft/client/gui/Gui;");
    f_inventory_menu=api.GetFieldID(c_player,"inventoryMenu","Lnet/minecraft/world/inventory/InventoryMenu;");
    if(!f_inventory_menu)f_inventory_menu=api.GetFieldID(c_player,"containerMenu","Lnet/minecraft/world/inventory/AbstractContainerMenu;");
    f_container_id=api.GetFieldID(c_abstract_menu,"containerId","I");
    f_totem_static=api.GetStaticFieldID(c_items,"TOTEM_OF_UNDYING","Lnet/minecraft/world/item/Item;");
    f_input=api.GetFieldID(c_player,"input",("L"+inputPath+";").c_str());
    f_no_jump_delay=api.GetFieldID(c_player,"noJumpDelay","I");
    f_move_vector=api.GetFieldID(c_input,"moveVector","Lnet/minecraft/world/phys/Vec2;");f_vec2_y=api.GetFieldID(c_vec2,"y","F");
    m_set_screen=api.GetMethodID(c_gui,"setScreen","(Lnet/minecraft/client/gui/screens/Screen;)V");
    m_inv_screen_init=api.GetMethodID(c_inv_screen,"<init>","(Lnet/minecraft/world/entity/player/Player;)V");
    m_stack_is_empty=api.GetMethodID(c_item_stack,"isEmpty","()Z");m_stack_get_item=api.GetMethodID(c_item_stack,"getItem","()Lnet/minecraft/world/item/Item;");
    m_get_offhand_item=api.GetMethodID(c_player,"getOffhandItem","()Lnet/minecraft/world/item/ItemStack;");
    m_get_inventory=api.GetMethodID(c_player,"getInventory","()Lnet/minecraft/world/entity/player/Inventory;");
    m_get_item_from_inv=api.GetMethodID(c_inventory,"getItem","(I)Lnet/minecraft/world/item/ItemStack;");
    m_is_shifting=api.GetMethodID(c_player,"isShiftKeyDown","()Z");m_set_sprinting=api.GetMethodID(c_player,"setSprinting","(Z)V");
    auto click="(IIIL"+m_click_path+";Lnet/minecraft/world/entity/player/Player;)V";
    m_handle_inv_click=api.GetMethodID(c_multiplayer_mode,"handleContainerInput",click.c_str());
    if(!m_handle_inv_click)m_handle_inv_click=api.GetMethodID(c_multiplayer_mode,"handleInventoryMouseClick",click.c_str());
    o_pickup=JniRefs::PromoteLocal(p_env,api.GetStaticObjectField(c_click_type,api.GetStaticFieldID(c_click_type,"PICKUP",("L"+m_click_path+";").c_str())));
    o_totem=JniRefs::PromoteLocal(p_env,api.GetStaticObjectField(c_items,f_totem_static));
    o_obsidian=JniRefs::PromoteLocal(p_env,api.GetStaticObjectField(c_items,api.GetStaticFieldID(c_items,"OBSIDIAN","Lnet/minecraft/world/item/Item;")));
    o_endcrystal=JniRefs::PromoteLocal(p_env,api.GetStaticObjectField(c_items,api.GetStaticFieldID(c_items,"END_CRYSTAL","Lnet/minecraft/world/item/Item;")));
    c_interaction_hand=cls("net/minecraft/world/InteractionHand");
    o_main_hand=JniRefs::PromoteLocal(p_env,api.GetStaticObjectField(c_interaction_hand,api.GetStaticFieldID(c_interaction_hand,"MAIN_HAND","Lnet/minecraft/world/InteractionHand;")));
    m_get_main_hand_item=api.GetMethodID(c_player,"getMainHandItem","()Lnet/minecraft/world/item/ItemStack;");
    m_swing_arm=api.GetMethodID(c_player,"swing","(Lnet/minecraft/world/InteractionHand;)V");
    m_attack=api.GetMethodID(c_multiplayer_mode,"attack","(Lnet/minecraft/world/entity/player/Player;Lnet/minecraft/world/entity/Entity;)V");
    m_initialized=f_player&&f_game_mode&&m_get_inventory&&m_stack_is_empty;
    std::printf("[%s] SDK: loader-aware initialization %s; inventory=%s.\n",m_initialized?"+":"-",m_initialized?"ready":"incomplete",m_handle_inv_click?"ready":"unavailable");
    return m_initialized;
}

void CMinecraft::Swing(JNIEnv* env,jobject player) {
    if(!env||!player||env->ExceptionCheck())return;
    if(m_swing_arm&&o_main_hand){env->CallVoidMethod(player,m_swing_arm,o_main_hand);if(env->ExceptionCheck())env->ExceptionClear();return;}
    JniSafety::LocalFrame frame(env,8);if(!frame)return;
    auto cls=SeedCracker::LoadExtensionClass(env,"phantomui.compat.NativeActions");
    if(env->ExceptionCheck()){env->ExceptionClear();return;}
    JniSafety::Lookup api(env);auto method=api.GetStaticMethodID(cls,"swing","(Ljava/lang/Object;)V");
    if(cls&&method){env->CallStaticVoidMethod(cls,method,player);if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();}}
}

void CMinecraft::SetRightClickDelay(int new_delay) {
        JNIEnv* env = nullptr;
        if (!p_jvm || p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env) return;
        if (!f_right_click_delay || !class_instance)
            return;
        env->SetIntField(class_instance, f_right_click_delay, new_delay);
    }

void CMinecraft::StartRightClickLoop( ) {
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

jobject CMinecraft::GetInstance( ) const { return class_instance; }

JavaVM* CMinecraft::GetJVM( ) const { return p_jvm; }
