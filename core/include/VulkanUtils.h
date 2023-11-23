#pragma once
#include <vulkan/vulkan_core.h>

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
// Custom define for better code readability
constexpr int VK_FLAGS_NONE = 0;
// Default fence timeout in nanoseconds
constexpr long long DEFAULT_FENCE_TIMEOUT = 100000000000;

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

		~OffscreenPass()
		{
			if(renderPass != VK_NULL_HANDLE)
				vkDestroyRenderPass(device,renderPass,nullptr);

			DestroyResource();
		}

		void DestroyResource()
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
	};	
}

namespace vks
{
    namespace utils
    {
        VkBool32 GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);
        VkBool32 GetSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat* depthStencilFormat);

    	// Create an image memory barrier for changing the layout of an image and put it into an active command buffer,
    	// See chapter 11.4 "Image Layout" for details
    	// Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    	void SetImageLayout(VkCommandBuffer cmdbuffer,VkImage image,VkImageLayout oldImageLayout,VkImageLayout newImageLayout,
			VkImageSubresourceRange subresourceRange,VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    	// Uses a fixed sub resource layout with first mip level and layer
    	void SetImageLayout(VkCommandBuffer cmdbuffer,VkImage image,VkImageAspectFlags aspectMask,VkImageLayout oldImageLayout,
			VkImageLayout newImageLayout,VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    	VkShaderModule LoadShader(const char *fileName, VkDevice device);
    }    
}
