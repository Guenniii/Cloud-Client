#include "CwCrystal.hpp"
#include "../../utils/sdk/jni_safety.hpp"
#include "../../utils/sdk/CMinecraft.h"
#include "../../utils/lifecycle/lifecycle.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

CwCrystal::CwCrystal(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) { }

void CwCrystal::Start( ) {
        if (Lifecycle::requested) return;
        m_running = true;
        if(m_workerStarted || GetTickCount64()<m_nextStartAttempt) return;
        m_nextStartAttempt=GetTickCount64()+1000;
        if (m_workerStarted.exchange(true)) return;
        Lifecycle::Spawn([this] {
            struct Reset { std::atomic<bool>& flag; ~Reset() { flag = false; } } reset{m_workerStarted};
            TickLoop();
        });
    }

void CwCrystal::Stop() {
    std::lock_guard lock(m_tickMutex);
    m_running = false;
}

jclass CwCrystal::FindGlobal(JNIEnv* env, const char* name) {
        jclass local = env->FindClass(name);
        if (!local) {
            if(env->ExceptionCheck()) env->ExceptionClear();
            printf("[-] Class not found: %s\n", name);
            return nullptr;
        }
        jclass global = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return global;
    }

bool CwCrystal::CacheIDs(JNIEnv* env) {
        if (m_ids_cached)
            return true;

        // Klassennamen ggf. an deine MC-Version anpassen!
        c_Minecraft = FindGlobal(env, "net/minecraft/client/Minecraft");
        c_ClientPlayerEntity = FindGlobal(env, "net/minecraft/client/player/LocalPlayer");
        c_InteractionManager = FindGlobal(env, "net/minecraft/client/multiplayer/MultiPlayerGameMode");
        c_EntityHitResult = FindGlobal(env, "net/minecraft/world/phys/EntityHitResult");
        c_BlockHitResult = FindGlobal(env, "net/minecraft/world/phys/BlockHitResult");
        c_EndCrystalEntity = FindGlobal(env, "net/minecraft/world/entity/boss/enderdragon/EndCrystal");
        c_PlayerInventory = FindGlobal(env, "net/minecraft/world/entity/player/Inventory");
        c_Items = FindGlobal(env, "net/minecraft/world/item/Items");
        c_BlockPos = FindGlobal(env, "net/minecraft/core/BlockPos");
        c_BlockState = FindGlobal(env, "net/minecraft/world/level/block/state/BlockState");
        c_Block = FindGlobal(env, "net/minecraft/world/level/block/Block");
        c_Blocks = FindGlobal(env, "net/minecraft/world/level/block/Blocks");
        c_ClientWorld = FindGlobal(env, "net/minecraft/client/multiplayer/ClientLevel");
        c_Hand = FindGlobal(env, "net/minecraft/world/InteractionHand");
        c_ActionResult = FindGlobal(env, "net/minecraft/world/InteractionResult");
        c_Window = FindGlobal(env, "com/mojang/blaze3d/platform/Window");

        // Prüfen ob alles geklappt hat
        if (!c_Minecraft || !c_ClientPlayerEntity || !c_InteractionManager) {
            printf("[-] CwCrystal: Critical class missing!\n");
            return false;
        }



        m_ids_cached = true;
        printf("[+] CwCrystal: All classes cached.\n");
        return true;

        
    }

