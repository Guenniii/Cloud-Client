#include "ModuleManager.hpp"
#include "runtime_modules.hpp"
#include "../console/console.hpp"
#include "combat/cw.hpp"
#include "../utils/SilentAim.hpp"

void ModuleManager::Init() {
    if (!m_modules.empty()) return;
    // Keep the previous update ordering for features which share Minecraft state.
    m_modules.push_back(RuntimeModules::CreateFastPlace());
    m_modules.push_back(std::make_unique<CW>());
    m_modules.push_back(RuntimeModules::CreateReach());
    m_modules.push_back(RuntimeModules::CreateAutoTotem());
    m_modules.push_back(RuntimeModules::CreateNoJumpDelay());
    m_modules.push_back(RuntimeModules::CreateAutoSprint());
    m_modules.push_back(RuntimeModules::CreateSafeWalk());
    m_modules.push_back(RuntimeModules::CreateShortWindCharge());
    m_modules.push_back(RuntimeModules::CreateAutoPearlCatch());
    m_modules.push_back(RuntimeModules::CreateHitCrystal());
    m_modules.push_back(RuntimeModules::CreateAutoMace());
    m_modules.push_back(RuntimeModules::CreateBreachSwap());
    m_modules.push_back(RuntimeModules::CreateSafeAnchor());
    m_modules.push_back(RuntimeModules::CreateFakeLag());
    m_modules.push_back(RuntimeModules::CreateAutoTool());
    m_modules.push_back(RuntimeModules::CreateAutoArmor());
    m_modules.push_back(RuntimeModules::CreateRefill());
    m_modules.push_back(RuntimeModules::CreateHitEffect());
    m_modules.push_back(RuntimeModules::CreatePredictDoubleHand());
    m_modules.push_back(RuntimeModules::CreateShieldBreaker());

    m_modules.push_back(RuntimeModules::CreateAimAssist());
    m_modules.push_back(RuntimeModules::CreateSilentAim());
    m_modules.push_back(RuntimeModules::CreateSeedCracker());
    LOG("[+] ModuleManager: %zu runtime modules initialized.\n", m_modules.size());
}

void ModuleManager::UpdateModules() {
    SilentAim::beginUpdate();
    // Each module dispatches its enabled state and pending disable transition.
    for (auto& module : m_modules) module->Update();
}
