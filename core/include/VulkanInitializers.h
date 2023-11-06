#pragma once
#include <vulkan/vulkan_core.h>

namespace vks
{
    namespace initializers
    {
        VkSubmitInfo SubmitInfo();
        VkSemaphoreCreateInfo SemaphoreCreateInfo();
        VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags = 0);

        VkBufferCreateInfo BufferCreateInfo();
        VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage,VkDeviceSize size);
        VkMemoryAllocateInfo MemoryAllocateInfo();
        VkMappedMemoryRange MappedMemoryRange();

#pragma region Image
        
        VkImageCreateInfo ImageCreateInfo();
        VkSamplerCreateInfo SamplerCreateInfo();
        VkImageViewCreateInfo ImageViewCreateInfo();
        /** @brief Initialize an image memory barrier with no image transfer ownership */
        VkImageMemoryBarrier ImageMemoryBarrier();
        
#pragma endregion Image

#pragma region CommandBuffer
        
        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount);
        VkCommandBufferBeginInfo CommandBufferBeginInfo();
        
#pragma endregion CommandBuffer
    }    
}
