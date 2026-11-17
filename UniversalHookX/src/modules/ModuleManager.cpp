#include "ModuleManager.hpp"
#include "runtime_modules.hpp"
#include "../console/console.hpp"
#include "combat/cw.hpp"
#include "../utils/SilentAim.hpp"
#include "../utils/sdk/java.hpp"
#include <Windows.h>
#include <string>
#include <unordered_map>

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
    m_modules.push_back(RuntimeModules::CreateKeyPearl());
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
    m_modules.push_back(RuntimeModules::CreateTriggerBot());

    m_modules.push_back(RuntimeModules::CreateAimAssist());
    m_modules.push_back(RuntimeModules::CreateSilentAim());
    m_modules.push_back(RuntimeModules::CreateSeedCracker());
    LOG("[+] ModuleManager: %zu runtime modules initialized.\n", m_modules.size());
}

void ModuleManager::UpdateModules() {
    SilentAim::beginUpdate();
    // Each module dispatches its enabled state and pending disable transition.
    JNIEnv* env = p_jni ? p_jni->GetEnv() : nullptr;
    for (auto& module : m_modules) {
        module->Update();
        // A pending exception would make every later module bail out silently; contain it here.
        if (env && env->ExceptionCheck()) {
            // Per-source throttle: a constantly failing module must not hide another one.
            static std::unordered_map<std::string, ULONGLONG> lastLog;
            const ULONGLONG now = GetTickCount64();
            const std::string name = module->GetName();
            auto& last = lastLog[name];
            if (!last || now - last >= 5000) {
                last = now;
                LOG("[ModuleManager] Java exception left pending by %s; cleared.\n", name.c_str());
                env->ExceptionDescribe();
            }
            env->ExceptionClear();
        }
    }
}
