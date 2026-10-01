#pragma once
#include "../../dependencies/jni/jni.h"
#include <array>
#include <chrono>

class CMinecraft;

class Reach final {
public:

    // Liest den aktuellen Wert aus, bevor er verändert wird, und cached ihn.
    bool CaptureOriginal( );

    // Setzt exakt den Wert zurück, der vor der ersten Änderung galt.
    bool RestoreDefault( );

    Reach(JavaVM* jvm, CMinecraft* mc);

    bool Init();

    bool SetReach(int value);

private:

    // Interne Variante ohne das value<3-Clamping, damit auch 4.5 möglich ist.
    bool SetReachRaw(float value);

    jclass FindGlobal(JNIEnv* env, const char* name);

    jobject GetGameModeInstance(JNIEnv* env) const;

    void CacheReachFieldIDs(JNIEnv* env);

    int SetReachByReflection(JNIEnv* env, jobject game_mode, float value);

    int SetReachByPlayerAttributes(JNIEnv* env, float value);

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

    bool m_original_captured = false;
    float m_original_value = 4.5f;

};
