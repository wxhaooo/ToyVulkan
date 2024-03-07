/*
* Vulkan framebuffer class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>
#include <vulkan/vulkan.h>
#include <VulkanDevice.h>
#include <VulkanHelper.h>
#include <VulkanUtils.h>
#include <VulkanInitializers.h>

namespace vks
{
    class VulkanAttachmentDescription;
}

namespace vks
{
    class VulkanRenderPass;
}

namespace vks
{
    /**
    * @brief Encapsulates a single frame buffer attachment 
    */
    struct FramebufferAttachment
    {
        std::string name;
        uint32_t binding;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format;
        VkImageSubresourceRange subresourceRange;
        VkAttachmentDescription description;

        VkDescriptorImageInfo descriptor;
        VkDescriptorSet descriptorSet;

        uint32_t refCount = 0;

        /**
        * @brief Returns true if the attachment has a depth component
        */
        bool HasDepth();

        /**
        * @brief Returns true if the attachment has a stencil component
        */
        bool HasStencil();

        /**
        * @brief Returns true if the attachment is a depth and/or stencil attachment
        */
        bool IsDepthStencil();

        bool IsGBuffer();
    };

    /**
   * @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
   */
    struct FrameBuffer
    {
        VulkanDevice* vulkanDevice = nullptr;
        uint32_t width = 0, height = 0;
        VkFramebuffer frameBuffer = VK_NULL_HANDLE;
        std::vector<FramebufferAttachment> attachments;

        VkDescriptorSetLayout attachmentDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorPool attachmentDescriptorPool = VK_NULL_HANDLE;

        /**
       * Default constructor
       *
       * @param device Pointer to a valid VulkanDevice
       */
        FrameBuffer(VulkanDevice* device);
        
        /**
       * Destroy and free Vulkan resources used for the framebuffer and all of its attachments
       */
        ~FrameBuffer();

        uint32_t CreateAttachment(const VulkanAttachmentDescription* attachmentDescription);

        const FramebufferAttachment& GetAttachment(const std::string& attachmentName);

        void CreateAttachmentDescriptorSet(VkSampler sampler, bool skipDepthStencil = true);

        void Destory();
    };
   
    class VulkanFrameBuffer
    {
    public:
        std::vector<FrameBuffer*> frameBuffers;
        uint32_t frameBufferCount = 0;
        
        VulkanFrameBuffer(vks::VulkanDevice* vulkanDevice, uint32_t width, uint32_t height, uint32_t frameBufferCount);
        ~VulkanFrameBuffer();

        void Init(VulkanRenderPass* renderPass, const utils::VulkanSamplerCreateInfo& samplerCreateInfo);

        uint32_t Width() const {return width;}
        uint32_t Height() const {return height;}

        FrameBuffer* GetFrameBuffer(uint32_t frameBufferIndex) const;
        
        void CrateDescriptorSet(bool skipDepthStencil = true);

    private:
        vks::VulkanDevice* vulkanDevice = nullptr;
        uint32_t width = 0, height = 0;
        VulkanRenderPass* renderPass = nullptr;

        // extra info
        VkSampler sampler = VK_NULL_HANDLE;

        void CreateAttachment(const VulkanAttachmentDescription* attachmentDescription);
    };
}
