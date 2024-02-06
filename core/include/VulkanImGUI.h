#pragma once
#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <glm/glm.hpp>
#include <vulkanDevice.h>
#include <VulkanSwapChain.h>
#include <VulkanTexture.h>

struct ImGUICreateInfo
{
	VkInstance instance = VK_NULL_HANDLE;
	vks::VulkanDevice* vulkanDevice = nullptr;
	VulkanSwapChain* vulkanSwapChain = nullptr;
	VkQueue copyQueue = VK_NULL_HANDLE;
	GLFWwindow* glfwWindow = nullptr;
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
	
	// void NewFrame();
	void UpdateBuffer();
	void DrawFrame(VkCommandBuffer commandBuffer);
	void Resize(uint32_t width, uint32_t height);
	
	VkRenderPass renderPass = VK_NULL_HANDLE;
	
private:

	void InitVulkanResource();

	void PrepareFontTexture();
	void CreatePipelineCache();
	void PrepareShaders();
	void PrepareRenderPass();
	void PreparePipelines();
	void SetupDescriptors();

	void CleanUpVulkanResource();
	void InitImGUIResource();

	// style
	void SwitchToUnrealEngineStyle();
	void SwitchToLightStyle();
	
	int32_t vertexCount = 0;
	vks::Buffer vertexBuffer;
	
	int32_t indexCount = 0;
	vks::Buffer indexBuffer;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	vks::Texture2D fontImageTexture;
	
	ImGuiStyle vulkanStyle;
	VkPhysicalDeviceDriverProperties driverProperties = {};

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;
	VkFormat depthFormat;

	// external resource
	VkInstance instance;
	vks::VulkanDevice* vulkanDevice;
	VulkanSwapChain* vulkanSwapChain;
	VkQueue copyQueue;
	
	GLFWwindow* glfwWindow;
};
