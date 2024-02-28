#pragma once
#include <VulkanDevice.h>
#include <VulkanFrameBuffer.h>
#include <map>
#include <memory>

namespace vks {
    class VulkanFrameBuffer;
} // namespace vks

namespace vks {
// for loadGLTF.cpp
    struct OffscreenPass {
        VkDevice device = VK_NULL_HANDLE;
        int32_t width, height;
        VkRenderPass renderPass;
        std::vector<VkFramebuffer> frameBuffer;
        std::vector<vks::FramebufferAttachment> color, depth;
        std::vector<VkSampler> sampler;
        std::vector<VkDescriptorImageInfo> descriptor;

        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorPool descriptorPool;
        std::vector<VkDescriptorSet> descriptorSet;

        ~OffscreenPass();
        void DestroyResource();
    };

/**
 * @brief Describes the attributes of an attachment to be created
 */
    struct AttachmentCreateInfo
    {
        std::string name;
        uint32_t binding;
        uint32_t width, height;
        uint32_t layerCount;
        VkFormat format;
        VkImageUsageFlags usage;
        VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
//        VkImageLayout finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    };

    class VulkanAttachmentDescription
    {
    public:
        VulkanAttachmentDescription() = delete;
        VulkanAttachmentDescription(const AttachmentCreateInfo& attachmentCreateInfo);

        std::string name;
        uint32_t binding;
        VkFormat format;
        uint32_t width, height, layerCount;
        VkSampleCountFlagBits imageSampleCount = VK_SAMPLE_COUNT_1_BIT;
        VkImageUsageFlags usage;
        VkAttachmentDescription description;
    };

/* general render pass and subPass */
    class VulkanSubPass {
    public:
        VulkanSubPass() = delete;
        VulkanSubPass(const std::string &subPassName,
                      VkPipelineBindPoint subPassBindPoint, VulkanDevice *device);

        void AddAttachments(const std::vector<VulkanAttachmentDescription*>& attachmentDescriptions,
                            const std::vector<uint32_t>& colorAttachmentIndices,
                            const std::vector<uint32_t>& inputAttachmentIndices);
        void CreateDescription();
        VkSubpassDescription& GetDescription();

    private:
        std::vector<VkAttachmentReference> inputReferences;
        std::vector<VkAttachmentReference> colorReferences;
        VkAttachmentReference depthReference = {};

        bool hasDepth = false;
        bool hasColor = false;
        bool hasInput = false;

        VkPipelineBindPoint bindPoint;
        VkSubpassDescription description;
        VulkanDevice *vulkanDevice = nullptr;
        std::string name;
    };

    class VulkanRenderPass {
    public:
        VulkanRenderPass() = delete;
        VulkanRenderPass(const std::string &renderPassName, VulkanDevice *device);
        ~VulkanRenderPass();

        VkRenderPass renderPass;

        // frame buffer
        VulkanFrameBuffer* vulkanFrameBuffer = nullptr;

        // attachment create info
        std::map<std::string, std::unique_ptr<VulkanAttachmentDescription>> attachmentName2DescriptionMap;
        std::vector<VulkanAttachmentDescription*> attachmentDescriptions;

        // subPass info
        std::vector<VkSubpassDependency> subPassDependencies;
        std::vector<VulkanSubPass *> subPassArray;
        std::map<std::string, std::unique_ptr<VulkanSubPass>> subPassName2InstMap;

        void Destroy();

        void AddAttachment(const vks::AttachmentCreateInfo &createInfo);

        void AddSubPass(const std::string &subPassName,
                        VkPipelineBindPoint subPassBindPoint,
                        const std::vector<std::string> & colorAttachmentNames,
                        const std::vector<std::string> & inputAttachmentNames = {});

        void AddSubPass(const std::string &subPassName,
                        VkPipelineBindPoint subPassBindPoint,
                        const std::vector<uint32_t> &colorAttachmentIndices,
                        const std::vector<uint32_t> &inputAttachmentIndices = {});

        void AddSubPassDependency(std::vector<VkSubpassDependency> subPassDependency);

        void Init(bool isDefault = false);

        int AttachmentCount();

        int ColorAttachmentCount();

    private:

        // all attachment and single subPass
        void CreateDefaultRenderPass();

        void CreateCustomRenderPass();

        std::string name;
        int attachmentCount = 0;
        int colorAttachmentCount = 0;
        VulkanDevice *vulkanDevice = nullptr;
    };
} // namespace vks