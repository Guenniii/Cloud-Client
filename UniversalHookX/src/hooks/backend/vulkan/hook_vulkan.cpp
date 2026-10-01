#include "../../../backend.hpp"
#include "../../../console/console.hpp"

#ifdef ENABLE_BACKEND_VULKAN
#include <Windows.h>
#include <thread>
#include <chrono>

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

// https://vulkan.lunarg.com/
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")
#include <atomic>
#include "hook_vulkan.hpp"
#include "present_sync.hpp"

#include "../../../dependencies/imgui/imgui_impl_vulkan.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../hooks.hpp"

#include "../../../menu/menu.hpp"
#include "../../../menu/resources.hpp"
#include "../../../utils/imageloader.hpp"
#include "../../../modules/settings.hpp"

static std::atomic<bool> g_bVKProbeCalled{false};
static std::add_pointer_t<VkResult VKAPI_CALL(VkQueue, const VkPresentInfoKHR*)> oProbeQueuePresentKHR;
static void* g_pProbedQueuePresent = nullptr;



static VkResult VKAPI_CALL hkProbeQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    Lifecycle::Callback callback;
    g_bVKProbeCalled.store(true, std::memory_order_relaxed);
    return oProbeQueuePresentKHR(queue, pPresentInfo);
}



VkAllocationCallbacks* g_Allocator = NULL;
VkInstance g_Instance = VK_NULL_HANDLE;
VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
VkDevice g_FakeDevice = VK_NULL_HANDLE, g_Device = VK_NULL_HANDLE;
VkQueue g_Queue = VK_NULL_HANDLE;
VkCommandPool g_CommandPool = VK_NULL_HANDLE;

static uint32_t g_QueueFamily = (uint32_t)-1;
static std::vector<VkQueueFamilyProperties> g_QueueFamilies;

static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;
static uint32_t g_MinImageCount = 2;
static VkRenderPass g_RenderPass = VK_NULL_HANDLE;
static std::vector<ImGui_ImplVulkanH_Frame> g_Frames;
static std::vector<ImGui_ImplVulkanH_FrameSemaphores> g_FrameSemaphores;

static HWND g_Hwnd = NULL;
static VkExtent2D g_ImageExtent = { };
struct SwapchainInfo { VkDevice device{}; VkExtent2D extent{}; VkFormat format=VK_FORMAT_B8G8R8A8_UNORM; };
static std::unordered_map<VkSwapchainKHR, SwapchainInfo> g_Swapchains;
static VkSwapchainKHR g_ActiveSwapchain = VK_NULL_HANDLE;
static VkFormat g_SwapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;
static bool g_RenderFailed = false;
static bool VkOk(VkResult result, const char* operation) {
    if(result==VK_SUCCESS) return true;
    LOG("[-] Vulkan overlay: %s failed (%d); overlay paused until swapchain replacement.\n", operation, static_cast<int>(result));
    g_RenderFailed=true;
    return false;
}


static void CleanupDeviceVulkan( );
static void CleanupRenderTarget( );
static void RenderImGui_Vulkan(VkQueue queue, const VkPresentInfoKHR* pPresentInfo, std::vector<VkSemaphore>& overlayWaits);
static bool DoesQueueSupportGraphic(VkQueue queue, VkQueue* pGraphicQueue);


// Only our short-lived discovery instance omits layers. The game's existing
// instance and its OBS layer remain active. Restore process environment on every exit.
class ProbeEnvironment {
    struct Value { const wchar_t* name; std::wstring value; bool existed=false, changed=false; };
    Value values[2]{{L"VK_LOADER_LAYERS_DISABLE"},{L"DISABLE_VULKAN_OBS_CAPTURE"}};
public:
    bool Apply() {
        const wchar_t* replacements[]={L"~all~",L"1"};
        for(int i=0;i<2;++i) {
            auto& v=values[i];SetLastError(ERROR_SUCCESS);
            DWORD count=GetEnvironmentVariableW(v.name,nullptr,0);
            v.existed=count!=0 || GetLastError()!=ERROR_ENVVAR_NOT_FOUND;
            if(count){std::vector<wchar_t> text(count);if(GetEnvironmentVariableW(v.name,text.data(),count)>=count)return false;v.value=text.data();}
            if(!SetEnvironmentVariableW(v.name,replacements[i]))return false;
            v.changed=true;
        }
        return true;
    }
    ~ProbeEnvironment(){for(auto& v:values)if(v.changed)SetEnvironmentVariableW(v.name,v.existed?v.value.c_str():nullptr);}
};

