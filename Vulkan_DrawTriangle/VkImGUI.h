#pragma once
#include <memory>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
// #include <windows.h>

struct ImGUICreateInfo
{
	// command pool
	VkCommandPool commandPool;

	// device
	VkPhysicalDevice physicalDevice;
	VkDevice logicalDevice;
	VkSurfaceKHR surface;

	// swap chain
	uint32_t swapChainImageWidth;
	uint32_t swapChainImageHeight;
	VkSwapchainKHR swapChain;
	VkFormat swapChainImageFormat;
	VkPresentModeKHR swapChainPresentMode;

	// render pass and queue
	VkRenderPass renderPass;
	VkQueue graphicsQueue;

};

class VkImGUI
{
public:
	VkImGUI() = delete;
	VkImGUI(ImGUICreateInfo& imGUICreateInfo)
		: commandPool(imGUICreateInfo.commandPool), swapChain(imGUICreateInfo.swapChain),
		swapChainImageWidth(imGUICreateInfo.swapChainImageWidth),
		swapChainImageHeight(imGUICreateInfo.swapChainImageHeight),
		swapChainImageFormat(imGUICreateInfo.swapChainImageFormat),
		swapChainPresentMode(imGUICreateInfo.swapChainPresentMode),
		physicalDevice(imGUICreateInfo.physicalDevice), logicalDevice(imGUICreateInfo.logicalDevice),
	    surface(imGUICreateInfo.surface),
		renderPass(imGUICreateInfo.renderPass),graphicsQueue(imGUICreateInfo.graphicsQueue)
	{
		InitImGUIResource();
		InitVulkanResource();
	}

	~VkImGUI()
	{
		ImGui::DestroyContext();

		CleanUpVulkanResource();
	}

private:

	void Render()
	{

	}

#pragma region Vulkan Resource

private:

	void InitVulkanResource()
	{
		ImGuiIO& io = ImGui::GetIO();

		// Create font texture
		unsigned char* fontData;
		int texWidth, texHeight;
		io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
		VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

		//SRS - Get Vulkan device driver information if available, use later for display
		std::set<std::string> requiredExtesions{ VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME };
		if (VkUtils::CheckDeviceExtensionSupport(physicalDevice, requiredExtesions))
		{
			VkPhysicalDeviceProperties2 deviceProperties2 = {};
			deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
			deviceProperties2.pNext = &driverProperties;
			driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
			vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);
		}

		// Create font image for copy
		VkUtils::CreateImage(physicalDevice, logicalDevice, texWidth, texHeight,
			VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, fontImage, fontMemory);

