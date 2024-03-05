#include <VulkanFrameBuffer.h>
#include <VulkanRenderPass.h>

namespace vks
{
	bool FramebufferAttachment::HasDepth()
	{
		return utils::HasDepth(format);
	}

	bool FramebufferAttachment::HasStencil()
	{
		return utils::HasStencil(format);
	}

	bool FramebufferAttachment::IsDepthStencil()
	{
		return(utils::HasDepth(format) || utils::HasStencil(format));
	}

    bool FramebufferAttachment::IsGBuffer()
    {
        return name.find("G_") != std::string::npos;
    }

	FrameBuffer::FrameBuffer(VulkanDevice* device):vulkanDevice(device)
	{
		
	}

	FrameBuffer::~FrameBuffer()
	{
        Destory();
	}

    void FrameBuffer::Destory() {
        assert(vulkanDevice);

        if(attachmentDescriptorSetLayout != VK_NULL_HANDLE)
            vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice,attachmentDescriptorSetLayout,nullptr);

        if (attachmentDescriptorPool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(vulkanDevice->logicalDevice, attachmentDescriptorPool, nullptr);

        for (auto attachment : attachments)
        {
            if(attachment.image != VK_NULL_HANDLE)
                vkDestroyImage(vulkanDevice->logicalDevice, attachment.image, nullptr);

            if(attachment.view != VK_NULL_HANDLE)
                vkDestroyImageView(vulkanDevice->logicalDevice, attachment.view, nullptr);

            if(attachment.memory != VK_NULL_HANDLE)
                vkFreeMemory(vulkanDevice->logicalDevice, attachment.memory, nullptr);
        }

        if(frameBuffer != VK_NULL_HANDLE)
            vkDestroyFramebuffer(vulkanDevice->logicalDevice, frameBuffer, nullptr);
    }