static void FreeProbeDevice() {
    if(g_FakeDevice){vkDestroyDevice(g_FakeDevice,g_Allocator);g_FakeDevice=VK_NULL_HANDLE;}
    if(g_Instance){vkDestroyInstance(g_Instance,g_Allocator);g_Instance=VK_NULL_HANDLE;}
}
static bool CreateDeviceVK() {
    ProbeEnvironment environment;
    if(!environment.Apply()){LOG("[-] Vulkan: discovery environment unavailable.\n");return false;}
    VkInstanceCreateInfo instance{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    const char* extension="VK_KHR_surface";instance.enabledExtensionCount=1;instance.ppEnabledExtensionNames=&extension;
    if(vkCreateInstance(&instance,g_Allocator,&g_Instance)!=VK_SUCCESS)return false;
    uint32_t count=0;
    if(vkEnumeratePhysicalDevices(g_Instance,&count,nullptr)!=VK_SUCCESS||!count){FreeProbeDevice();return false;}
    std::vector<VkPhysicalDevice> devices(count);
    if(vkEnumeratePhysicalDevices(g_Instance,&count,devices.data())!=VK_SUCCESS){FreeProbeDevice();return false;}
    g_PhysicalDevice=devices[0];
    for(auto device:devices){VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties(device,&props);if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){g_PhysicalDevice=device;break;}}
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice,&count,nullptr);g_QueueFamilies.resize(count);
    vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice,&count,g_QueueFamilies.data());
    g_QueueFamily=UINT32_MAX;
    for(uint32_t i=0;i<count;++i)if(g_QueueFamilies[i].queueFlags&VK_QUEUE_GRAPHICS_BIT){g_QueueFamily=i;break;}
    if(g_QueueFamily==UINT32_MAX){FreeProbeDevice();return false;}
    float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=g_QueueFamily;queue.queueCount=1;queue.pQueuePriorities=&priority;
    const char* swapchain="VK_KHR_swapchain";VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.enabledExtensionCount=1;device.ppEnabledExtensionNames=&swapchain;
    if(vkCreateDevice(g_PhysicalDevice,&device,g_Allocator,&g_FakeDevice)!=VK_SUCCESS){FreeProbeDevice();return false;}
    return true;
}
static HMODULE FunctionModule(void* address) {
    HMODULE module=nullptr;
    if(address)GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&module);
    return module;
}
static void* DriverPresentAddress() {
    void* present=reinterpret_cast<void*>(vkGetDeviceProcAddr(g_FakeDevice,"vkQueuePresentKHR"));
    void* create=reinterpret_cast<void*>(vkGetDeviceProcAddr(g_FakeDevice,"vkCreateBuffer"));
    HMODULE module=FunctionModule(present);
    if(!module||module!=FunctionModule(create)||module==GetModuleHandleW(L"vulkan-1.dll")||module==GetModuleHandleW(L"graphics-hook64.dll")){
        LOG("[-] Vulkan: could not verify a driver-level presentation entry; hook not installed.\n");return nullptr;
    }
    wchar_t path[MAX_PATH]{};GetModuleFileNameW(module,path,MAX_PATH);
    LOG("[Vulkan] Driver presentation entry: %ls\n",path);
    return present;
}
static void ReportCaptureOrder() {
    static bool observed=false;static ULONGLONG next=0;
    if(observed||GetTickCount64()<next)return;next=GetTickCount64()+5000;
    void* frames[24]{};USHORT count=CaptureStackBackTrace(0,24,frames,nullptr);
    HMODULE obs=GetModuleHandleW(L"graphics-hook64.dll");
    for(USHORT i=0;obs&&i<count;++i)if(FunctionModule(frames[i])==obs){
        observed=true;LOG("[Vulkan] OBS layer precedes client rendering (capture -> client -> present).\n");return;
    }
}

