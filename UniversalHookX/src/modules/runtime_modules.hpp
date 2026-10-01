#pragma once
#include "ModuleBase.hpp"
#include <memory>

namespace RuntimeModules {
    std::unique_ptr<ModuleBase> CreateAutoArmor();
    std::unique_ptr<ModuleBase> CreateRefill();
    std::unique_ptr<ModuleBase> CreateHitEffect();
    std::unique_ptr<ModuleBase> CreatePredictDoubleHand();
    std::unique_ptr<ModuleBase> CreateShieldBreaker();
    std::unique_ptr<ModuleBase> CreateFastPlace();
    std::unique_ptr<ModuleBase> CreateReach();
    std::unique_ptr<ModuleBase> CreateAutoTotem();
    std::unique_ptr<ModuleBase> CreateNoJumpDelay();
    std::unique_ptr<ModuleBase> CreateAutoSprint();
    std::unique_ptr<ModuleBase> CreateSafeWalk();
    std::unique_ptr<ModuleBase> CreateShortWindCharge();
    std::unique_ptr<ModuleBase> CreateAutoPearlCatch();
    std::unique_ptr<ModuleBase> CreateHitCrystal();
    std::unique_ptr<ModuleBase> CreateAimAssist();
    std::unique_ptr<ModuleBase> CreateSilentAim();
    std::unique_ptr<ModuleBase> CreateAutoMace();
    std::unique_ptr<ModuleBase> CreateBreachSwap();
    std::unique_ptr<ModuleBase> CreateFakeLag();
    std::unique_ptr<ModuleBase> CreateAutoTool();
    std::unique_ptr<ModuleBase> CreateSafeAnchor();
    std::unique_ptr<ModuleBase> CreateSeedCracker();
}
