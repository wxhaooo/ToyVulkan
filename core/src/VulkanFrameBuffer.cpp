#include <VulkanFrameBuffer.h>

namespace vks
{
	bool FramebufferAttachment::HasDepth()
	{
		std::vector<VkFormat> formats = 
		{
			VK_FORMAT_D16_UNORM,
			VK_FORMAT_X8_D24_UNORM_PACK32,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	bool FramebufferAttachment::HasStencil()
	{
		std::vector<VkFormat> formats = 
		{
			VK_FORMAT_S8_UINT,
			VK_FORMAT_D16_UNORM_S8_UINT,
			VK_FORMAT_D24_UNORM_S8_UINT,
			VK_FORMAT_D32_SFLOAT_S8_UINT,
		};
		return std::find(formats.begin(), formats.end(), format) != std::end(formats);
	}

	bool FramebufferAttachment::IsDepthStencil()
	{
		return(HasDepth() || HasStencil());
	}

	FrameBuffer::FrameBuffer(VulkanDevice* device):vulkanDevice(device)
	{
		
	}

	FrameBuffer::~FrameBuffer()
	{
		assert(vulkanDevice);

		if(descriptorSetLayout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice,descriptorSetLayout,nullptr);

		if (descriptorPool != VK_NULL_HANDLE)
			vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
		
		for (auto attachment : attachments)
		{
			vkDestroyImage(vulkanDevice->logicalDevice, attachment.image, nullptr);
			vkDestroyImageView(vulkanDevice->logicalDevice, attachment.view, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, attachment.memory, nullptr);
		}

		vkDestroyFramebuffer(vulkanDevice->logicalDevice, frameBuffer, nullptr);
	}

	uint32_t FrameBuffer::AddAttachment(vks::AttachmentCreateInfo createInfo)
	{
		vks::FramebufferAttachment attachment;
		
		attachment.name = createInfo.name;
		attachment.binding = createInfo.binding;
		attachment.format = createInfo.format;

		VkImageAspectFlags aspectMask = VK_FLAGS_NONE;

		// Select aspect mask and layout depending on usage

		// Color attachment
		if (createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

		// Depth (and/or stencil) attachment
		if (createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			if (attachment.HasDepth())
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (attachment.HasStencil())
				aspectMask = aspectMask | VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::ImageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = createInfo.format;
		image.extent.width = createInfo.width;
		image.extent.height = createInfo.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = createInfo.layerCount;
		image.samples = createInfo.imageSampleCount;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = createInfo.usage;

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
		attachment.subresourceRange.layerCount = createInfo.layerCount;

		VkImageViewCreateInfo imageView = vks::initializers::ImageViewCreateInfo();
		imageView.viewType = (createInfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageView.format = createInfo.format;
		imageView.subresourceRange = attachment.subresourceRange;
		//todo: workaround for depth+stencil attachments
		imageView.subresourceRange.aspectMask = (attachment.HasDepth()) ? VK_IMAGE_ASPECT_DEPTH_BIT : aspectMask;
		imageView.image = attachment.image;
		CheckVulkanResult(vkCreateImageView(vulkanDevice->logicalDevice, &imageView, nullptr, &attachment.view));

		// Fill attachment description
		attachment.description = {};
		attachment.description.samples = createInfo.imageSampleCount;
		attachment.description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.description.storeOp = (createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.description.format = createInfo.format;
		attachment.description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		// Final layout
		// If not, final layout depends on attachment type
		if (attachment.HasDepth() || attachment.HasStencil())
			attachment.description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		else
			attachment.description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		
		attachments.push_back(attachment);

		return static_cast<uint32_t>(attachments.size() - 1);
	}

	void FrameBuffer::CreateDescriptorSet(VkSampler sampler)
	{
		uint32_t attachmentSize = static_cast<uint32_t>(attachments.size());
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(
			poolSizes, attachmentSize + 5);
		CheckVulkanResult(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBinding =
		{
			vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
			setLayoutBinding.data(), setLayoutBinding.size());
		CheckVulkanResult(
			vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

		// Descriptor set for scene matrices
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
			descriptorPool, &descriptorSetLayout, 1);
		
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

	FrameBuffer* VulkanFrameBuffer::GetFrameBuffer(uint32_t frameBufferIndex) const
	{
		if(frameBufferIndex < 0 || frameBufferIndex >= frameBufferCount) return nullptr;	
		return frameBuffers[frameBufferIndex];
	}

	void VulkanFrameBuffer::AddAttachment(vks::AttachmentCreateInfo createInfo)
	{
		for(uint32_t i=0; i < frameBufferCount; i++)
			frameBuffers[i]->AddAttachment(createInfo);
	}
	
	VkResult VulkanFrameBuffer::CreateSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode)
    {
    	VkSamplerCreateInfo samplerInfo = vks::initializers::SamplerCreateInfo();
    	samplerInfo.magFilter = magFilter;
    	samplerInfo.minFilter = minFilter;
    	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    	samplerInfo.addressModeU = addressMode;
    	samplerInfo.addressModeV = addressMode;
    	samplerInfo.addressModeW = addressMode;
    	samplerInfo.mipLodBias = 0.0f;
    	samplerInfo.maxAnisotropy = 1.0f;
    	samplerInfo.minLod = 0.0f;
    	samplerInfo.maxLod = 1.0f;
    	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    	return vkCreateSampler(vulkanDevice->logicalDevice, &samplerInfo, nullptr, &sampler);
    }

	void VulkanFrameBuffer::CrateDescriptorSet()
	{
		for(uint32_t i =0; i<frameBufferCount;i++)
			frameBuffers[i]->CreateDescriptorSet(sampler);
	}

}