static bool CreateRenderTarget(VkDevice device, VkSwapchainKHR swapchain) {
    uint32_t uImageCount=0;
    if(!VkOk(vkGetSwapchainImagesKHR(device, swapchain, &uImageCount, nullptr), "image count")) return false;
    if(uImageCount<2 || uImageCount>64) { g_RenderFailed=true; return false; }
    std::vector<VkImage> backbuffers(uImageCount);
    if(!VkOk(vkGetSwapchainImagesKHR(device, swapchain, &uImageCount, backbuffers.data()), "swapchain images")) return false;
    if(uImageCount<2 || uImageCount>backbuffers.size()) { g_RenderFailed=true; return false; }
    g_Frames.resize(uImageCount);
    g_FrameSemaphores.resize(uImageCount);

    for (uint32_t i = 0; i < uImageCount; ++i) {
        g_Frames[i].Backbuffer = backbuffers[i];

        ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
        ImGui_ImplVulkanH_FrameSemaphores* fsd = &g_FrameSemaphores[i];
        {
            VkCommandPoolCreateInfo info = { };
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = g_QueueFamily;

            if(!VkOk(vkCreateCommandPool(device, &info, g_Allocator, &fd->CommandPool), "vkCreateCommandPool")) return false;
        }
        {
            VkCommandBufferAllocateInfo info = { };
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = fd->CommandPool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;

            if(!VkOk(vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer), "vkAllocateCommandBuffers")) return false;
        }
        {
            VkFenceCreateInfo info = { };
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            if(!VkOk(vkCreateFence(device, &info, g_Allocator, &fd->Fence), "vkCreateFence")) return false;
        }
        {
            VkSemaphoreCreateInfo info = { };
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            if(!VkOk(vkCreateSemaphore(device, &info, g_Allocator, &fsd->ImageAcquiredSemaphore), "vkCreateSemaphore")) return false;
            if(!VkOk(vkCreateSemaphore(device, &info, g_Allocator, &fsd->RenderCompleteSemaphore), "vkCreateSemaphore")) return false;
        }
    }

    // Create the Render Pass
    {
        VkAttachmentDescription attachment = { };
        attachment.format = g_SwapchainFormat;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference color_attachment = { };
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = { };
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;

        VkRenderPassCreateInfo info = { };
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;

        VkSubpassDependency dependency{};
        dependency.srcSubpass=VK_SUBPASS_EXTERNAL; dependency.dstSubpass=0;
        dependency.srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dependency.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
        dependency.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        info.dependencyCount=1; info.pDependencies=&dependency;
        if(!VkOk(vkCreateRenderPass(device, &info, g_Allocator, &g_RenderPass), "vkCreateRenderPass")) return false;
    }

    // Create The Image Views
    {
        VkImageViewCreateInfo info = { };
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = g_SwapchainFormat;

        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;

        for (uint32_t i = 0; i < uImageCount; ++i) {
            ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
            info.image = fd->Backbuffer;

            if(!VkOk(vkCreateImageView(device, &info, g_Allocator, &fd->BackbufferView), "vkCreateImageView")) return false;
        }
    }

    // Create Framebuffer
    {
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = { };
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = g_RenderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.layers = 1;
        info.width = g_ImageExtent.width;
        info.height = g_ImageExtent.height;

        for (uint32_t i = 0; i < uImageCount; ++i) {
            ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
            attachment[0] = fd->BackbufferView;

            if(!VkOk(vkCreateFramebuffer(device, &info, g_Allocator, &fd->Framebuffer), "vkCreateFramebuffer")) return false;
        }
    }

    if (!g_DescriptorPool) // Create Descriptor Pool.
    {
        constexpr VkDescriptorPoolSize pool_sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

        VkDescriptorPoolCreateInfo pool_info = { };
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        if(!VkOk(vkCreateDescriptorPool(device, &pool_info, g_Allocator, &g_DescriptorPool), "vkCreateDescriptorPool")) return false;
    }
    return true;
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*)> oAcquireNextImageKHR;
static VkResult VKAPI_CALL hkAcquireNextImageKHR(VkDevice device,
                                                 VkSwapchainKHR swapchain,
                                                 uint64_t timeout,
                                                 VkSemaphore semaphore,
                                                 VkFence fence,
                                                 uint32_t* pImageIndex) {
    Lifecycle::Callback callback;
    if(!H::bShuttingDown) {
        std::lock_guard<std::recursive_mutex> renderLock(Lifecycle::renderMutex);
        if(!g_Device) g_Device=device;
        g_Swapchains[swapchain].device=device;
    }

    return oAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, const VkAcquireNextImageInfoKHR*, uint32_t*)> oAcquireNextImage2KHR;
