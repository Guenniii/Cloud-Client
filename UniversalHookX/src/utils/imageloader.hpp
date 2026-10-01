#pragma once
#include <vector>
#include <cstring>
#include <cstdint>
#include <vulkan/vulkan.h>
#include "../dependencies/imgui/imgui.h"

// ======================================================
// SINGLE TEXTURE
// ======================================================
struct MyTextureData {
    VkDescriptorSet DS;
    unsigned int GLTexture;
    ImTextureID TextureID() const { return GLTexture ? (ImTextureID)(intptr_t)GLTexture : (ImTextureID)DS; }

    int Width;
    int Height;

    VkImage Image;
    VkDeviceMemory ImageMemory;
    VkImageView ImageView;
    VkSampler Sampler;

    VkBuffer UploadBuffer;
    VkDeviceMemory UploadBufferMemory;

    MyTextureData( ) { memset(this, 0, sizeof(*this)); }
};

// ======================================================
// ANIMATED GIF
// ======================================================
struct MyGifData {
    std::vector<MyTextureData> Frames; // one texture per frame
    std::vector<float> Delays;         // delay in seconds per frame

    int CurrentFrame = 0;
    float Timer = 0.0f;

    int Width = 0;
    int Height = 0;

    bool IsValid( ) const { return !Frames.empty( ); }
};

// ======================================================
// API
// ======================================================

// Load a single image from memory (PNG / JPG / …)
bool LoadTextureFromMemory(const unsigned char* data, int data_size, MyTextureData* out_tex);
void DestroyTexture(MyTextureData* tex);

// Load an animated GIF from memory
// Call UpdateGif() every frame, then use gif.Frames[gif.CurrentFrame].DS as ImTextureID
bool LoadGifFromMemory(const unsigned char* data, int data_size, MyGifData* out_gif);

// Advance the animation; pass ImGui::GetIO().DeltaTime
void UpdateGif(MyGifData& gif, float delta_time);

// Free all GPU resources for a GIF
void DestroyGif(MyGifData& gif);
