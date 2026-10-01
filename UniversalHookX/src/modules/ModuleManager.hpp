#pragma once

#include <memory>
#include <vector>

#include "ModuleBase.hpp"

struct ModuleManager {
    static void Init( );

    static void UpdateModules( );
    static void RenderOverlay( );
    static void RenderHud( );

    static void RenderMenu(int index);

    static std::vector<std::string> GetCategories( );
    static std::vector<std::unique_ptr<ModuleBase>>& GetModules( ) { return m_modules; }
    static int GetFirstModuleIndexByCategory(const std::string& category);
private:
    static inline std::vector<std::unique_ptr<ModuleBase>> m_modules;
};