static VkResult VKAPI_CALL hkAcquireNextImage2KHR(VkDevice device,
                                                  const VkAcquireNextImageInfoKHR* pAcquireInfo,
                                                  uint32_t* pImageIndex) {
    Lifecycle::Callback callback;
    if(!H::bShuttingDown) {
        std::lock_guard<std::recursive_mutex> renderLock(Lifecycle::renderMutex);
        if(!g_Device) g_Device=device;
        if(pAcquireInfo) g_Swapchains[pAcquireInfo->swapchain].device=device;
    }

    return oAcquireNextImage2KHR(device, pAcquireInfo, pImageIndex);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkQueue, const VkPresentInfoKHR*)> oQueuePresentKHR;
static VkResult VKAPI_CALL hkQueuePresentKHR(VkQueue queue,
                                             const VkPresentInfoKHR* pPresentInfo) {
    Lifecycle::Callback callback;
    std::lock_guard<std::recursive_mutex> renderLock(Lifecycle::renderMutex);
    ReportCaptureOrder();
    std::vector<VkSemaphore> overlayWaits;
    if(H::bShuttingDown && Lifecycle::workersStopped && !Lifecycle::renderStopped) {
        Lifecycle::Trace("Vulkan: wait for GPU idle");
        if(g_Device) vkDeviceWaitIdle(g_Device);
        Lifecycle::Trace("Vulkan: GPU idle; menu shutdown");
        Menu::Shutdown(g_Device!=VK_NULL_HANDLE);
        Lifecycle::Trace("Vulkan: backend shutdown");
        VK::Unhook();
        Lifecycle::Trace("Vulkan: cleanup complete");
        Lifecycle::renderStopped=true;
    } else if(!H::bShuttingDown) RenderImGui_Vulkan(queue, pPresentInfo, overlayWaits);

    VkPresentInfoKHR present=VulkanOverlay::ComposePresent(*pPresentInfo,overlayWaits);
    // Overlay submission consumed the game's waits; presentation must consume our completion signals.
    return oQueuePresentKHR(queue, &present);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*)> oCreateSwapchainKHR;
static VkResult VKAPI_CALL hkCreateSwapchainKHR(VkDevice device,
                                                const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                const VkAllocationCallbacks* pAllocator,
                                                VkSwapchainKHR* pSwapchain) {
    Lifecycle::Callback callback;
    VkResult result=oCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
    if(result==VK_SUCCESS && !H::bShuttingDown) {
        std::lock_guard<std::recursive_mutex> renderLock(Lifecycle::renderMutex);
        g_Swapchains[*pSwapchain]={device,pCreateInfo->imageExtent,pCreateInfo->imageFormat};
        if(pCreateInfo->oldSwapchain) g_Swapchains.erase(pCreateInfo->oldSwapchain);
    }
    return result;
}

