#pragma once
#include "../dependencies/imgui/imgui.h"

// Legacy symbols referenced by the customized ImGui widgets; defined once in resources.cpp.
extern ImFont* tab_title;
extern ImFont* font_icon;
extern ImFont* poppins;
extern ImFont* icons;

namespace Menu::Resources {
    void SetupStyleAndFonts();
    ImTextureID LogoTexture();
    ImTextureID UserTexture();
    void UpdateAnimation(float deltaTime);
    void ReleaseTextures(bool vulkanTextures);
}
