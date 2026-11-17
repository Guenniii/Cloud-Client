#pragma once
#include <cmath>
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
// Loaded size of `face` closest to px (static atlas); falls back to the legacy font when Orbit fonts failed to load.
inline ImFont* NearestOrbitFont(const OrbitFace& face, float px, float* outSize) {
    int best = -1;
    for (int i = 0; i < face.count; ++i)
        if (face.font[i] && (best < 0 || std::abs(face.size[i] - px) < std::abs(face.size[best] - px))) best = i;
    if (outSize) *outSize = best >= 0 ? face.size[best] : ImGui::GetFontSize();
    return best >= 0 ? face.font[best] : (poppins ? poppins : ImGui::GetFont());
}

namespace Menu::Resources {
    void SetupStyleAndFonts();
    ImTextureID LogoTexture();
    ImTextureID UserTexture();
    bool HasUserAvatar(); // downloaded avatar uploaded; UserTexture() otherwise returns the default logo
    bool UserAvatarPending(); // download still running or downloaded but not yet uploaded by UserTexture()
    void RequestUserAvatar(const std::string& username);
    void UpdateAnimation(float deltaTime);
    void ReleaseTextures(bool vulkanTextures);
}