// Löst die Adresse von vkQueuePresentKHR über ein temporäres Fake-Device auf
// und räumt Instance/Device danach sofort wieder auf.
static void* ResolveQueuePresentAddress( ) {
    if (!CreateDeviceVK( )) {
        LOG("[!] VK probe: CreateDeviceVK() failed.\n");
        return nullptr;
    }

    void* fn = DriverPresentAddress();

    if (g_FakeDevice) {
        vkDestroyDevice(g_FakeDevice, g_Allocator);
        g_FakeDevice = NULL;
    }
    if (g_Instance) {
        vkDestroyInstance(g_Instance, g_Allocator);
        g_Instance = NULL;
    }

    return fn;
}


namespace VK {
    void Hook(HWND hwnd) {
        if (!CreateDeviceVK( )) {
            LOG("[!] CreateDeviceVK() failed.\n");
            Lifecycle::renderStopped=true;
            return;
        }

        void* fnAcquireNextImageKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_FakeDevice, "vkAcquireNextImageKHR"));
        void* fnAcquireNextImage2KHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_FakeDevice, "vkAcquireNextImage2KHR"));
        void* fnQueuePresentKHR = DriverPresentAddress();
        void* fnCreateSwapchainKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_FakeDevice, "vkCreateSwapchainKHR"));

        if (g_FakeDevice) {
            vkDestroyDevice(g_FakeDevice, g_Allocator);
            g_FakeDevice = NULL;
        }

        if (fnAcquireNextImageKHR && fnQueuePresentKHR && fnCreateSwapchainKHR) {
            g_Hwnd = hwnd;

            // Hook
            LOG("[+] Vulkan: fnAcquireNextImageKHR: 0x%p\n", fnAcquireNextImageKHR);
            LOG("[+] Vulkan: fnAcquireNextImage2KHR: 0x%p\n", fnAcquireNextImage2KHR);
            LOG("[+] Vulkan: fnQueuePresentKHR: 0x%p\n", fnQueuePresentKHR);
            LOG("[+] Vulkan: fnCreateSwapchainKHR: 0x%p\n", fnCreateSwapchainKHR);

            struct Entry { void* address; void* hook; void** original; bool required; };
            Entry entries[]={
                {fnAcquireNextImageKHR,reinterpret_cast<void*>(&hkAcquireNextImageKHR),reinterpret_cast<void**>(&oAcquireNextImageKHR),true},
                {fnAcquireNextImage2KHR,reinterpret_cast<void*>(&hkAcquireNextImage2KHR),reinterpret_cast<void**>(&oAcquireNextImage2KHR),false},
                {fnQueuePresentKHR,reinterpret_cast<void*>(&hkQueuePresentKHR),reinterpret_cast<void**>(&oQueuePresentKHR),true},
                {fnCreateSwapchainKHR,reinterpret_cast<void*>(&hkCreateSwapchainKHR),reinterpret_cast<void**>(&oCreateSwapchainKHR),true}};
            std::vector<void*> installed;
            for(auto& entry:entries){
                if(!entry.address&&!entry.required)continue;
                auto result=MH_CreateHook(entry.address,entry.hook,entry.original);
                if(result!=MH_OK){
                    LOG("[-] Vulkan hook creation failed: %s\n",MH_StatusToString(result));
                    for(auto address:installed)MH_RemoveHook(address);
                    FreeProbeDevice();Lifecycle::renderStopped=true;return;
                }
                installed.push_back(entry.address);
            }
            bool queued=true;
            for(auto address:installed)queued=MH_QueueEnableHook(address)==MH_OK && queued;
            auto result=queued?MH_ApplyQueued():MH_UNKNOWN;
            if(result!=MH_OK){
                LOG("[-] Vulkan hook activation failed: %s\n",MH_StatusToString(result));
                for(auto address:installed){MH_QueueDisableHook(address);MH_DisableHook(address);}
                // Retain trampolines until the normal callback drain at unload.
                Lifecycle::renderStopped=true;return;
            }
            LOG("[Vulkan] Native rendering installed below implicit capture layers.\n");
        } else {
            LOG("[-] Vulkan: required driver entries unavailable.\n");
            FreeProbeDevice();Lifecycle::renderStopped=true;
        }

    }

    void Unhook( ) {
        Lifecycle::Trace("Vulkan: destroy ImGui context/backend");
        if (ImGui::GetCurrentContext( )) {
            if (ImGui::GetIO( ).BackendRendererUserData) {
                Lifecycle::Trace("Vulkan: ImGui renderer shutdown begin");
                ImGui_ImplVulkan_Shutdown( );
                Lifecycle::Trace("Vulkan: ImGui renderer shutdown complete");
            }

            if (ImGui::GetIO( ).BackendPlatformUserData) {
                Lifecycle::Trace("Vulkan: ImGui Win32 shutdown begin");
                ImGui_ImplWin32_Shutdown( );
                Lifecycle::Trace("Vulkan: ImGui Win32 shutdown complete");
            }

            Lifecycle::Trace("Vulkan: ImGui context destruction begin");
            ImGui::DestroyContext( );
            Lifecycle::Trace("Vulkan: ImGui context destruction complete");
        }

        Lifecycle::Trace("Vulkan: destroy owned frame resources");
        CleanupDeviceVulkan( );
    }

    bool InstallProbe( ) {
        if (!GetModuleHandleA("vulkan-1.dll"))
            return false;

        void* fn = ResolveQueuePresentAddress( );
        if (!fn)
            return false;

        if (MH_CreateHook(fn, &hkProbeQueuePresentKHR, reinterpret_cast<void**>(&oProbeQueuePresentKHR)) != MH_OK)
            return false;

        if(MH_EnableHook(fn)!=MH_OK){MH_RemoveHook(fn);return false;}
        g_pProbedQueuePresent = fn;
        LOG("[+] VK probe hook installed (0x%p).\n", fn);
        return true;
    }

    void RemoveProbe( ) {
        if (!g_pProbedQueuePresent)
            return;

        MH_DisableHook(g_pProbedQueuePresent);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // WICHTIG: Race Condition vermeiden
        MH_RemoveHook(g_pProbedQueuePresent);

        g_pProbedQueuePresent = nullptr;
        LOG("[-] VK probe hook removed.\n");
    }

    bool WasCalled( ) {
        return g_bVKProbeCalled.load(std::memory_order_relaxed);
    }

} // namespace VK

