#include "resources.hpp"
#include "menu.hpp"
#include <vulkan/vulkan.h>
#include "../utils/imageloader.hpp"
#include "../dependencies/font/IconsFontAwesome5.h"
// Embedded arrays have exactly one translation unit owner.
#include "../dependencies/font/fa-solid-900.h"
#include "../dependencies/images/bytearray.h"
#include "fonts/orbit_fonts.h"
#include "../dependencies/gifs/gif.h"
#include "../auth/Authclient.h"
#include "../auth/avatar_policy.hpp"
#include "../utils/lifecycle/lifecycle.hpp"
#include <future>
#include <memory>
#include <chrono>

// Fonts
ImFont* tab_title;
ImFont* font_icon;
ImFont* poppins;
ImFont* icons;
OrbitFace orbit_text;
OrbitFace orbit_text_bold;
OrbitFace orbit_mono;
OrbitFace orbit_mono_bold;

static MyTextureData logo;
static MyTextureData userlogo;
static MyTextureData profileAvatar;
static std::vector<unsigned char> profileImage;
static std::future<std::vector<unsigned char>> pendingAvatar;
static bool profileUploadAttempted=false;
MyGifData gif;
static bool g_TextureInitialized = false;

void Menu::Images( )

{
    if (!g_TextureInitialized) {
        LoadTextureFromMemory(logo_two, sizeof(logo_two), &logo);
        LoadTextureFromMemory(user, sizeof(user), &userlogo);
        LoadGifFromMemory(gif_file_bytes, sizeof(gif_file_bytes), &gif);
        g_TextureInitialized = true;
    }
}

namespace Menu::Resources {
    void SetupStyleAndFonts() {
        ImGuiIO& io = ImGui::GetIO( );
        (void)io;

        ImGuiStyle& style = ImGui::GetStyle( );
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = false; // wichtig: false, sonst kÃƒÂ¶nnen Glow-Kreise bei manchen Backends kantig wirken
        style.AntiAliasedFill = true;
        style.CircleTessellationMaxError = 0.10f; // kleinerer Wert = glattere Kreise (mehr Segmente)

        // Load Fonts
        static const ImWchar ranges[] =
            {
                0x0020,
                0x00FF,
                0x0400,
                0x052F,
                0x2DE0,
                0x2DFF,
                0xA640,
                0xA69F,
                0xE000,
                0xE226,
                0,
            };

        ImFontConfig font_config;
        // Embedded arrays belong to the DLL; let ImGui copy them into owned storage.
        font_config.FontDataOwnedByAtlas = false;
        font_config.PixelSnapH = false;
        font_config.OversampleH = 5;
        font_config.OversampleV = 5;
        font_config.RasterizerMultiply = 1.2f;

        static const ImWchar icons_ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};

        io.Fonts->AddFontFromMemoryTTF(poppin_font, sizeof(poppin_font), 16, &font_config, ranges);
        // File-loaded data is allocated by ImGui and must stay owned by the atlas.
        ImFontConfig file_font_config = font_config;
        file_font_config.FontDataOwnedByAtlas = true;
        tab_title = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arialbd.ttf", 19.0f, &file_font_config, ranges);
        font_icon = io.Fonts->AddFontFromMemoryTTF(icon_font, sizeof(icon_font), 25.0f, &font_config, ranges);
        poppins = io.Fonts->AddFontFromMemoryTTF(poppin_font, sizeof(poppin_font), 25.0f, &font_config, ranges);

        ImFontConfig icons_config;
        icons_config.FontDataOwnedByAtlas = false;
        icons_config.PixelSnapH = true;
        icons_config.GlyphMinAdvanceX = 25.0f;
        icons = io.Fonts->AddFontFromMemoryTTF(new_icons, sizeof(new_icons), 25.0f, &icons_config, icons_ranges);

        // Orbit menu fonts. Orbit picks the nearest loaded size for the drawn pixel size (spec size x menu scale x text size %,
        // minimum 10 px), so the sizes below cover 80/100/120 % within ~8 %. Latin-1 for text up to 16 px (the middle dot is
        // used in small mono text), ASCII only for the large sizes (titles, module names) to keep the atlas small.
        static const ImWchar orbit_ranges[] = {0x0020, 0x00FF, 0};
        static const ImWchar orbit_ascii[] = {0x0020, 0x007E, 0};
        auto load_face = [&](OrbitFace& face, const unsigned char* data, int bytes, bool pixel_snap, std::initializer_list<float> sizes) {
            for (const float px : sizes) {
                ImFontConfig cfg = font_config; // original quality settings: oversampling 5x5
                cfg.PixelSnapH = pixel_snap;
                if (px > 26.0f) { cfg.OversampleH = 2; cfg.OversampleV = 2; }
                else if (px > 16.0f) { cfg.OversampleH = 3; cfg.OversampleV = 3; }
                ImFont* f = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), bytes, px, &cfg, px > 16.0f ? orbit_ascii : orbit_ranges);
                if (f && face.count < OrbitFace::MAX) { face.font[face.count] = f; face.size[face.count++] = px; }
            }
        };
        load_face(orbit_text, space_grotesk_medium, sizeof(space_grotesk_medium), false, {10.0f, 12.0f, 13.0f, 15.0f});
        load_face(orbit_text_bold, space_grotesk_bold, sizeof(space_grotesk_bold), false, {10.0f, 12.0f, 13.0f, 16.0f, 18.0f, 22.0f, 26.0f, 35.0f, 44.0f, 53.0f});
        load_face(orbit_mono, jetbrains_mono_regular, sizeof(jetbrains_mono_regular), true, {10.0f, 12.0f, 14.0f});
        load_face(orbit_mono_bold, jetbrains_mono_bold, sizeof(jetbrains_mono_bold), true, {10.0f, 12.0f, 13.0f, 16.0f, 19.0f, 24.0f, 29.0f});

        io.Fonts->Build( );
    }

    ImTextureID LogoTexture() { return logo.TextureID(); }
    void RequestUserAvatar(const std::string& username) {
        // GPU resources are touched only by UserTexture() on the presentation thread.
        auto task=std::make_shared<std::packaged_task<std::vector<unsigned char>()>>([username]{return DownloadUserAvatar(username);});
        pendingAvatar=task->get_future();
        Lifecycle::Spawn([task]{(*task)();});
    }
    ImTextureID UserTexture() {
        if(pendingAvatar.valid() && pendingAvatar.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
            try {profileImage=pendingAvatar.get();}catch(...) {profileImage.clear();}
        }
        if(!profileUploadAttempted && !profileImage.empty()) {
            profileUploadAttempted=true;
            if(!AvatarPolicy::ValidImage(profileImage) || !LoadTextureFromMemory(profileImage.data(),static_cast<int>(profileImage.size()),&profileAvatar)) {
                profileImage.clear();
            }
        }
        return profileAvatar.TextureID()?profileAvatar.TextureID():userlogo.TextureID();
    }
    bool HasUserAvatar() { return profileAvatar.TextureID() ? true : false; }
    void UpdateAnimation(float deltaTime) { UpdateGif(gif, deltaTime); }
    void ReleaseTextures(bool /*vulkanTextures*/) {
        DestroyTexture(&logo);
        DestroyTexture(&userlogo);
        DestroyTexture(&profileAvatar);
        profileUploadAttempted=false; // CPU pixels survive backend/context recreation.
        DestroyGif(gif);
        g_TextureInitialized = false;
    }
}
