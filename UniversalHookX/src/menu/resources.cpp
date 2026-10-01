#include "resources.hpp"
#include "menu.hpp"
#include <vulkan/vulkan.h>
#include "../utils/imageloader.hpp"
#include "../dependencies/font/IconsFontAwesome5.h"
// Embedded arrays have exactly one translation unit owner.
#include "../dependencies/font/fa-solid-900.h"
#include "../dependencies/images/bytearray.h"
#include "../dependencies/gifs/gif.h"

// Fonts
ImFont* tab_title;
ImFont* font_icon;
ImFont* poppins;
ImFont* icons;

static MyTextureData logo;
static MyTextureData userlogo;
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

        io.Fonts->Build( );
    }

    ImTextureID LogoTexture() { return logo.TextureID(); }
    ImTextureID UserTexture() { return userlogo.TextureID(); }
    void UpdateAnimation(float deltaTime) { UpdateGif(gif, deltaTime); }
    void ReleaseTextures(bool /*vulkanTextures*/) {
        DestroyTexture(&logo);
        DestroyTexture(&userlogo);
        DestroyGif(gif);
        g_TextureInitialized = false;
    }
}