static void CleanupRenderTarget( ) {
    for (uint32_t i = 0; i < g_Frames.size(); ++i) {
        if (g_Frames[i].Fence) {
            vkDestroyFence(g_Device, g_Frames[i].Fence, g_Allocator);
            g_Frames[i].Fence = VK_NULL_HANDLE;
        }
        if (g_Frames[i].CommandBuffer) {
            vkFreeCommandBuffers(g_Device, g_Frames[i].CommandPool, 1, &g_Frames[i].CommandBuffer);
            g_Frames[i].CommandBuffer = VK_NULL_HANDLE;
        }
        if (g_Frames[i].CommandPool) {
            vkDestroyCommandPool(g_Device, g_Frames[i].CommandPool, g_Allocator);
            g_Frames[i].CommandPool = VK_NULL_HANDLE;
        }
        if (g_Frames[i].Framebuffer) {
            vkDestroyFramebuffer(g_Device, g_Frames[i].Framebuffer, g_Allocator);
            g_Frames[i].Framebuffer = VK_NULL_HANDLE;
        }
        if (g_Frames[i].BackbufferView) {
            vkDestroyImageView(g_Device, g_Frames[i].BackbufferView, g_Allocator);
            g_Frames[i].BackbufferView = VK_NULL_HANDLE;
        }
    }

    for (uint32_t i = 0; i < g_FrameSemaphores.size(); ++i) {
        if (g_FrameSemaphores[i].ImageAcquiredSemaphore) {
            vkDestroySemaphore(g_Device, g_FrameSemaphores[i].ImageAcquiredSemaphore, g_Allocator);
            g_FrameSemaphores[i].ImageAcquiredSemaphore = VK_NULL_HANDLE;
        }
        if (g_FrameSemaphores[i].RenderCompleteSemaphore) {
            vkDestroySemaphore(g_Device, g_FrameSemaphores[i].RenderCompleteSemaphore, g_Allocator);
            g_FrameSemaphores[i].RenderCompleteSemaphore = VK_NULL_HANDLE;
        }
    }
    g_Frames.clear();
    g_FrameSemaphores.clear();
    g_CommandPool=VK_NULL_HANDLE;
}

