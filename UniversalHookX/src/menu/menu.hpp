#pragma once

#include <Windows.h>
#include <string>

namespace Menu {
    void InitializeContext(HWND hwnd);
    void Render( );
    void Particles( ); // background particles; also login-window callback and used by ClickGui
    void Shutdown(bool vulkanTextures);
    void Images( );
    inline bool bShowMenu = true;
} // namespace Menu
