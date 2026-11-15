#pragma once
// Bridges the SeedCracker result to the ore simulation.
//
// The SeedCracker verifies world seeds against the hash of the current world.
// This module keeps the verified candidates available to the menu and applies
// the selection to OreSim, so a seed never has to be typed twice.
#include "seedcracker_bridge.hpp"
#include "../oresim/seed_settings.hpp"
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <mutex>
#include <vector>

namespace SeedHandoff {

// Applies the verified seed to OreSim as soon as the cracker reports one.
inline std::atomic<bool> AutoApply{false};

inline std::mutex mutex;
inline std::vector<long long> candidates;
inline long long appliedSeed = 0;
inline bool hasApplied = false;

// Reads the verified seeds for the current world. The Java side publishes them
// from the client thread, so this is safe to call from the module worker.
inline void Poll(JNIEnv* env) {
    if (!env) return;
    auto verified = SeedCracker::GetVerifiedSeeds(env);
    std::lock_guard<std::mutex> lock(mutex);
    if (verified == candidates) return;
    candidates = std::move(verified);
    // A new world produces a new candidate set. Drop the previous selection
    // so the seed of the old world is never reused for the new one.
    if (hasApplied) {
        const bool still_valid = std::find(candidates.begin(), candidates.end(), appliedSeed) != candidates.end();
        if (!still_valid) {
            appliedSeed = 0;
            hasApplied = false;
        }
    }
    if (!candidates.empty()) {
        std::printf("[SeedHandoff] %zu verifizierte Seed-Kandidaten verfuegbar.\n",
                    candidates.size());
    }
}

inline std::vector<long long> Candidates() {
    std::lock_guard<std::mutex> lock(mutex);
    return candidates;
}

// Applies a seed to the ore simulation.
inline void Apply(long long seed) {
    OreSim::SetCrackerSeed(seed);
    std::lock_guard<std::mutex> lock(mutex);
    appliedSeed = seed;
    hasApplied = true;
    std::printf("[SeedHandoff] Seed an OreSim uebergeben: %lld\n", seed);
}

inline bool HasApplied() {
    std::lock_guard<std::mutex> lock(mutex);
    return hasApplied;
}

inline long long AppliedSeed() {
    std::lock_guard<std::mutex> lock(mutex);
    return appliedSeed;
}

inline void Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    candidates.clear();
    appliedSeed = 0;
    hasApplied = false;
}

} // namespace SeedHandoff
