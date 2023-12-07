#pragma once
#include <VulkanDevice.h>

namespace vks
{
    // Framebuffer for offscreen rendering
    struct FrameBufferAttachment {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    };
    
    struct OffscreenPass
    {
        VkDevice device = VK_NULL_HANDLE;
        int32_t width, height;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> frameBuffer;
        std::vector<FrameBufferAttachment> color, depth;
        std::vector<VkSampler> sampler;
        std::vector<VkDescriptorImageInfo> descriptor;

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSet;

        ~OffscreenPass();
        void DestroyResource();
    };	
}