	uint32_t FrameBuffer::CreateAttachment(const VulkanAttachmentDescription* attachmentDescription)
	{
		FramebufferAttachment attachment;
		
		attachment.name = attachmentDescription->name;
		attachment.binding = attachmentDescription->binding;
		attachment.format = attachmentDescription->format;

		VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

		// Select aspect mask and layout depending on usage

		// Color attachment
		if (attachmentDescription->usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		// Depth (and/or stencil) attachment
		if (attachmentDescription->usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (attachment.HasDepth())
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (attachment.HasStencil())
				aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::ImageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = attachmentDescription->format;
		image.extent.width = attachmentDescription->width;
		image.extent.height = attachmentDescription->height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = attachmentDescription->layerCount;
		image.samples = attachmentDescription->imageSampleCount;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = attachmentDescription->usage;

		VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
		VkMemoryRequirements memReqs;

		// Create image for this attachment
		CheckVulkanResult(vkCreateImage(vulkanDevice->logicalDevice, &image, nullptr, &attachment.image));
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVulkanResult(vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &attachment.memory));
		CheckVulkanResult(vkBindImageMemory(vulkanDevice->logicalDevice, attachment.image, attachment.memory, 0));

		attachment.subresourceRange = {};
		attachment.subresourceRange.aspectMask = aspectMask;
		attachment.subresourceRange.levelCount = 1;
		attachment.subresourceRange.layerCount = attachmentDescription->layerCount;

		VkImageViewCreateInfo imageView = vks::initializers::ImageViewCreateInfo();
		imageView.viewType = (attachmentDescription->layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageView.format = attachmentDescription->format;
		imageView.subresourceRange = attachment.subresourceRange;
		//todo: workaround for depth+stencil attachments
		imageView.subresourceRange.aspectMask = (attachment.HasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
		imageView.image = attachment.image;
		CheckVulkanResult(vkCreateImageView(vulkanDevice->logicalDevice, &imageView, nullptr, &attachment.view));

		// Fill attachment description
		attachment.description = attachmentDescription->description;
		attachments.push_back(attachment);

		return static_cast<uint32_t>(attachments.size() - 1);
	}

    const FramebufferAttachment& FrameBuffer::GetAttachment(const std::string &attachmentName)
    {
        auto res = std::find_if(attachments.begin(), attachments.end(),[&](const FramebufferAttachment& framebufferAttachment)
        {
           return framebufferAttachment.name == attachmentName;
        });
        return *res;
    }

	void FrameBuffer::CreateAttachmentDescriptorSet(VkSampler sampler)
	{
		uint32_t attachmentSize = static_cast<uint32_t>(attachments.size());
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(
			poolSizes, attachmentSize + 5);
		CheckVulkanResult(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &attachmentDescriptorPool));
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBinding =
		{
			vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
			setLayoutBinding.data(), setLayoutBinding.size());
		CheckVulkanResult(
			vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &attachmentDescriptorSetLayout));

		// Descriptor set for scene matrices
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
			attachmentDescriptorPool, &attachmentDescriptorSetLayout, 1);
		
		for(uint32_t i = 0; i < attachments.size(); i++)
		{
			// create descriptor
			FramebufferAttachment& attachment = attachments[i];

			if(attachment.HasDepth() || attachment.HasStencil()) continue;

			attachment.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachment.descriptor.imageView = attachment.view;
			attachment.descriptor.sampler = sampler;

			CheckVulkanResult(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &attachment.descriptorSet));
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(attachment.descriptorSet,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &attachment.descriptor);
			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, 1, &writeDescriptorSet, 0, nullptr);
		}
	}
	
    VulkanFrameBuffer::VulkanFrameBuffer(vks::VulkanDevice* vulkanDevice, uint32_t width, uint32_t height, uint32_t frameBufferCount)
    {
        assert(vulkanDevice);
        this->vulkanDevice = vulkanDevice;
    	this->frameBufferCount = frameBufferCount;
		this->width = width;
		this->height = height;

		frameBuffers.resize(frameBufferCount);
		for(uint32_t i=0; i < frameBufferCount; i++)
		{
			frameBuffers[i] = new FrameBuffer(vulkanDevice);
			frameBuffers[i]->width = width;
			frameBuffers[i]->height = height;
		}
    }

    VulkanFrameBuffer::~VulkanFrameBuffer()
    {
		vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
		for(uint32_t i=0; i < frameBufferCount; i++)
			delete frameBuffers[i];
		
		frameBuffers.clear();
    }

	void VulkanFrameBuffer::Init(VulkanRenderPass* vulkanRenderPass)
	{
		// binding renderPass and frameBuffer
		this->renderPass = vulkanRenderPass;
		renderPass->vulkanFrameBuffer = this;

		std::vector<VulkanAttachmentDescription*>& attachmentDescriptions = renderPass->attachmentDescriptions;
		
		// create frame buffer
		for (uint32_t i = 0; i < frameBufferCount; i++)
		{
			FrameBuffer* frameBuffer = GetFrameBuffer(i);

			// create instanced attachment
			for(auto& attachmentDescription:attachmentDescriptions)
				frameBuffer->CreateAttachment(attachmentDescription);
			
			std::vector<VkImageView> attachmentViews;
			for (auto attachment : frameBuffer->attachments)
				attachmentViews.push_back(attachment.view);

			// Find max number of layers across attachments
			uint32_t maxLayers = 0;
			for (auto attachment : frameBuffer->attachments)
			{
				if (attachment.subresourceRange.layerCount > maxLayers)
					maxLayers = attachment.subresourceRange.layerCount;
			}

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass->renderPass;
			framebufferInfo.pAttachments = attachmentViews.data();
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
			framebufferInfo.width = width;
			framebufferInfo.height = height;
			framebufferInfo.layers = maxLayers;

			FrameBuffer* currentFrameBuffer = GetFrameBuffer(i);
			CheckVulkanResult(vkCreateFramebuffer(vulkanDevice->logicalDevice,
				&framebufferInfo, nullptr, &currentFrameBuffer->frameBuffer));
		}
	}

	FrameBuffer* VulkanFrameBuffer::GetFrameBuffer(uint32_t frameBufferIndex) const
	{
		if(frameBufferIndex < 0 || frameBufferIndex >= frameBufferCount) return nullptr;	
		return frameBuffers[frameBufferIndex];
	}

	void VulkanFrameBuffer::CreateAttachment(const VulkanAttachmentDescription* attachmentDescription)
	{
		for(uint32_t i = 0; i < frameBufferCount; i++)
			frameBuffers[i]->CreateAttachment(attachmentDescription);
	}

	void VulkanFrameBuffer::CrateDescriptorSet(const utils::VulkanSamplerCreateInfo& samplerCreateInfo)
	{
		sampler = CreateSampler(vulkanDevice, samplerCreateInfo);
		for(uint32_t i =0; i<frameBufferCount;i++)
			frameBuffers[i]->CreateAttachmentDescriptorSet(sampler);
	}

}