static void CleanupDeviceVulkan( ) {
    CleanupRenderTarget( );

    if(g_RenderPass && g_Device) {
        vkDestroyRenderPass(g_Device,g_RenderPass,g_Allocator);
        g_RenderPass=VK_NULL_HANDLE;
    }
    g_CommandPool=VK_NULL_HANDLE;
    if (g_DescriptorPool) {
        vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
        g_DescriptorPool = NULL;
    }
    if (g_Instance) {
        vkDestroyInstance(g_Instance, g_Allocator);
        g_Instance = NULL;
    }

    g_ImageExtent = { };
    g_Device = NULL;
}



// Called only on the serialized render path after the device has become idle.
static void ResetSwapchainOverlay() {
    Menu::Resources::ReleaseTextures(true);
    if(ImGui::GetCurrentContext() && ImGui::GetIO().BackendRendererUserData) ImGui_ImplVulkan_Shutdown();
    CleanupRenderTarget();
    if(g_RenderPass) vkDestroyRenderPass(g_Device,g_RenderPass,g_Allocator);
    if(g_DescriptorPool) vkDestroyDescriptorPool(g_Device,g_DescriptorPool,g_Allocator);
    g_RenderPass=VK_NULL_HANDLE;g_DescriptorPool=VK_NULL_HANDLE;
    g_RenderFailed=false;
}

