#include "cw.hpp"
#include "CwCrystal.hpp"
#include "../settings.hpp"
#include "../../utils/sdk/java.hpp"

CW::CW() : RuntimeModule("CW", "Combat", CW_Enabled) {}
// ClientWorker joins Lifecycle workers before clearing the module registry.
CW::~CW() = default;

void CW::Tick() {
    if (CW_Enabled) {

        if (!g_crystal) {

            p_jni->p_cminecraft->SetRightClickDelay(0);
            g_crystal = std::make_unique<CwCrystal>(p_jni->GetJVM( ), p_jni->p_cminecraft.get( ));

        }
        g_crystal->Start();
    } else if (g_crystal) {
        g_crystal->Stop();
    }
}
