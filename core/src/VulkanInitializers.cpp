#include <VulkanInitializers.h>

namespace vks
{
    namespace initializers
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo()
        {
            VkSemaphoreCreateInfo semaphoreCreateInfo {};
            semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            return semaphoreCreateInfo;
        }

        VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags)
        {
            VkFenceCreateInfo fenceCreateInfo {};
            fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fenceCreateInfo.flags = flags;
            return fenceCreateInfo;
        }

        VkSubmitInfo submitInfo()
        {
            VkSubmitInfo submitInfo {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            return submitInfo;
        }
    }    
}
