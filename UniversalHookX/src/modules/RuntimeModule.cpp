#include "RuntimeModule.hpp"
#include "../utils/sdk/java.hpp"

void RuntimeModule::Update() {
    // Preserve the original SDK guard, including pending disable transitions.
    if (!p_jni || !p_jni->p_cminecraft) return;
    const bool enabled = IsEnabled();
    if (enabled || wasEnabled_) Tick();
    wasEnabled_ = enabled;
}
