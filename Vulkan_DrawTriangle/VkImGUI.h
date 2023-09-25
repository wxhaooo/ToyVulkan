#pragma once
#include <memory>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>

#include "VkUtils.h"
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
	// UI params are set via push constants
	struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;


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

public:

	void NewFrame()
	{
		ImGui::NewFrame();

		ImGui::Begin("Draw Triangle");
		bool checkBoxTest = true;
		ImGui::Checkbox("Checkbox Test", &checkBoxTest);

		ImGui::End();
		ImGui::Render();
	}

	void UpdateBuffer()
	{
		ImDrawData* imDrawData = ImGui::GetDrawData();

		// Note: Alignment is done inside buffer creation
		VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
		VkDeviceSize indexBufferSize = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

		if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
			return;
		}

		// Update buffers only if vertex or index count has been changed compared to current buffer size

		// Vertex buffer
		if ((vertexBuffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount)) {
			if (vertexBufferMapped)
			{
				vkUnmapMemory(logicalDevice, vertexBufferMemory);
				vertexBufferMapped = nullptr;
			}

			if (vertexBuffer)
				vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);

			if (vertexBufferMemory)
				vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

			VkUtils::CreateBuffer(physicalDevice, logicalDevice, vertexBufferSize,
				VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vertexBuffer, vertexBufferMemory);
			vertexCount = imDrawData->TotalVtxCount;
			CheckVulkanResult(vkMapMemory(logicalDevice, vertexBufferMemory, 0, VK_WHOLE_SIZE, 0, &vertexBufferMapped));
		}

		// Index buffer
		if ((indexBuffer == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
			if (indexBufferMapped)
			{
				vkUnmapMemory(logicalDevice, indexBufferMemory);
				indexBufferMapped = nullptr;
			}

			if(indexBuffer)
				vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);

			if(indexBufferMemory)
				vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);


			VkUtils::CreateBuffer(physicalDevice, logicalDevice, indexBufferSize,
				VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
				indexBuffer, indexBufferMemory);
			indexCount = imDrawData->TotalIdxCount;
			CheckVulkanResult(vkMapMemory(logicalDevice, indexBufferMemory, 0, VK_WHOLE_SIZE, 0, &indexBufferMapped));
		}

		// Upload data
		ImDrawVert* vtxDst = (ImDrawVert*)vertexBufferMapped;
		ImDrawIdx* idxDst = (ImDrawIdx*)indexBufferMapped;

		for (int n = 0; n < imDrawData->CmdListsCount; n++) {
			const ImDrawList* cmd_list = imDrawData->CmdLists[n];
			memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
			memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
			vtxDst += cmd_list->VtxBuffer.Size;
			idxDst += cmd_list->IdxBuffer.Size;
		}

		// Flush to make writes visible to GPU
		VkMappedMemoryRange vertexMappedRange = {};
		vertexMappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		vertexMappedRange.memory = vertexBufferMemory;
		vertexMappedRange.offset = 0;
		vertexMappedRange.size = VK_WHOLE_SIZE;
		CheckVulkanResult(vkFlushMappedMemoryRanges(logicalDevice, 1, &vertexMappedRange));

		VkMappedMemoryRange indexMappedRange = {};
		indexMappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		indexMappedRange.memory = indexBufferMemory;
		indexMappedRange.offset = 0;
		indexMappedRange.size = VK_WHOLE_SIZE;
		CheckVulkanResult(vkFlushMappedMemoryRanges(logicalDevice, 1, &indexMappedRange));
	}

	void DrawFrame(VkCommandBuffer commandBuffer)
	{
		ImGuiIO& io = ImGui::GetIO();

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewport = VkUtils::CreateViewport(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// UI scale and translate via push constants
		pushConstBlock.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
		pushConstBlock.translate = glm::vec2(-1.0f);
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

		// Render commands
		ImDrawData* imDrawData = ImGui::GetDrawData();
		int32_t vertexOffset = 0;
		int32_t indexOffset = 0;

		if (imDrawData->CmdListsCount > 0) {

			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

			for (int32_t i = 0; i < imDrawData->CmdListsCount; i++)
			{
				const ImDrawList* cmd_list = imDrawData->CmdLists[i];
				for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++)
				{
					const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
					VkRect2D scissorRect;
					scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
					scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
					scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
					scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
					vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
					vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
					indexOffset += pcmd->ElemCount;
				}
				vertexOffset += cmd_list->VtxBuffer.Size;
			}
		}
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

		// Image view
		VkImageViewCreateInfo viewInfo =VkUtils::CreateImageViewCreateInfo();
		viewInfo.image = fontImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.layerCount = 1;
		CheckVulkanResult(vkCreateImageView(logicalDevice, &viewInfo, nullptr, &fontView));

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

		// Font texture Sampler
		VkSamplerCreateInfo samplerInfo = VkUtils::CreateSamplerCreateInfo();
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		CheckVulkanResult(vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler));
		
		// Descriptor pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			VkUtils::CreateDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = VkUtils::CreateDescriptorPoolCreateInfo(poolSizes, 2);
		CheckVulkanResult(vkCreateDescriptorPool(logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

		// Descriptor set layout
		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
			VkUtils::CreateDescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorLayout = VkUtils::CreateDescriptorSetLayoutCreateInfo(setLayoutBindings);
		CheckVulkanResult(vkCreateDescriptorSetLayout(logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));
		
		// Descriptor set
		VkDescriptorSetAllocateInfo allocInfo = VkUtils::CreateDescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		CheckVulkanResult(vkAllocateDescriptorSets(logicalDevice, &allocInfo, &descriptorSet));

		VkDescriptorImageInfo fontDescriptor = VkUtils::CreateDescriptorImageInfo(
			sampler,
			fontView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			VkUtils::CreateWriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)
		};
		vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()),
			writeDescriptorSets.data(), 0, nullptr);
		
		// Pipeline cache
		VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
		pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		CheckVulkanResult(vkCreatePipelineCache(logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
		
		// Pipeline layout
		// Push constants for UI rendering parameters
		VkPushConstantRange pushConstantRange = VkUtils::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = VkUtils::CreatePipelineLayoutCreateInfo(&descriptorSetLayout, 1);
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
		CheckVulkanResult(vkCreatePipelineLayout(logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
		
		// Setup graphics pipeline for UI rendering
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			VkUtils::CreatePipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		
		VkPipelineRasterizationStateCreateInfo rasterizationState =
			VkUtils::CreatePipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
		
		// Enable blending
		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.blendEnable = VK_TRUE;
		blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
		
		VkPipelineColorBlendStateCreateInfo colorBlendState =
			VkUtils::CreatePipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		
		VkPipelineDepthStencilStateCreateInfo depthStencilState =
			VkUtils::CreatePipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
		
		VkPipelineViewportStateCreateInfo viewportState =
			VkUtils::CreatePipelineViewportStateCreateInfo(1, 1, 0);
		
		VkPipelineMultisampleStateCreateInfo multisampleState =
			VkUtils::CreatePipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		
		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState =
			VkUtils::CreatePipelineDynamicStateCreateInfo(dynamicStateEnables);
		
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};
		
		VkGraphicsPipelineCreateInfo pipelineCreateInfo = VkUtils::CreatePipelineCreateInfo(pipelineLayout, renderPass);
		
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();
		
		// Vertex bindings an attributes based on ImGui vertex definition
		std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
			VkUtils::CreateVertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
		};
		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
			VkUtils::CreateVertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
			VkUtils::CreateVertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
			VkUtils::CreateVertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
		};
		VkPipelineVertexInputStateCreateInfo vertexInputState = VkUtils::CreatePipelineVertexInputStateCreateInfo();
		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
		vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
		vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
		
		pipelineCreateInfo.pVertexInputState = &vertexInputState;

		std::string workingPath = std::filesystem::current_path().string();
		std::string vertShader = workingPath + "/Shaders/ui.vert.spv";
		std::string fragShader = workingPath + "/Shaders/ui.frag.spv";

		// load shader code
		auto vertShaderCode = VkUtils::ReadBinaryFile(vertShader);
		auto fragShaderCode = VkUtils::ReadBinaryFile(fragShader);

		// create shader module
		VkShaderModule vertShaderModule = VkUtils::CreateShaderModule(logicalDevice, vertShaderCode);
		VkShaderModule fragShaderModule = VkUtils::CreateShaderModule(logicalDevice, fragShaderCode);

		// create shader stage
		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		shaderStages[0] = vertShaderStageInfo;
		shaderStages[1] = fragShaderStageInfo;
		
		CheckVulkanResult(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

		vkDestroyShaderModule(logicalDevice, vertShaderModule, nullptr);
		vkDestroyShaderModule(logicalDevice, fragShaderModule, nullptr);
	}

	void CleanUpVulkanResource()
	{
		vkDestroyBuffer(logicalDevice, vertexBuffer, nullptr);
		vkFreeMemory(logicalDevice, vertexBufferMemory, nullptr);

		vkDestroyBuffer(logicalDevice, indexBuffer, nullptr);
		vkFreeMemory(logicalDevice, indexBufferMemory, nullptr);

		vkDestroyImage(logicalDevice, fontImage, nullptr);
		vkDestroyImageView(logicalDevice, fontView, nullptr);
		vkFreeMemory(logicalDevice, fontMemory, nullptr);
		vkDestroySampler(logicalDevice, sampler, nullptr);
		vkDestroyPipelineCache(logicalDevice, pipelineCache, nullptr);
		vkDestroyPipeline(logicalDevice, pipeline, nullptr);
		vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
		vkDestroyDescriptorPool(logicalDevice, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(logicalDevice, descriptorSetLayout, nullptr);
	}

private:

	int32_t vertexCount = 0;
	VkBuffer vertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
	void* vertexBufferMapped = nullptr;

	int32_t indexCount = 0;
	VkBuffer indexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
	void* indexBufferMapped = nullptr;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;

	VkPipelineCache pipelineCache;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
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
