#pragma once

#include "../../dependencies/jni/jni.h"
#include "../../utils/sdk/CMinecraft.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <chrono>

class Reach final {
public:
    Reach(JavaVM* jvm, CMinecraft* mc) : p_jvm(jvm), p_mc(mc) {}

    bool Init() {
        if (!p_jvm || !p_mc || !p_mc->IsInitialized()) {
            return false;
        }

        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env) {
            return false;
        }

        if (m_initialized) {
            return true;
        }

        c_minecraft = FindGlobal(env, "net/minecraft/client/Minecraft");
        c_game_mode = FindGlobal(env, "net/minecraft/client/multiplayer/MultiPlayerGameMode");

        if (!c_minecraft || !c_game_mode) {
            std::printf("[-] Reach: class lookup failed\n");
            return false;
        }

        f_game_mode = env->GetFieldID(
            c_minecraft,
            "gameMode",
            "Lnet/minecraft/client/multiplayer/MultiPlayerGameMode;");
        if (!f_game_mode) {
            env->ExceptionClear();
            std::printf("[-] Reach: Minecraft.gameMode field not found\n");
            return false;
        }

        f_player = env->GetFieldID(
            c_minecraft,
            "player",
            "Lnet/minecraft/client/player/LocalPlayer;");
        if (!f_player) {
            env->ExceptionClear();
        }

        CacheReachFieldIDs(env);
        m_initialized = true;
        std::printf("[+] Reach: initialized\n");
        return true;
    }

    bool SetReach(int value) {
        if (value < 3) {
            value = 3;
        }

        if (!Init()) {
            return false;
        }

        JNIEnv* env = nullptr;
        if (p_jvm->AttachCurrentThread((void**)&env, nullptr) != JNI_OK || !env) {
            return false;
        }

        jobject game_mode = GetGameModeInstance(env);
        if (!game_mode) {
            return false;
        }

        bool wrote_any = false;
        for (const auto& entry : m_reach_float_fields) {
            if (entry) {
                env->SetFloatField(game_mode, entry, value);
                wrote_any = true;
            }
        }

        for (const auto& entry : m_reach_double_fields) {
            if (entry) {
                env->SetDoubleField(game_mode, entry, static_cast<jdouble>(value));
                wrote_any = true;
            }
        }

        // Fallback for versions/mappings where field names differ.
        const int reflected_writes = SetReachByReflection(env, game_mode, value);
        if (reflected_writes > 0) {
            wrote_any = true;
        }

        const int attribute_writes = SetReachByPlayerAttributes(env, value);
        if (attribute_writes > 0) {
            wrote_any = true;
        }

        if (!wrote_any) {
            const auto now = std::chrono::steady_clock::now();
            if (now - m_last_fail_log > std::chrono::seconds(2)) {
                std::printf("[-] Reach: no writable reach fields found\n");
                m_last_fail_log = now;
            }
        }

        env->DeleteLocalRef(game_mode);
        return wrote_any;
    }

