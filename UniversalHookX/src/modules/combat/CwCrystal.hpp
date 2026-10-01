#pragma once
#include "../../dependencies/jni/jni.h"
#include <atomic>
#include <mutex>
#include <cstdint>

class CMinecraft;

class CwCrystal {
public:
    CwCrystal(JavaVM* jvm, CMinecraft* mc);

    void Start( );

    void Stop( );
private:
    JavaVM* p_jvm;
    CMinecraft* p_mc;
    std::mutex m_tickMutex;
    std::atomic<bool> m_running = false;
    std::atomic<bool> m_workerStarted = false;
    std::uint64_t m_nextStartAttempt = 0;

    // ── gecachte JNI-IDs ──────────────────────────────────────────
    jclass c_Minecraft = nullptr;
    jclass c_ClientPlayerEntity = nullptr;
    jclass c_InteractionManager = nullptr;
    jclass c_EntityHitResult = nullptr;
    jclass c_BlockHitResult = nullptr;
    jclass c_EndCrystalEntity = nullptr;
    jclass c_PlayerInventory = nullptr;
    jclass c_Items = nullptr;
    jclass c_BlockPos = nullptr;
    jclass c_BlockState = nullptr;
    jclass c_Block = nullptr;
    jclass c_Blocks = nullptr;
    jclass c_ClientWorld = nullptr;
    jclass c_Hand = nullptr;
    jclass c_ActionResult = nullptr;
    jclass c_Window = nullptr;

    bool m_ids_cached = false;

    // ── Hilfsfunktion: Klasse finden + GlobalRef ──────────────────
    jclass FindGlobal(JNIEnv* env, const char* name);

    // ── Alle IDs einmalig cachen ──────────────────────────────────
    bool CacheIDs(JNIEnv* env);

    // ── Haupt-Loop (entspricht dem ClientTickEvent) ───────────────
    void TickLoop( );
};
