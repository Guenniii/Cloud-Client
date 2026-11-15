#pragma once
#include <string>
#include "../dependencies/imgui/imgui.h"

// Legacy symbols referenced by the customized ImGui widgets; defined once in resources.cpp.
extern ImFont* tab_title;
extern ImFont* font_icon;
extern ImFont* poppins;
extern ImFont* icons;
// Orbit menu fonts (Space Grotesk Medium/Bold, JetBrains Mono Regular/Bold); may be null if loading failed.
struct OrbitFace {
    static constexpr int MAX = 12;
    ImFont* font[MAX] = {};
    float size[MAX] = {};
    int count = 0; // 0 = failed to load
};
extern OrbitFace orbit_text;
extern OrbitFace orbit_text_bold;
extern OrbitFace orbit_mono;
extern OrbitFace orbit_mono_bold;

namespace Menu::Resources {
    void SetupStyleAndFonts();
    ImTextureID LogoTexture();
    ImTextureID UserTexture();
    void RequestUserAvatar(const std::string& username);
    void UpdateAnimation(float deltaTime);
    void ReleaseTextures(bool vulkanTextures);
}