static void RenderImGui_Vulkan(VkQueue queue, const VkPresentInfoKHR* present, std::vector<VkSemaphore>& overlayWaits) {
    if(!g_Device || H::bShuttingDown || !present || present->swapchainCount!=1) return;
    const auto swapchain=present->pSwapchains[0];
    auto found=g_Swapchains.find(swapchain);
    if(found==g_Swapchains.end() || found->second.device!=g_Device) return;
    if(g_ActiveSwapchain!=swapchain) {
        if(!VkOk(vkDeviceWaitIdle(g_Device), "swapchain idle")) return;
        ResetSwapchainOverlay();
        g_ActiveSwapchain=swapchain;
        g_ImageExtent=found->second.extent;
        g_SwapchainFormat=found->second.format;
    }
    if(g_RenderFailed) return;
    if(!g_ImageExtent.width || !g_ImageExtent.height) {
        // Injection can happen after swapchain creation; use the current client dimensions.
        RECT rect{};
        if(!GetClientRect(g_Hwnd,&rect) || rect.right<=rect.left || rect.bottom<=rect.top) return;
        g_ImageExtent={static_cast<uint32_t>(rect.right-rect.left),static_cast<uint32_t>(rect.bottom-rect.top)};
    }
    VkQueue graphicQueue=VK_NULL_HANDLE;
    // The overlay needs a graphics-capable presentation queue. Other queues pass through unchanged.
    if(!DoesQueueSupportGraphic(queue,&graphicQueue)) return;
    graphicQueue=queue;
    Menu::InitializeContext(g_Hwnd);
    if(g_Frames.empty() && !CreateRenderTarget(g_Device,swapchain)) return;
    const uint32_t index=present->pImageIndices[0];
    if(index>=g_Frames.size()) {g_RenderFailed=true;return;}
    auto* fd=&g_Frames[index];auto* sem=&g_FrameSemaphores[index];
    if(!VkOk(vkWaitForFences(g_Device,1,&fd->Fence,VK_TRUE,1000000000ull),"frame fence")) return;
    if(!VkOk(vkResetCommandBuffer(fd->CommandBuffer,0),"reset command buffer")) return;
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if(!VkOk(vkBeginCommandBuffer(fd->CommandBuffer,&begin),"begin command buffer")) return;
    if(!ImGui::GetIO().BackendRendererUserData) {
        ImGui_ImplVulkan_InitInfo init{};
        init.Instance=g_Instance;init.PhysicalDevice=g_PhysicalDevice;init.Device=g_Device;
        init.QueueFamily=g_QueueFamily;init.Queue=graphicQueue;init.PipelineCache=g_PipelineCache;
        init.DescriptorPool=g_DescriptorPool;init.MinImageCount=2;
        init.ImageCount=static_cast<uint32_t>(g_Frames.size());init.MSAASamples=VK_SAMPLE_COUNT_1_BIT;init.Allocator=g_Allocator;
        if(!ImGui_ImplVulkan_Init(&init,g_RenderPass)) {g_RenderFailed=true;return;}
        // Font copy/barrier commands must be outside a render pass.
        if(!ImGui_ImplVulkan_CreateFontsTexture(fd->CommandBuffer)) {g_RenderFailed=true;return;}
        g_Queue=graphicQueue;g_CommandPool=fd->CommandPool;
        Menu::Images();
    }
    ImGui_ImplVulkan_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    Menu::Render();ImGui::Render();
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass=g_RenderPass;pass.framebuffer=fd->Framebuffer;pass.renderArea.extent=g_ImageExtent;
    vkCmdBeginRenderPass(fd->CommandBuffer,&pass,VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),fd->CommandBuffer);
    vkCmdEndRenderPass(fd->CommandBuffer);
    if(!VkOk(vkEndCommandBuffer(fd->CommandBuffer),"end command buffer")) return;
    std::vector<VkPipelineStageFlags> stages(present->waitSemaphoreCount,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount=present->waitSemaphoreCount;submit.pWaitSemaphores=present->pWaitSemaphores;
    submit.pWaitDstStageMask=stages.data();submit.commandBufferCount=1;submit.pCommandBuffers=&fd->CommandBuffer;
    submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&sem->ImageAcquiredSemaphore;
    // Reset only when there is work ready to submit; never wait on a fence from a failed submit.
    if(!VkOk(vkResetFences(g_Device,1,&fd->Fence),"reset fence")) return;
    if(!VkOk(vkQueueSubmit(graphicQueue,1,&submit,fd->Fence),"overlay submit")) return;
    overlayWaits.push_back(sem->ImageAcquiredSemaphore);
}

static bool DoesQueueSupportGraphic(VkQueue queue, VkQueue* pGraphicQueue) {
    for (uint32_t i = 0; i < g_QueueFamilies.size( ); ++i) {
        const VkQueueFamilyProperties& family = g_QueueFamilies[i];
        for (uint32_t j = 0; j < family.queueCount; ++j) {
            VkQueue it = VK_NULL_HANDLE;
            vkGetDeviceQueue(g_Device, i, j, &it);

            if (pGraphicQueue && family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                if (*pGraphicQueue == VK_NULL_HANDLE) {
                    *pGraphicQueue = it;
                }
            }

            if (queue == it && family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                g_QueueFamily=i;
                if(pGraphicQueue) *pGraphicQueue=it;
                return true;
            }
        }
    }

    return false;
}

#else
#include <Windows.h>
namespace VK {
    void Hook(HWND hwnd) { LOG("[!] Vulkan backend is not enabled!\n"); }
    void Unhook( ) { }

    bool InstallProbe( ) { return false; }
    void RemoveProbe( ) { }
    bool WasCalled( ) { return false; }
} // namespace VK
#endif
