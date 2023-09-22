#pragma once
#include <memory>
#include <vulkan/vulkan_core.h>

#include "DrawTraiangleVkApplication.h"

struct ImGUICreateInfo
{
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;
	VkSurfaceKHR surface;

	uint32_t swapChainImageWidth;
	uint32_t swapChainImageHeight;
	VkSwapchainKHR swapChain;
	VkFormat swapChainImageFormat;
	VkPresentModeKHR swapChainPresentMode;
};

class VkImGUI
{
public:
	VkImGUI() = delete;
	VkImGUI(ImGUICreateInfo& imGUICreateInfo)
		:swapChain(imGUICreateInfo.swapChain),
		swapChainImageWidth(imGUICreateInfo.swapChainImageWidth),
		swapChainImageHeight(imGUICreateInfo.swapChainImageHeight),
		swapChainImageFormat(imGUICreateInfo.swapChainImageFormat),
		swapChainPresentMode(imGUICreateInfo.swapChainPresentMode),
		physicalDevice(imGUICreateInfo.physicalDevice), logicalDevice(imGUICreateInfo.logicalDevice),
		graphicsQueue(imGUICreateInfo.graphicsQueue), surface(imGUICreateInfo.surface)
	{
		Init();
	}

	~VkImGUI()
	{
		CleanUp();
	}

private:
	void Init()
	{
		CreateDescriptorPool();
	}

	void CleanUp()
	{
		vkDestroyDescriptorPool(logicalDevice, imGuiDescriptorPool, nullptr);
	}

	void Render()
	{

	}

	void CreateDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 1> pool_sizes =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VkUtils::MaxFrameInFlight },
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
		pool_info.pPoolSizes = pool_sizes.data();
		pool_info.maxSets = VkUtils::MaxFrameInFlight;

		CheckVulkanResult(vkCreateDescriptorPool(logicalDevice, &pool_info, nullptr, &imGuiDescriptorPool));
	}

private:
	// swap chain
	VkSwapchainKHR swapChain;
	uint32_t swapChainImageWidth;
	uint32_t swapChainImageHeight;
	VkFormat swapChainImageFormat;
	VkPresentModeKHR swapChainPresentMode;

	// device
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkQueue graphicsQueue;

	VkSurfaceKHR surface;

	VkSurfaceFormatKHR surfaceFormat;

	// self data
	VkCommandPool commandPool;
	VkDescriptorPool imGuiDescriptorPool;
};
