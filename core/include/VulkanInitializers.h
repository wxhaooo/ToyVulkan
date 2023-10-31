#pragma once
#include <vulkan/vulkan_core.h>

namespace vks
{
    namespace initializers
    {
        VkSemaphoreCreateInfo semaphoreCreateInfo();
        VkFenceCreateInfo fenceCreateInfo(VkFenceCreateFlags flags = 0);
        VkSubmitInfo submitInfo();
    }    
}
