#pragma once
#include <vulkan/vulkan_core.h>

namespace vks
{
    namespace initializers
    {
        VkSemaphoreCreateInfo SemaphoreCreateInfo();
        VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);
        VkSubmitInfo SubmitInfo();
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
            VkCommandPool commandPool, 
            VkCommandBufferLevel level, 
            uint32_t bufferCount);
    }    
}
