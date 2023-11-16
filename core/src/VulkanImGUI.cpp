#include <set>
#include <backends/imgui_impl_glfw.h>
#include <VulkanImGUI.h>
#include <VulkanHelper.h>

#include <VulkanBuffer.h>
#include <VulkanInitializers.h>

#include <VulkanTexture.h>
#include <VulkanUtils.h>

#include "Camera.h"
#include "Singleton.hpp"

#undef max
#undef min

VulkanGUI::VulkanGUI(ImGUICreateInfo& imGUICreateInfo)
        : instance(imGUICreateInfo.instance),
        vulkanDevice(imGUICreateInfo.vulkanDevice),
		vulkanSwapChain(imGUICreateInfo.vulkanSwapChain),
		copyQueue(imGUICreateInfo.copyQueue),
		renderPass(imGUICreateInfo.renderPass),
		glfwWindow(imGUICreateInfo.glfwWindow)
{
    InitImGUIResource();
    InitVulkanResource();
}

VulkanGUI::~VulkanGUI()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanUpVulkanResource();
}

void VulkanGUI::NewFrame()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ImGui::Begin("Camera");

    // Camera* camera = Singleton<Camera>::Instance();
	
    // ImGui::InputFloat3("Camera Position", camera->position);
    // ImGui::InputFloat3("Camera Rotation", camera->rotation);

    // ImGui::End();

    ImGui::ShowDemoWindow();

    ImGui::Render();
}

void VulkanGUI::UpdateBuffer()
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
	if ((vertexBuffer.buffer == VK_NULL_HANDLE) || (vertexCount != imDrawData->TotalVtxCount))
	{
		// unmap
		if (vertexBuffer.mapped)
			vertexBuffer.Unmap();
		
		// destory
		if (vertexBuffer.buffer)
			vertexBuffer.Destroy();

		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,&vertexBuffer,vertexBufferSize);
		vertexCount = imDrawData->TotalVtxCount;
		vertexBuffer.Map();
	}

	// Index buffer
	if ((indexBuffer.buffer == VK_NULL_HANDLE) || (indexCount < imDrawData->TotalIdxCount)) {
		if (indexBuffer.mapped)
			indexBuffer.Unmap();

		if(indexBuffer.buffer)
			indexBuffer.Destroy();

		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,&indexBuffer,indexBufferSize);
		indexCount = imDrawData->TotalIdxCount;
		indexBuffer.Map();
	}

	// Upload data
	ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffer.mapped;
	ImDrawIdx* idxDst = (ImDrawIdx*)indexBuffer.mapped;

	for (int n = 0; n < imDrawData->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = imDrawData->CmdLists[n];
		memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtxDst += cmd_list->VtxBuffer.Size;
		idxDst += cmd_list->IdxBuffer.Size;
	}

	// Flush to make writes visible to GPU
	vertexBuffer.Flush();
	indexBuffer.Flush();
}

void VulkanGUI::DrawFrame(VkCommandBuffer commandBuffer)
{
	ImGuiIO& io = ImGui::GetIO();

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkViewport viewport = vks::initializers::Viewport(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);
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
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

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

void VulkanGUI::InitVulkanResource()
{
	ImGuiIO& io = ImGui::GetIO();

	// Create font texture
	unsigned char* fontData;
	int texWidth, texHeight;
	io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
	VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

	//SRS - Get Vulkan device driver information if available, use later for display
	if (vulkanDevice->ExtensionSupported(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME))
	{
		VkPhysicalDeviceProperties2 deviceProperties2 = {};
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &driverProperties;
		driverProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
		vkGetPhysicalDeviceProperties2(vulkanDevice->physicalDevice, &deviceProperties2);
	}
	
	// Create font image for copy
	fontImageTexture.FromBuffer(fontData,uploadSize,
		VK_FORMAT_R8G8B8A8_UNORM,texWidth, texHeight,vulkanDevice,
		copyQueue,VK_FILTER_LINEAR,VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
	// Descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 2);
	CheckVulkanResult(vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

	// Descriptor set layout
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
	};
	VkDescriptorSetLayoutCreateInfo descriptorLayout = vks::initializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
	CheckVulkanResult(vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));
	
	// Descriptor set
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	CheckVulkanResult(vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, &descriptorSet));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		vks::initializers::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontImageTexture.descriptor)
	};
	vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()),
		writeDescriptorSets.data(), 0, nullptr);
	
	// Pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = vks::initializers::PipelineCacheCreateInfo();
	CheckVulkanResult(vkCreatePipelineCache(vulkanDevice->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
	
	// Pipeline layout
	// Push constants for UI rendering parameters
	VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::PipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	CheckVulkanResult(vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
	
	// Setup graphics pipeline for UI rendering
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	
	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
	
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
		vks::initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
	
	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vks::initializers::PipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
	
	VkPipelineViewportStateCreateInfo viewportState =
		vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
	
	VkPipelineMultisampleStateCreateInfo multisampleState =
		vks::initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
	
	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		vks::initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables);
	
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{};

	// need ui render pass
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::PipelineCreateInfo();
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	
	// Vertex bindings an attributes based on ImGui vertex definition
	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::VertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)),	// Location 0: Position
		vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),	// Location 1: UV
		vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),	// Location 0: Color
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState = vks::initializers::PipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();
	
	pipelineCreateInfo.pVertexInputState = &vertexInputState;

	std::string vertShader = vks::helper::GetShaderBasePath() + "/ui/ui.vert.spv";
	std::string fragShader = vks::helper::GetShaderBasePath() + "/ui/ui.frag.spv";

	// load shader code
	auto vertShaderModule = vks::utils::LoadShader(vertShader.c_str(),vulkanDevice->logicalDevice);
	auto fragShaderModule = vks::utils::LoadShader(fragShader.c_str(),vulkanDevice->logicalDevice);

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
	
	CheckVulkanResult(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

	vkDestroyShaderModule(vulkanDevice->logicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(vulkanDevice->logicalDevice, fragShaderModule, nullptr);
}

void VulkanGUI::CleanUpVulkanResource()
{
	vertexBuffer.Destroy();
	indexBuffer.Destroy();

	fontImageTexture.Destroy();
	vkDestroyPipelineCache(vulkanDevice->logicalDevice, pipelineCache, nullptr);
	vkDestroyPipeline(vulkanDevice->logicalDevice, pipeline, nullptr);
	vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);
	vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayout, nullptr);
}

void VulkanGUI::InitImGUIResource()
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

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);
}