#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace VulkanOverlay {
inline VkPresentInfoKHR ComposePresent(const VkPresentInfoKHR& original,const std::vector<VkSemaphore>& completed) {
    VkPresentInfoKHR result=original;
    if(!completed.empty()) {
        result.waitSemaphoreCount=static_cast<uint32_t>(completed.size());
        result.pWaitSemaphores=completed.data();
    }
    return result;
}
}
