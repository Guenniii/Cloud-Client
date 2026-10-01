#define STB_IMAGE_IMPLEMENTATION
#include "../dependencies/images/stb_image.h"

#include "../dependencies/imgui/imgui_impl_vulkan.h"

#include "imageloader.hpp"
#include <Windows.h>
#include "utils.hpp"
#include <GL/gl.h>
#pragma comment(lib, "opengl32.lib")
#include <cstring>
#include <vulkan/vulkan.h>

// Externe Vulkan Daten aus deinem Hook
extern VkDevice g_Device;
extern VkPhysicalDevice g_PhysicalDevice;
extern VkAllocationCallbacks* g_Allocator;
extern VkQueue g_Queue;
extern uint32_t g_QueueFamily;
extern VkCommandPool g_CommandPool;

// Texture uploads run on the presentation thread with the game's context current.
static bool UploadOpenGL(const unsigned char* pixels, int w, int h, MyTextureData* out) {
    if (!wglGetCurrentContext() || !pixels || w <= 0 || h <= 0) return false;
    using BindBuffer = void (APIENTRY*)(GLenum, GLuint);
    auto bindBuffer = reinterpret_cast<BindBuffer>(wglGetProcAddress("glBindBuffer"));
    if (!bindBuffer) return false;
    constexpr GLenum unpackBuffer = 0x88EC, unpackBinding = 0x88EF;
    GLint texture = 0, pbo = 0, alignment = 0, row = 0, rows = 0, columns = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(unpackBinding, &pbo);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &rows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &columns);
    bindBuffer(unpackBuffer, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    GLuint id = 0;
    glGenTextures(1, &id);
    GLint actualWidth = 0;
    if (id) {
        glBindTexture(GL_TEXTURE_2D, id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &actualWidth);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    bindBuffer(unpackBuffer, pbo);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, rows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, columns);
    if (actualWidth != w) { if (id) glDeleteTextures(1, &id); return false; }
    out->GLTexture = id; out->Width = w; out->Height = h;
    return true;
}

// ======================================================
// MEMORY HELPER
// ======================================================
uint32_t FindMemoryType(uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &mem_properties);

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    return 0xFFFFFFFF;
}

