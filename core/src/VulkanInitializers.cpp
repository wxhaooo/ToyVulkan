#include <VulkanInitializers.h>

namespace vks
{
    namespace initializers
    {
        VkSemaphoreCreateInfo SemaphoreCreateInfo()
        {
            VkSemaphoreCreateInfo semaphoreCreateInfo {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            return semaphoreCreateInfo;
        }

        VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags flags)
        {
            VkFenceCreateInfo fenceCreateInfo {};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = flags;
            return fenceCreateInfo;
        }

        VkSubmitInfo SubmitInfo()
        {
            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            return submitInfo;
        }
        
        VkBufferCreateInfo BufferCreateInfo()
        {
            VkBufferCreateInfo bufCreateInfo {};
            bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            return bufCreateInfo;
        }

        VkBufferCreateInfo BufferCreateInfo(VkBufferUsageFlags usage, VkDeviceSize size)
        {
            VkBufferCreateInfo bufCreateInfo {};
            bufCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufCreateInfo.usage = usage;
            bufCreateInfo.size = size;
            return bufCreateInfo;
        }

        VkMemoryAllocateInfo MemoryAllocateInfo()
        {
            VkMemoryAllocateInfo memAllocInfo {};
            memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            return memAllocInfo;
        }

        VkMappedMemoryRange MappedMemoryRange()
        {
            VkMappedMemoryRange mappedMemoryRange {};
            mappedMemoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            return mappedMemoryRange;
        }

#pragma region Image
        
        VkImageCreateInfo ImageCreateInfo()
        {
            VkImageCreateInfo imageCreateInfo {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            return imageCreateInfo;
        }

        VkSamplerCreateInfo SamplerCreateInfo()
        {
            VkSamplerCreateInfo samplerCreateInfo {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            return samplerCreateInfo;
        }

        VkImageViewCreateInfo ImageViewCreateInfo()
        {
            VkImageViewCreateInfo imageViewCreateInfo {};
            imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            return imageViewCreateInfo;
        }

        /** @brief Initialize an image memory barrier with no image transfer ownership */
        VkImageMemoryBarrier ImageMemoryBarrier()
        {
            VkImageMemoryBarrier imageMemoryBarrier {};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            return imageMemoryBarrier;
        }
        
#pragma endregion Image

#pragma region CommandBuffer

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool commandPool, VkCommandBufferLevel level, uint32_t bufferCount)
        {
            VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandPool = commandPool;
            commandBufferAllocateInfo.level = level;
            commandBufferAllocateInfo.commandBufferCount = bufferCount;
            return commandBufferAllocateInfo;
        }

        VkCommandBufferBeginInfo CommandBufferBeginInfo()
        {
            VkCommandBufferBeginInfo cmdBufferBeginInfo {};
            cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            return cmdBufferBeginInfo;
        }
        
#pragma endregion CommnandBuffer
    }    
}