void CwCrystal::TickLoop( ) {
        JNIEnv* env = nullptr;
        jint attach_result = p_jvm->AttachCurrentThread((void**)&env, nullptr);
        if (attach_result != JNI_OK || !env) {
            printf("[-] CwCrystal: AttachCurrentThread failed!\n");
            return;
        }

        JniSafety::ScopeExit cleanup{[this, env] {
            if(env->ExceptionCheck()) env->ExceptionClear();
            if(c_Minecraft) { env->DeleteGlobalRef(c_Minecraft); c_Minecraft=nullptr; }
            if(c_ClientPlayerEntity) { env->DeleteGlobalRef(c_ClientPlayerEntity); c_ClientPlayerEntity=nullptr; }
            if(c_InteractionManager) { env->DeleteGlobalRef(c_InteractionManager); c_InteractionManager=nullptr; }
            if(c_EntityHitResult) { env->DeleteGlobalRef(c_EntityHitResult); c_EntityHitResult=nullptr; }
            if(c_BlockHitResult) { env->DeleteGlobalRef(c_BlockHitResult); c_BlockHitResult=nullptr; }
            if(c_EndCrystalEntity) { env->DeleteGlobalRef(c_EndCrystalEntity); c_EndCrystalEntity=nullptr; }
            if(c_PlayerInventory) { env->DeleteGlobalRef(c_PlayerInventory); c_PlayerInventory=nullptr; }
            if(c_Items) { env->DeleteGlobalRef(c_Items); c_Items=nullptr; }
            if(c_BlockPos) { env->DeleteGlobalRef(c_BlockPos); c_BlockPos=nullptr; }
            if(c_BlockState) { env->DeleteGlobalRef(c_BlockState); c_BlockState=nullptr; }
            if(c_Block) { env->DeleteGlobalRef(c_Block); c_Block=nullptr; }
            if(c_Blocks) { env->DeleteGlobalRef(c_Blocks); c_Blocks=nullptr; }
            if(c_ClientWorld) { env->DeleteGlobalRef(c_ClientWorld); c_ClientWorld=nullptr; }
            if(c_Hand) { env->DeleteGlobalRef(c_Hand); c_Hand=nullptr; }
            if(c_ActionResult) { env->DeleteGlobalRef(c_ActionResult); c_ActionResult=nullptr; }
            if(c_Window) { env->DeleteGlobalRef(c_Window); c_Window=nullptr; }
            m_ids_cached=false;
            p_jvm->DetachCurrentThread();
        }};
        JniSafety::LocalFrame setupFrame(env,256);
        if(!setupFrame) return;
        JniSafety::Lookup api(env);
        if (!CacheIDs(env))
            return;

        auto SafeGetField = [&](jclass cls, const char* name, const char* sig) -> jfieldID {
            jfieldID id = api.GetFieldID(cls, name, sig);
            if (!id) {
                printf("[-] FieldID not found: %s %s\n", name, sig);
                env->ExceptionClear( ); // ← CRITICAL, sonst crash beim nächsten JNI call
            }
            return id;
        };

        auto SafeGetMethod = [&](jclass cls, const char* name, const char* sig) -> jmethodID {
            jmethodID id = api.GetMethodID(cls, name, sig);
            if (!id) {
                printf("[-] MethodID not found: %s %s\n", name, sig);
                env->ExceptionClear( ); // ← CRITICAL
            }
            return id;
        };

        auto SafeGetStaticField = [&](jclass cls, const char* name, const char* sig) -> jfieldID {
            jfieldID id = api.GetStaticFieldID(cls, name, sig);
            if (!id) {
                printf("[-] Static FieldID not found: %s %s\n", name, sig);
                env->ExceptionClear( );
            }
            return id;
        };

        // ── Alle benötigten Field/Method IDs ──
        // Minecraft fields
        jfieldID f_player = SafeGetField(c_Minecraft, "player",
                                            "Lnet/minecraft/client/player/LocalPlayer;");
        jfieldID f_level = SafeGetField(c_Minecraft, "level",
                                           "Lnet/minecraft/client/multiplayer/ClientLevel;");
        jfieldID f_gameMode = SafeGetField(c_Minecraft, "gameMode",
                                              "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
        jfieldID f_hitResult = SafeGetField(c_Minecraft, "hitResult",
                                               "Lnet/minecraft/world/phys/HitResult;");
        jfieldID f_window = SafeGetField(c_Minecraft, "window",
                                            "Lcom/mojang/blaze3d/platform/Window;");

        // Window
        jmethodID m_getHandle = SafeGetMethod(c_Window, "handle", "()J");

        // Player
        jfieldID f_inventory = SafeGetField(c_ClientPlayerEntity, "inventory",
                                               "Lnet/minecraft/world/entity/player/Inventory;");
        jmethodID m_swingHand = SafeGetMethod(c_ClientPlayerEntity, "swing",
                                                 "(Lnet/minecraft/world/InteractionHand;)V");

        // Inventory
        jmethodID m_hasItem = SafeGetMethod(c_PlayerInventory, "hasAnyMatching",
                                               "(Ljava/util/function/Predicate;)Z");
        // Einfachere Alternative: direkt nach END_CRYSTAL-Item suchen
        // → wir nutzen contains() via Reflection oder direkte Slot-Iteration

        // Items.END_CRYSTAL (static field)
        jfieldID f_endCrystalItem = SafeGetStaticField(c_Items, "END_CRYSTAL",
                                                          "Lnet/minecraft/world/item/Item;");
        jobject END_CRYSTAL_ITEM = api.GetStaticObjectField(c_Items, f_endCrystalItem);

        // InteractionManager
        jmethodID m_attackEntity = SafeGetMethod(c_InteractionManager, "attack",
                                                    "(Lnet/minecraft/world/entity/player/Player;Lnet/minecraft/world/entity/Entity;)V");
        jmethodID m_interactBlock = SafeGetMethod(c_InteractionManager, "useItemOn",
                                                  "(Lnet/minecraft/client/player/LocalPlayer;"
                                                  "Lnet/minecraft/world/InteractionHand;"
                                                  "Lnet/minecraft/world/phys/BlockHitResult;)"
                                                  "Lnet/minecraft/world/InteractionResult;");

        // EntityHitResult
        jmethodID m_getEntity = SafeGetMethod(c_EntityHitResult, "getEntity",
                                                 "()Lnet/minecraft/world/entity/Entity;");

        // BlockHitResult
        jmethodID m_getBlockPos = SafeGetMethod(c_BlockHitResult, "getBlockPos",
                                                   "()Lnet/minecraft/core/BlockPos;");

        // BlockPos
        jmethodID m_blockPosDown = SafeGetMethod(c_BlockPos, "below",
                                                    "()Lnet/minecraft/core/BlockPos;");

        // ClientWorld
        jmethodID m_getBlockState = SafeGetMethod(c_ClientWorld, "getBlockState",
                                                     "(Lnet/minecraft/core/BlockPos;)Lnet/minecraft/world/level/block/state/BlockState;");

        // BlockState
        jmethodID m_getBlock = SafeGetMethod(c_BlockState, "getBlock",
                                                "()Lnet/minecraft/world/level/block/Block;");

        // Blocks.OBSIDIAN / BEDROCK (static fields)
        jfieldID f_obsidian = SafeGetStaticField(c_Blocks, "OBSIDIAN",
                                                    "Lnet/minecraft/world/level/block/Block;");
        jfieldID f_bedrock = SafeGetStaticField(c_Blocks, "BEDROCK",
                                                   "Lnet/minecraft/world/level/block/Block;");
        jobject OBSIDIAN = api.GetStaticObjectField(c_Blocks, f_obsidian);
        jobject BEDROCK = api.GetStaticObjectField(c_Blocks, f_bedrock);

        // Hand.MAIN_HAND (static field)
        jfieldID f_mainHand = api.GetStaticFieldID(c_Hand, "MAIN_HAND",
                                                    "Lnet/minecraft/world/InteractionHand;");
        jobject MAIN_HAND = api.GetStaticObjectField(c_Hand, f_mainHand);

        // ActionResult
        jmethodID m_isAccepted = SafeGetMethod(c_ActionResult, "consumesAction", "()Z");
        jmethodID m_swingSide = SafeGetMethod(c_ActionResult, "shouldSwing", "()Z");

    //    printf("[+] CwCrystal: All method/field IDs resolved. Starting tick loop.\n");

        // ── Inventory-Hilfsfunktion: hat der Spieler END_CRYSTAL? ──
        // Wir iterieren über die 36 Hauptslots der Inventory-Klasse
        jfieldID f_items = SafeGetField(c_PlayerInventory, "items",
                                           "Lnet/minecraft/core/NonNullList;");
        jclass c_NonNullList = api.FindClass("net/minecraft/core/NonNullList");
        jmethodID m_listGet = SafeGetMethod(c_NonNullList, "get", "(I)Ljava/lang/Object;");
        jmethodID m_listSize = SafeGetMethod(c_NonNullList, "size", "()I");

        jclass c_ItemStack = api.FindClass("net/minecraft/world/item/ItemStack");
        jmethodID m_getItem = SafeGetMethod(c_ItemStack, "getItem",
                                               "()Lnet/minecraft/world/item/Item;");


        if (!c_EntityHitResult || !c_EndCrystalEntity || !f_player || !f_level || !f_gameMode || !f_hitResult || !f_inventory || !f_items || !m_listSize || !m_listGet || !m_getItem || !END_CRYSTAL_ITEM || !m_getEntity || !m_attackEntity || !m_swingHand || !MAIN_HAND) {
            printf("[-] CwCrystal: Critical field IDs missing, aborting!\n");

            return;
        }

        printf("[+] CwCrystal: Entering tick loop\n");

        while (!Lifecycle::requested) {
            Sleep(5);
            std::lock_guard lock(m_tickMutex);
            if (!m_running || Lifecycle::requested) continue;
            JniSafety::LocalFrame iteration(env);
            if(!iteration) break;
            JniSafety::ScopeExit clearException{[env] { if(env->ExceptionCheck()) env->ExceptionClear(); }};
        //    Sleep(5); // ~20 ticks/s

            jobject mc = p_mc->GetInstance( ); // Getter für class_instance hinzufügen!
            if (!mc)
                continue;

            // ── player & world null-check ──
            jobject player = api.GetObjectField(mc, f_player);
            if (!player)
                continue;

            jobject world = api.GetObjectField(mc, f_level);
            if (!world) {
                env->DeleteLocalRef(player);
                continue;
            }

            // GLFW direkt aus nativem Context aufrufen
            // (funktioniert da wir im gleichen Prozess sind)
            // RMB = Button 1
            if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) == 0) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            // ── Hat Spieler END_CRYSTAL? ──
            jobject inventory = api.GetObjectField(player, f_inventory);
            jobject itemsList = api.GetObjectField(inventory, f_items);
            bool hasEndCrystal = false;
            if (!itemsList) continue;
            jint slotCount = env->CallIntMethod(itemsList, m_listSize);
            if (env->ExceptionCheck()) continue;

            for (jint i = 0; i < slotCount && !hasEndCrystal; i++) {
                jobject stack = api.CallObjectMethod(itemsList, m_listGet, i);
                if (!stack)
                    continue;
                jobject item = api.CallObjectMethod(stack, m_getItem);
                if (env->IsSameObject(item, END_CRYSTAL_ITEM))
                    hasEndCrystal = true;
                env->DeleteLocalRef(item);
                env->DeleteLocalRef(stack);
            }
            env->DeleteLocalRef(itemsList);
            env->DeleteLocalRef(inventory);

            if (!hasEndCrystal) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            // ── crosshairTarget / hitResult ──
            jobject hitResult = api.GetObjectField(mc, f_hitResult);
            if (!hitResult) {
                env->DeleteLocalRef(player);
                env->DeleteLocalRef(world);
                continue;
            }

            jobject gameMode = api.GetObjectField(mc, f_gameMode);

            // ── Case 1: EntityHitResult ──
            if (env->IsInstanceOf(hitResult, c_EntityHitResult)) {
                jobject entity = api.CallObjectMethod(hitResult, m_getEntity);
                if (entity && env->IsInstanceOf(entity, c_EndCrystalEntity)) {
                    if (gameMode)
                        env->CallVoidMethod(gameMode, m_attackEntity, player, entity);
                    if (!env->ExceptionCheck())
                        env->CallVoidMethod(player, m_swingHand, MAIN_HAND);
                }
                if (entity)
                    env->DeleteLocalRef(entity);
            }
            // ── Case 2: BlockHitResult ──
//            else if (env->IsInstanceOf(hitResult, c_BlockHitResult)) {
//                jobject blockPos = api.CallObjectMethod(hitResult, m_getBlockPos);
//                jobject blockPosDown = api.CallObjectMethod(blockPos, m_blockPosDown);
//
//                jobject stateBelow = api.CallObjectMethod(world, m_getBlockState, blockPosDown);
//                jobject stateTarget = api.CallObjectMethod(world, m_getBlockState, blockPos);
//
//                jobject blockBelow = api.CallObjectMethod(stateBelow, m_getBlock);
//                jobject blockTarget = api.CallObjectMethod(stateTarget, m_getBlock);
//
//                bool belowIsObsidian = env->IsSameObject(blockBelow, OBSIDIAN);
//                bool targetIsValid = env->IsSameObject(blockTarget, OBSIDIAN) || env->IsSameObject(blockTarget, BEDROCK);
//
//                if (belowIsObsidian && targetIsValid && gameMode) {
//                    jobject result = api.CallObjectMethod(
//                        gameMode, m_interactBlock, player, MAIN_HAND, hitResult);
//                    if (result) {
//                        bool accepted = env->CallBooleanMethod(result, m_isAccepted);
//                        if (accepted) // ← shouldSwing weg, nur noch accepted prüfen
//                            if (!env->ExceptionCheck())
                        env->CallVoidMethod(player, m_swingHand, MAIN_HAND);
//                       env->DeleteLocalRef(result);
//                    }
//                }
//
//                env->DeleteLocalRef(blockTarget);
//                env->DeleteLocalRef(blockBelow);
//                env->DeleteLocalRef(stateTarget);
//                env->DeleteLocalRef(stateBelow);
//                env->DeleteLocalRef(blockPosDown);
//                env->DeleteLocalRef(blockPos);
//            }

            // ── Cleanup ──
            if (gameMode)
                env->DeleteLocalRef(gameMode);
            env->DeleteLocalRef(hitResult);
            env->DeleteLocalRef(world);
            env->DeleteLocalRef(player);
        }


    }
