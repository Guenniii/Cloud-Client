#pragma once
#include <span>
#include "../utils/config/config.hpp"

namespace Menu::Registry {
    // The registry owns the stable backing storage; rendering and config share the same entries.
    std::span<ModuleData> GetModules();
    void ProcessHotkeys(bool blocked);
    void CaptureHotkey();
    bool HasConflict(int key, int exceptModule = -1);
}
