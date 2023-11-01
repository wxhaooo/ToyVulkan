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

        VkCommandBufferAllocateInfo CommandBufferAllocateInfo(
            VkCommandPool commandPool, 
            VkCommandBufferLevel level, 
            uint32_t bufferCount)
        {
            VkCommandBufferAllocateInfo commandBufferAllocateInfo {};
            commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commandBufferAllocateInfo.commandPool = commandPool;
            commandBufferAllocateInfo.level = level;
            commandBufferAllocateInfo.commandBufferCount = bufferCount;
            return commandBufferAllocateInfo;
        }
    }    
}