// ======================================================
// INTERNAL: upload raw RGBA pixels → MyTextureData
// (shared by single-image loader and GIF frame uploader)
// ======================================================
static bool UploadPixels(const unsigned char* pixels,
                         int w, int h,
                         MyTextureData* out_tex) {
    if (U::GetRenderingBackend() == OPENGL) return UploadOpenGL(pixels, w, h, out_tex);
    out_tex->Width = w;
    out_tex->Height = h;

    VkDeviceSize image_size = (VkDeviceSize)w * h * 4;

    // ── Image ──────────────────────────────────────────
    {
        VkImageCreateInfo info{ };
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = VK_FORMAT_R8G8B8A8_UNORM;
        info.extent = {(uint32_t)w, (uint32_t)h, 1};
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        vkCreateImage(g_Device, &info, g_Allocator, &out_tex->Image);

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(g_Device, out_tex->Image, &req);

        VkMemoryAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(g_Device, &alloc, g_Allocator, &out_tex->ImageMemory);
        vkBindImageMemory(g_Device, out_tex->Image, out_tex->ImageMemory, 0);
    }

    // ── Image View ─────────────────────────────────────
    {
        VkImageViewCreateInfo view{ };
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = out_tex->Image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_R8G8B8A8_UNORM;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;

        vkCreateImageView(g_Device, &view, g_Allocator, &out_tex->ImageView);
    }

    // ── Sampler ────────────────────────────────────────
    {
        VkSamplerCreateInfo sampler{ };
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler.maxLod = 1.0f;

        vkCreateSampler(g_Device, &sampler, g_Allocator, &out_tex->Sampler);
    }

    // ── Staging Buffer ─────────────────────────────────
    {
        VkBufferCreateInfo buf{ };
        buf.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buf.size = image_size;
        buf.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        vkCreateBuffer(g_Device, &buf, g_Allocator, &out_tex->UploadBuffer);

        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(g_Device, out_tex->UploadBuffer, &req);

        VkMemoryAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = FindMemoryType(req.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(g_Device, &alloc, g_Allocator, &out_tex->UploadBufferMemory);
        vkBindBufferMemory(g_Device, out_tex->UploadBuffer, out_tex->UploadBufferMemory, 0);

        void* map;
        vkMapMemory(g_Device, out_tex->UploadBufferMemory, 0, image_size, 0, &map);
        memcpy(map, pixels, (size_t)image_size);
        vkUnmapMemory(g_Device, out_tex->UploadBufferMemory);
    }

    // ── Command Buffer: transition + copy ──────────────
    VkCommandBuffer cmd;
    {
        VkCommandBufferAllocateInfo alloc{ };
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool = g_CommandPool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        vkAllocateCommandBuffers(g_Device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{ };
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin);
    }

    {
        VkImageMemoryBarrier barrier{ };
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.image = out_tex->Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{ };
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {(uint32_t)w, (uint32_t)h, 1};

        vkCmdCopyBufferToImage(cmd, out_tex->UploadBuffer, out_tex->Image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);

    {
        VkSubmitInfo submit{ };
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;

        vkQueueSubmit(g_Queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(g_Queue);
    }

    vkFreeCommandBuffers(g_Device, g_CommandPool, 1, &cmd);

    // ── ImGui Descriptor ───────────────────────────────
    out_tex->DS = ImGui_ImplVulkan_AddTexture(
        out_tex->Sampler,
        out_tex->ImageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return true;
}

// ======================================================
// SINGLE IMAGE LOAD (public)
// ======================================================
bool LoadTextureFromMemory(const unsigned char* data, int data_size, MyTextureData* out_tex) {
    int channels;
    unsigned char* pixels = stbi_load_from_memory(
        data, data_size,
        &out_tex->Width, &out_tex->Height,
        &channels, 4);

    if (!pixels)
        return false;

    bool ok = UploadPixels(pixels, out_tex->Width, out_tex->Height, out_tex);
    stbi_image_free(pixels);
    return ok;
}

// ======================================================
// GIF LOAD
// ======================================================
bool LoadGifFromMemory(const unsigned char* data, int data_size, MyGifData* out_gif) {
    // stbi_load_gif_from_memory liefert alle Frames am Stück:
    //   Layout: [frame0_row0..rowH][frame1_row0..rowH]…
    //   delays[i] = Delay von Frame i in Millisekunden

    int width = 0;
    int height = 0;
    int frames = 0;
    int channels;
    int* delays = nullptr; // stbi alloziert; wir müssen stbi_image_free() rufen

    stbi_uc* all_pixels = stbi_load_gif_from_memory(
        data, data_size,
        &delays,
        &width, &height,
        &frames,
        &channels,
        STBI_rgb_alpha); // erzwingt RGBA

    if (!all_pixels || frames <= 0) {
        stbi_image_free(all_pixels); stbi_image_free(delays); return false;
    }

    out_gif->Width = width;
    out_gif->Height = height;
    out_gif->Frames.resize(frames);
    out_gif->Delays.resize(frames);

    const size_t frame_bytes = (size_t)width * height * 4;

    for (int i = 0; i < frames; ++i) {
        const unsigned char* frame_pixels = all_pixels + i * frame_bytes;

        if (!UploadPixels(frame_pixels, width, height, &out_gif->Frames[i])) {
            // Bereits hochgeladene Frames aufräumen
            DestroyGif(*out_gif);

            stbi_image_free(all_pixels);
            stbi_image_free(delays); // delays ist ein stbi-Puffer
            return false;
        }

        // stb liefert Delays in Millisekunden → Sekunden umrechnen
        // Manche GIFs kodieren 0 ms; dann Fallback auf 100 ms (10 fps)
        int delay_ms = (delays && delays[i] > 0) ? delays[i] : 100;
        out_gif->Delays[i] = delay_ms / 1000.0f;
    }

    stbi_image_free(all_pixels);
    if (delays)
        stbi_image_free(delays);

    out_gif->CurrentFrame = 0;
    out_gif->Timer = 0.0f;

    return true;
}

// ======================================================
// GIF UPDATE  –  einmal pro Frame aufrufen
// ======================================================
void UpdateGif(MyGifData& gif, float delta_time) {
    if (!gif.IsValid( ))
        return;

    gif.Timer += delta_time;

    // Frame(s) weiterschalten (bei sehr kleinen Delays kann es mehrere sein)
    while (gif.Timer >= gif.Delays[gif.CurrentFrame]) {
        gif.Timer -= gif.Delays[gif.CurrentFrame];
        gif.CurrentFrame = (gif.CurrentFrame + 1) % (int)gif.Frames.size( );
    }
}

// ======================================================
// CLEANUP
// ======================================================
void DestroyTexture(MyTextureData* tex) {
    if (!tex) return;
    if (tex->GLTexture) {
        glDeleteTextures(1, &tex->GLTexture);
        *tex = {}; return;
    }
    if (!tex->DS && !tex->Image && !tex->UploadBuffer) return;
    vkDestroySampler(g_Device, tex->Sampler, nullptr);
    vkDestroyImageView(g_Device, tex->ImageView, nullptr);
    vkDestroyImage(g_Device, tex->Image, nullptr);
    vkFreeMemory(g_Device, tex->ImageMemory, nullptr);
    vkDestroyBuffer(g_Device, tex->UploadBuffer, nullptr);
    vkFreeMemory(g_Device, tex->UploadBufferMemory, nullptr);

    if (tex->DS) ImGui_ImplVulkan_RemoveTexture(tex->DS);
    *tex = {};
}

void DestroyGif(MyGifData& gif) {
    for (auto& frame : gif.Frames)
        DestroyTexture(&frame);

    gif.Frames.clear( );
    gif.Delays.clear( );
    gif.CurrentFrame = 0;
    gif.Timer = 0.0f;
}
