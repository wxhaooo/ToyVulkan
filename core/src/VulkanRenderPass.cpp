#include <VulkanRenderPass.h>

namespace vks
{
    OffscreenPass::~OffscreenPass()
    {
        if(renderPass != VK_NULL_HANDLE)
            vkDestroyRenderPass(device,renderPass,nullptr);

        DestroyResource(); 
    }

    void OffscreenPass::DestroyResource()
    {
        {
            for(uint32_t i = 0;i < frameBuffer.size(); i++)
            {
                vkDestroySampler(device,sampler[i],nullptr);
                vkDestroyFramebuffer(device,frameBuffer[i],nullptr);

                vkDestroyImage(device,color[i].image,nullptr);
                vkDestroyImageView(device,color[i].view,nullptr);
                vkFreeMemory(device,color[i].mem,nullptr);
                
                vkDestroyImage(device,depth[i].image,nullptr);
                vkDestroyImageView(device,depth[i].view,nullptr);
                vkFreeMemory(device,depth[i].mem,nullptr);
            }

            vkDestroyDescriptorPool(device,descriptorPool,nullptr);
            vkDestroyDescriptorSetLayout(device,descriptorSetLayout,nullptr);
        }
    }
}
