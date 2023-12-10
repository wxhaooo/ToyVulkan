#pragma once
#include <memory>
#include <VulkanDevice.h>

namespace vks
{
    struct AttachmentCreateInfo;
    class VulkanFrameBuffer;
}

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

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass() = delete;
        VulkanRenderPass(const std::string& renderPassName, VulkanDevice* device);
        ~VulkanRenderPass();
        
        VkRenderPass renderPass;
        std::unique_ptr<VulkanFrameBuffer> vulkanFrameBuffer = nullptr;

        void Init(uint32_t width, uint32_t height, uint32_t maxFrameInFlight);

        void AddSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode);
        
        void AddAttachment(vks::AttachmentCreateInfo attachmentInfo);

        void CreateRenderPass();

        void CreateDescriptorSet();
    
    private:
        std::string name;
        VulkanDevice* vulkanDevice = nullptr;

    };
}
