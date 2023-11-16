#pragma once
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <vulkanDevice.h>
#include <VulkanSwapChain.h>
#include <VulkanTexture.h>

struct ImGUICreateInfo
{
	VkInstance instance;
	vks::VulkanDevice* vulkanDevice;
	VulkanSwapChain* vulkanSwapChain;
	VkQueue copyQueue;
	VkRenderPass renderPass;

	GLFWwindow* glfwWindow;
};

class VulkanGUI
{
public:
	// UI params are set via push constants
	struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;

	VulkanGUI() = delete;
	VulkanGUI(ImGUICreateInfo& imGUICreateInfo);
	~VulkanGUI();
	
	void NewFrame();
	void UpdateBuffer();
	void DrawFrame(VkCommandBuffer commandBuffer);

private:

	void InitVulkanResource();
	void CleanUpVulkanResource();
	void InitImGUIResource();
	
	int32_t vertexCount = 0;
	vks::Buffer vertexBuffer;
	// VkBuffer vertexBuffer = VK_NULL_HANDLE;
	// VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	// void* vertexBufferMapped = nullptr;
	
	int32_t indexCount = 0;
	vks::Buffer indexBuffer;
	// VkBuffer indexBuffer = VK_NULL_HANDLE;
	// VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	// void* indexBufferMapped = nullptr;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	vks::Texture2D fontImageTexture;

	// VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	// VkImage fontImage = VK_NULL_HANDLE;
	// VkImageView fontView = VK_NULL_HANDLE;
	// VkSampler sampler;
	
	ImGuiStyle vulkanStyle;
	VkPhysicalDeviceDriverProperties driverProperties = {};

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;

	// external resource
	VkInstance instance;
	vks::VulkanDevice* vulkanDevice;
	VulkanSwapChain* vulkanSwapChain;
	VkQueue copyQueue;
	VkRenderPass renderPass;
	GLFWwindow* glfwWindow;
};
