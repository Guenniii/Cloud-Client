#pragma once

#include <Windows.h>
#include <string>

namespace Menu {
    void InitializeContext(HWND hwnd);
    void Render( );
    void Shutdown(bool vulkanTextures);
    void Images( );
    inline bool bShowMenu = true;
} // namespace Menu
