#include <array>
#include <cassert>
#include <VulkanRenderPass.h>
#include <VulkanHelper.h>

#include "VulkanFrameBuffer.h"

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

namespace vks
{
	VulkanRenderPass::VulkanRenderPass(const std::string& renderPassName, VulkanDevice* device)
	:name(renderPassName),vulkanDevice(device)
	{
		attachmentCount = 0;
		colorAttachmentCount = 0;
	}

	VulkanRenderPass::~VulkanRenderPass()
	{
		Destroy();
	}

	void VulkanRenderPass::Init(uint32_t width, uint32_t height, uint32_t maxFrameInFlight)
	{
		vulkanFrameBuffer = std::make_unique<vks::VulkanFrameBuffer>(vulkanDevice, width, height, maxFrameInFlight);
	}

	void VulkanRenderPass::Destroy()
	{
		vulkanFrameBuffer.reset();
		vkDestroyRenderPass(vulkanDevice->logicalDevice,renderPass,nullptr);
	}
	
	void VulkanRenderPass::CreateRenderPass()
	{
		FrameBuffer* firstFrameBuffer = vulkanFrameBuffer->GetFrameBuffer(0);
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		for (auto& attachment : firstFrameBuffer->attachments)
			attachmentDescriptions.push_back(attachment.description);

		// Collect attachment references
		std::vector<VkAttachmentReference> colorReferences;
		VkAttachmentReference depthReference = {};
		bool hasDepth = false; 
		bool hasColor = false;

		for (auto& attachment : firstFrameBuffer->attachments)
		{
			uint32_t attachmentIndex = attachment.binding;
			if (attachment.IsDepthStencil())
			{
				// Only one depth attachment allowed
				assert(!hasDepth);
				depthReference.attachment = attachmentIndex;
				depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				hasDepth = true;
			}
			else
			{
				colorReferences.push_back({ attachmentIndex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
				hasColor = true;
			}
			// attachmentIndex++;
		}

		// Default render pass setup uses only one subpass
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		if (hasColor)
		{
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		}
		if (hasDepth)
			subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for attachment layout transitions
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		// Create render pass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		CheckVulkanResult(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));

		for(uint32_t i=0;i<vulkanFrameBuffer->frameBufferCount;i++)
		{
			FrameBuffer* frameBuffer = vulkanFrameBuffer->GetFrameBuffer(i);

			std::vector<VkImageView> attachmentViews;
			for (auto attachment : frameBuffer->attachments)
				attachmentViews.push_back(attachment.view);

			// Find. max number of layers across attachments
			uint32_t maxLayers = 0;
			for (auto attachment : frameBuffer->attachments)
			{
				if (attachment.subresourceRange.layerCount > maxLayers)
					maxLayers = attachment.subresourceRange.layerCount;
			}

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.pAttachments = attachmentViews.data();
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
			framebufferInfo.width = vulkanFrameBuffer->Width();
			framebufferInfo.height = vulkanFrameBuffer->Height();
			framebufferInfo.layers = maxLayers;
			
			FrameBuffer* currentFrameBuffer = vulkanFrameBuffer->GetFrameBuffer(i);
			CheckVulkanResult(vkCreateFramebuffer(vulkanDevice->logicalDevice, &framebufferInfo, nullptr, &currentFrameBuffer->frameBuffer));
		}
	}

	void VulkanRenderPass::CreateDescriptorSet()
	{
		if(vulkanFrameBuffer != nullptr)
			vulkanFrameBuffer->CrateDescriptorSet();
	}

	int VulkanRenderPass::AttachmentCount()
	{
		return attachmentCount;
	}

	int VulkanRenderPass::ColorAttachmentCount()
	{
		return colorAttachmentCount;	
	}

	void VulkanRenderPass::AddAttachment(vks::AttachmentCreateInfo attachmentInfo)
	{
		vulkanFrameBuffer->AddAttachment(attachmentInfo);
		attachmentCount++;
		if(!utils::IsDepthStencil(attachmentInfo.format))
			colorAttachmentCount++;
	}

	void VulkanRenderPass::AddSampler(VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode)
	{
		// create sampler
		vulkanFrameBuffer->CreateSampler(magFilter,minFilter,addressMode);
	}

}
