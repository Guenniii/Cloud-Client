#pragma once

#include "../../dependencies/jni/jni.h"
#include <string>
#include <vector>

namespace SeedCracker {

    struct ChestHit {
        int x, y, z;
    };

    struct ConfirmedShipwreck {
        int chestX, chestY, chestZ;
        int regionChunkX, regionChunkZ;
    };

    enum class CrackState {
        IDLE,
        COLLECTING,
        FAILED,
        MULTIPLE,
        FOUND,
        STRUCTURE,
        CANDIDATES
    };

    struct CrackingStatus {
        double baseBits = 0.0;
        double liftingBits = 0.0;
        double wantedBits = 0.0;
        int wrackCount = 0;
        CrackState state = CrackState::IDLE;
        bool hasSeed = false;
        long long seed = 0;
        int candidates = 0;
        int structureSeedCount = 0;
        int biomeCount = 0;
        std::vector<long long> candidateSeeds;
    };

    JNIEnv* GetMinecraftJNIEnv( );
    void Shutdown(JNIEnv* env);

    bool Init(JNIEnv* env, const std::string& bridgeJarPath);

    // Returns a local reference; call only after successful Init.
    jclass LoadExtensionClass(JNIEnv* env, const char* dottedName);

    // Queues initialization, tick/scan or reset on Minecraft's client thread.
    bool RequestClientUpdate(JNIEnv* env, bool enabled);

    bool InitCracking(JNIEnv* env);

    void ResetCracking(JNIEnv* env);

    void TickCracking(JNIEnv* env);

    void SetHashedSeed(JNIEnv* env, long long hashedSeed);

    void FinalizeWorldSeedCandidates(JNIEnv* env);

    CrackingStatus GetCrackingStatus(JNIEnv* env);

    std::vector<ChestHit> ScanPlayerChunkForShipwreckCandidates(JNIEnv* env);

    std::vector<ChestHit> ScanChunkForShipwreckCandidates(JNIEnv* env, int chunkX, int chunkZ);

    std::vector<ConfirmedShipwreck> ScanChunkForConfirmedShipwrecks(JNIEnv* env, int chunkX, int chunkZ);

    std::vector<ChestHit> ScanChunkForBuriedTreasure(JNIEnv* env, int chunkX, int chunkZ);

} // namespace SeedCracker
