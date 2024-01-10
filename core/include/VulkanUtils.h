#pragma once
#include <vulkan/vulkan_core.h>
#include <VulkanTexture.h>
#include <glm/vec4.hpp>

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
// Custom define for better code readability
constexpr int VK_FLAGS_NONE = 0;
// Default fence timeout in nanoseconds
constexpr long long DEFAULT_FENCE_TIMEOUT = 100000000000;

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

    	bool HasDepth(VkFormat format);

    	bool HasStencil(VkFormat format);

    	bool IsDepthStencil(VkFormat format);

        Texture2D* CreateDefaultTexture2D(VulkanDevice* vulkanDevice, VkQueue transferQueue, uint32_t width, uint32_t height, glm::vec4 clearColor = glm::vec4(1.0f));
    }    
}