		VkUtils::CreateBuffer(physicalDevice, logicalDevice, uploadSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		CheckVulkanResult(vkMapMemory(logicalDevice, stagingBufferMemory, 0, uploadSize, 0, &data));
		memcpy(data, fontData, uploadSize);
		vkUnmapMemory(logicalDevice, stagingBufferMemory);

		VkUtils::TransitionImageLayout(logicalDevice, commandPool, graphicsQueue, fontImage,
			VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_HOST_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		VkUtils::CopyBufferToImage(logicalDevice, commandPool, graphicsQueue,
			stagingBuffer, fontImage, texWidth, texHeight);

		VkUtils::TransitionImageLayout(logicalDevice, commandPool, graphicsQueue, fontImage,
			VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

		vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
		vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

		// // Font texture Sampler
		// VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		// samplerInfo.magFilter = VK_FILTER_LINEAR;
		// samplerInfo.minFilter = VK_FILTER_LINEAR;
		// samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		// samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		// samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		// samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		// samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		// VK_CHECK_RESULT(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &sampler));
		//
		// // Descriptor pool
		// std::vector<VkDescriptorPoolSize> poolSizes = {
		// 	vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		// };
		// VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 2);
		// VK_CHECK_RESULT(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
		//
		// // Descriptor set layout
		// std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// 	vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		// };
		// VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		// VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));
		//
		// // Descriptor set
		// VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		// VK_CHECK_RESULT(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));
		// VkDescriptorImageInfo fontDescriptor = vks::initializers::descriptorImageInfo(
		// 	sampler,
		// 	fontView,
		// 	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		// );
		// std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// 	vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
		// };
		// vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		//
		// // Pipeline cache
		// VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		// pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		// VK_CHECK_RESULT(vkCreatePipelineCache(device->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
		//
		// // Pipeline layout
		// // Push constants for UI rendering parameters
		// VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
		// VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		// pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		// pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		// VK_CHECK_RESULT(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		//
		// // Setup graphics pipeline for UI rendering
		// VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		// 	vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		//
		// VkPipelineRasterizationStateCreateInfo rasterizationState =
		// 	vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		//
		// // Enable blending
		// VkPipelineColorBlendAttachmentState blendAttachmentState{};
		// blendAttachmentState.blendEnable = VK_TRUE;
		// blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		// blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		// blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		// blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		// blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		// blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		// blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		//
		// VkPipelineColorBlendStateCreateInfo colorBlendState =
		// 	vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		//
		// VkPipelineDepthStencilStateCreateInfo depthStencilState =
		// 	vks::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		//
		// VkPipelineViewportStateCreateInfo viewportState =
		// 	vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		//
		// VkPipelineMultisampleStateCreateInfo multisampleState =
		// 	vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		//
		// std::vector<VkDynamicState> dynamicStateEnables = {
		// 	VK_DYNAMIC_STATE_VIEWPORT,
		// 	VK_DYNAMIC_STATE_SCISSOR
		// };
		// VkPipelineDynamicStateCreateInfo dynamicState =
		// 	vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		//
		// std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
		//
		// VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
		//
		// pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		// pipelineCreateInfo.pRasterizationState = &rasterizationState;
		// pipelineCreateInfo.pColorBlendState = &colorBlendState;
		// pipelineCreateInfo.pMultisampleState = &multisampleState;
		// pipelineCreateInfo.pViewportState = &viewportState;
		// pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		// pipelineCreateInfo.pDynamicState = &dynamicState;
		// pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		// pipelineCreateInfo.pStages = shaderStages.data();
		//
		// // Vertex bindings an attributes based on ImGui vertex definition
		// std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		// 	vks::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
		// };
		// std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		// 	vks::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
		// 	vks::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
		// 	vks::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
		// };
		// VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		// vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		// vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		// vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		// vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
		//
		// pipelineCreateInfo.pVertexInputState = &vertexInputState;
		//
		// shaderStages[0] = example->loadShader(shadersPath + "imgui/ui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		// shaderStages[1] = example->loadShader(shadersPath + "imgui/ui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		//
		// VK_CHECK_RESULT(vkCreateGraphicsPipelines(device->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
	}

	void CleanUpVulkanResource()
	{
		vkDestroyImage(logicalDevice, fontImage, nullptr);
		vkDestroyImageView(logicalDevice, fontView, nullptr);
		vkFreeMemory(logicalDevice, fontMemory, nullptr);
	}


private:

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
	int32_t vertexCount = 0;
	int32_t indexCount = 0;
	VkSampler sampler;

#pragma endregion Vulkan Resource

#pragma region ImGUI Resource

private:
	void InitImGUIResource()
	{
		ImGui::CreateContext();

		// style
		vulkanStyle = ImGui::GetStyle();
		vulkanStyle.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
		vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
		vulkanStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		vulkanStyle.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
		vulkanStyle.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

		ImGuiStyle& style = ImGui::GetStyle();
		style = vulkanStyle;

		// Dimensions
		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize = ImVec2(swapChainImageWidth, swapChainImageHeight);
		io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
// #if defined(_WIN32)
// 		// If we directly work with os specific key codes, we need to map special key types like tab
// 		io.KeyMap[ImGuiKey_Tab] = VK_TAB;
// 		io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
// 		io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
// 		io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
// 		io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
// 		io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
// 		io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
// 		io.KeyMap[ImGuiKey_Space] = VK_SPACE;
// 		io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
// #endif
	}

private:
	ImGuiStyle vulkanStyle;
	VkPhysicalDeviceDriverProperties driverProperties = {};

#pragma endregion ImGUI Resource

#pragma region ImageView

private:

private:

	std::vector<VkImage> images;
	std::vector<VkImageView> imageViews;
	std::vector<VkFramebuffer> frameBuffers;

#pragma endregion ImageView

	// external resource
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
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surfaceFormat;

	// command
	VkCommandPool commandPool;

	// render pass and queue
	VkRenderPass renderPass;
	VkQueue graphicsQueue;
};