private:
    jclass FindGlobal(JNIEnv* env, const char* name) {
        jclass local = env->FindClass(name);
        if (!local) {
            env->ExceptionClear();
            return nullptr;
        }
        jclass global = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        return global;
    }

    jobject GetGameModeInstance(JNIEnv* env) const {
        jobject mc = p_mc->GetInstance();
        if (!mc || !f_game_mode) {
            return nullptr;
        }

        jobject game_mode = env->GetObjectField(mc, f_game_mode);
        if (!game_mode && env->ExceptionCheck()) {
            env->ExceptionClear();
        }
        return game_mode;
    }

    void CacheReachFieldIDs(JNIEnv* env) {
        // Different mappings/versions can use different names.
        constexpr std::array<const char*, 4> kFieldCandidates = {
            "blockReach",
            "entityReach",
            "pickRange",
            "attackRange"
        };

        std::size_t float_idx = 0;
        std::size_t double_idx = 0;
        for (const char* field_name : kFieldCandidates) {
            jfieldID f_id = env->GetFieldID(c_game_mode, field_name, "F");
            if (f_id) {
                if (float_idx < m_reach_float_fields.size()) {
                    m_reach_float_fields[float_idx++] = f_id;
                }
                continue;
            }

            env->ExceptionClear();
            jfieldID d_id = env->GetFieldID(c_game_mode, field_name, "D");
            if (d_id) {
                if (double_idx < m_reach_double_fields.size()) {
                    m_reach_double_fields[double_idx++] = d_id;
                }
            } else {
                env->ExceptionClear();
            }
        }
    }

    int SetReachByReflection(JNIEnv* env, jobject game_mode, float value) {
        jclass cls_class = env->FindClass("java/lang/Class");
        jclass cls_field = env->FindClass("java/lang/reflect/Field");

        if (!cls_class || !cls_field) {
            env->ExceptionClear();
            return 0;
        }

        jmethodID m_get_declared_fields = env->GetMethodID(cls_class, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
        jmethodID m_set_accessible = env->GetMethodID(cls_field, "setAccessible", "(Z)V");
        jmethodID m_get_name = env->GetMethodID(cls_field, "getName", "()Ljava/lang/String;");
        jmethodID m_get_type = env->GetMethodID(cls_field, "getType", "()Ljava/lang/Class;");
        jmethodID m_get_float = env->GetMethodID(cls_field, "getFloat", "(Ljava/lang/Object;)F");
        jmethodID m_set_float = env->GetMethodID(cls_field, "setFloat", "(Ljava/lang/Object;F)V");
        jmethodID m_get_double = env->GetMethodID(cls_field, "getDouble", "(Ljava/lang/Object;)D");
        jmethodID m_set_double = env->GetMethodID(cls_field, "setDouble", "(Ljava/lang/Object;D)V");

        if (!m_get_declared_fields || !m_set_accessible || !m_get_name || !m_get_type ||
            !m_get_float || !m_set_float || !m_get_double || !m_set_double) {
            env->ExceptionClear();
            return 0;
        }

        jclass game_mode_class = env->GetObjectClass(game_mode);
        if (!game_mode_class) {
            return 0;
        }

        jobjectArray fields = (jobjectArray)env->CallObjectMethod(game_mode_class, m_get_declared_fields);
        env->DeleteLocalRef(game_mode_class);
        if (!fields || env->ExceptionCheck()) {
            env->ExceptionClear();
            return 0;
        }

        int writes = 0;
        const jint count = env->GetArrayLength(fields);
        for (jint i = 0; i < count; ++i) {
            jobject field_obj = env->GetObjectArrayElement(fields, i);
            if (!field_obj) {
                continue;
            }

            env->CallVoidMethod(field_obj, m_set_accessible, JNI_TRUE);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
                env->DeleteLocalRef(field_obj);
                continue;
            }

            jstring field_name_j = (jstring)env->CallObjectMethod(field_obj, m_get_name);
            const char* field_name = field_name_j ? env->GetStringUTFChars(field_name_j, nullptr) : "";

            // Prefer reach/range names, but allow obfuscated names as fallback.
            const bool reach_name = field_name &&
                (std::strstr(field_name, "reach") || std::strstr(field_name, "Reach") ||
                 std::strstr(field_name, "range") || std::strstr(field_name, "Range"));
            const bool obfuscated_name = field_name &&
                (std::strstr(field_name, "f_") == field_name || std::strstr(field_name, "field_") == field_name);
            const bool name_candidate = reach_name || obfuscated_name;

            if (name_candidate) {
                jobject type_obj = env->CallObjectMethod(field_obj, m_get_type);
                jclass type_class = type_obj ? env->GetObjectClass(type_obj) : nullptr;
                jmethodID m_get_type_name = type_class ? env->GetMethodID(type_class, "getName", "()Ljava/lang/String;") : nullptr;
                jstring type_name_j = (type_obj && m_get_type_name) ? (jstring)env->CallObjectMethod(type_obj, m_get_type_name) : nullptr;
                const char* type_name = type_name_j ? env->GetStringUTFChars(type_name_j, nullptr) : "";

                if (type_name && std::strcmp(type_name, "float") == 0) {
                    jfloat current = env->CallFloatMethod(field_obj, m_get_float, game_mode);
                    if (!env->ExceptionCheck() && current >= 2.5f && current <= 7.0f) {
                        env->CallVoidMethod(field_obj, m_set_float, game_mode, value);
                        if (!env->ExceptionCheck()) {
                            ++writes;
                        } else {
                            env->ExceptionClear();
                        }
                    } else {
                        env->ExceptionClear();
                    }
                } else if (type_name && std::strcmp(type_name, "double") == 0) {
                    jdouble current = env->CallDoubleMethod(field_obj, m_get_double, game_mode);
                    if (!env->ExceptionCheck() && current >= 2.5 && current <= 7.0) {
                        env->CallVoidMethod(field_obj, m_set_double, game_mode, static_cast<jdouble>(value));
                        if (!env->ExceptionCheck()) {
                            ++writes;
                        } else {
                            env->ExceptionClear();
                        }
                    } else {
                        env->ExceptionClear();
                    }
                }

                if (type_name_j) {
                    env->ReleaseStringUTFChars(type_name_j, type_name);
                    env->DeleteLocalRef(type_name_j);
                }
                if (type_class) {
                    env->DeleteLocalRef(type_class);
                }
                if (type_obj) {
                    env->DeleteLocalRef(type_obj);
                }
            }

            if (field_name_j) {
                env->ReleaseStringUTFChars(field_name_j, field_name);
                env->DeleteLocalRef(field_name_j);
            }
            env->DeleteLocalRef(field_obj);
        }

        env->DeleteLocalRef(fields);
        if (writes > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (now - m_last_success_log > std::chrono::seconds(1)) {
                std::printf("[+] Reach: reflection writes=%d\n", writes);
                m_last_success_log = now;
            }
        }
        return writes;
    }

    int SetReachByPlayerAttributes(JNIEnv* env, float value) {
        if (!f_player) {
            return 0;
        }

        jobject mc = p_mc->GetInstance();
        if (!mc) {
            return 0;
        }

        jobject player = env->GetObjectField(mc, f_player);
        if (!player || env->ExceptionCheck()) {
            env->ExceptionClear();
            return 0;
        }

        jclass c_attributes = env->FindClass("net/minecraft/world/entity/ai/attributes/Attributes");
        jclass c_attribute_instance = env->FindClass("net/minecraft/world/entity/ai/attributes/AttributeInstance");
        if (!c_attributes || !c_attribute_instance) {
            env->ExceptionClear();
            env->DeleteLocalRef(player);
            return 0;
        }

        // Since your runtime has readable names, these are usually stable for 1.20.5+ style builds.
        jfieldID f_block_range = env->GetStaticFieldID(c_attributes, "BLOCK_INTERACTION_RANGE", "Lnet/minecraft/core/Holder;");
        if (!f_block_range) {
            env->ExceptionClear();
            f_block_range = env->GetStaticFieldID(c_attributes, "blockInteractionRange", "Lnet/minecraft/core/Holder;");
            if (!f_block_range) {
                env->ExceptionClear();
            }
        }

        jfieldID f_entity_range = env->GetStaticFieldID(c_attributes, "ENTITY_INTERACTION_RANGE", "Lnet/minecraft/core/Holder;");
        if (!f_entity_range) {
            env->ExceptionClear();
            f_entity_range = env->GetStaticFieldID(c_attributes, "entityInteractionRange", "Lnet/minecraft/core/Holder;");
            if (!f_entity_range) {
                env->ExceptionClear();
            }
        }

        if (!f_block_range && !f_entity_range) {
            env->DeleteLocalRef(player);
            return 0;
        }

        jclass c_player = env->GetObjectClass(player);
        jmethodID m_get_attribute = env->GetMethodID(
            c_player,
            "getAttribute",
            "(Lnet/minecraft/core/Holder;)Lnet/minecraft/world/entity/ai/attributes/AttributeInstance;");
        if (!m_get_attribute) {
            env->ExceptionClear();
            env->DeleteLocalRef(c_player);
            env->DeleteLocalRef(player);
            return 0;
        }

        jmethodID m_set_base = env->GetMethodID(c_attribute_instance, "setBaseValue", "(D)V");
        if (!m_set_base) {
            env->ExceptionClear();
            env->DeleteLocalRef(c_player);
            env->DeleteLocalRef(player);
            return 0;
        }

        int writes = 0;
        if (f_block_range) {
            jobject block_holder = env->GetStaticObjectField(c_attributes, f_block_range);
            jobject block_attr = block_holder ? env->CallObjectMethod(player, m_get_attribute, block_holder) : nullptr;
            if (!env->ExceptionCheck() && block_attr) {
                env->CallVoidMethod(block_attr, m_set_base, static_cast<jdouble>(value));
                if (!env->ExceptionCheck()) {
                    ++writes;
                } else {
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(block_attr);
            } else {
                env->ExceptionClear();
            }
            if (block_holder) {
                env->DeleteLocalRef(block_holder);
            }
        }

        if (f_entity_range) {
            jobject entity_holder = env->GetStaticObjectField(c_attributes, f_entity_range);
            jobject entity_attr = entity_holder ? env->CallObjectMethod(player, m_get_attribute, entity_holder) : nullptr;
            if (!env->ExceptionCheck() && entity_attr) {
                env->CallVoidMethod(entity_attr, m_set_base, static_cast<jdouble>(value));
                if (!env->ExceptionCheck()) {
                    ++writes;
                } else {
                    env->ExceptionClear();
                }
                env->DeleteLocalRef(entity_attr);
            } else {
                env->ExceptionClear();
            }
            if (entity_holder) {
                env->DeleteLocalRef(entity_holder);
            }
        }

        env->DeleteLocalRef(c_player);
        env->DeleteLocalRef(player);

        if (writes > 0) {
            const auto now = std::chrono::steady_clock::now();
            if (now - m_last_success_log > std::chrono::seconds(1)) {
                std::printf("[+] Reach: attribute writes=%d\n", writes);
                m_last_success_log = now;
            }
        }
        return writes;
    }

private:
    JavaVM* p_jvm = nullptr;
    CMinecraft* p_mc = nullptr;

    bool m_initialized = false;

    jclass c_minecraft = nullptr;
    jclass c_game_mode = nullptr;
    jfieldID f_game_mode = nullptr;
    jfieldID f_player = nullptr;

    std::array<jfieldID, 4> m_reach_float_fields = {nullptr, nullptr, nullptr, nullptr};
    std::array<jfieldID, 4> m_reach_double_fields = {nullptr, nullptr, nullptr, nullptr};
    std::chrono::steady_clock::time_point m_last_fail_log{};
    std::chrono::steady_clock::time_point m_last_success_log{};
};
