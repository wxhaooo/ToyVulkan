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
    /**
    * @brief Encapsulates a single frame buffer attachment 
    */
    struct FramebufferAttachment
    {
        std::string name;
        uint32_t binding;
        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
        VkFormat format;
        VkImageSubresourceRange subresourceRange;
        VkAttachmentDescription description;

        VkDescriptorImageInfo descriptor;
        VkDescriptorSet descriptorSet;

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
    };

    /**
   * @brief Encapsulates a complete Vulkan framebuffer with an arbitrary number and combination of attachments
   */
    struct FrameBuffer
    {
        VulkanDevice* vulkanDevice;
        uint32_t width, height;
        VkFramebuffer frameBuffer;
        std::vector<FramebufferAttachment> attachments;

        VkDescriptorSetLayout attachmentDescriptorSetLayout;
        VkDescriptorPool attachmentDescriptorPool;

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

        /**
       * Add a new attachment described by createinfo to the framebuffer's attachment list
       *
       * @param createInfo Structure that specifies the framebuffer to be constructed
       *
       * @return Index of the new attachment
       */
        uint32_t AddAttachment(vks::AttachmentCreateInfo createInfo);

        void CreateAttachmentDescriptorSet(VkSampler sampler);

        // void CreateFrameBufferDescriptorSet(VkSampler sampler);
    };

   
    class VulkanFrameBuffer
    {
    private:
        vks::VulkanDevice* vulkanDevice;
        uint32_t width, height;

    public:
        std::vector<FrameBuffer*> frameBuffers;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t frameBufferCount = 0;
        
        VulkanFrameBuffer(vks::VulkanDevice* vulkanDevice,uint32_t width, uint32_t height, uint32_t frameBufferCount);
        ~VulkanFrameBuffer();

        uint32_t Width() const {return width;}
        uint32_t Height() const {return height;}

        FrameBuffer* GetFrameBuffer(uint32_t frameBufferIndex) const;

        void AddAttachment(vks::AttachmentCreateInfo createInfo);

        /**
        * Creates a default sampler for sampling from any of the framebuffer attachments
        * Applications are free to create their own samplers for different use cases 
        *
        * @param magFilter Magnification filter for lookups
        * @param minFilter Minification filter for lookups
        * @param addressMode Addressing mode for the U,V and W coordinates
        *
        * @return VkResult for the sampler creation
        */
        VkResult CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode);

        void CrateDescriptorSet();
    };
}
