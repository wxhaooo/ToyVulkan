#include <set>
#include <backends/imgui_impl_glfw.h>
#include <VulkanImGUI.h>
#include <VulkanHelper.h>

#include <VulkanBuffer.h>
#include <VulkanInitializers.h>

#include <VulkanTexture.h>
#include <VulkanUtils.h>

#include <Camera.h>

#include <GraphicSettings.hpp>
#include <Singleton.hpp>

#undef max
#undef min

VulkanGUI::VulkanGUI(ImGUICreateInfo& imGUICreateInfo)
        : instance(imGUICreateInfo.instance),
        vulkanDevice(imGUICreateInfo.vulkanDevice),
		renderPass(imGUICreateInfo.renderPass),
		vulkanSwapChain(imGUICreateInfo.vulkanSwapChain),
		copyQueue(imGUICreateInfo.copyQueue),
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

// void VulkanGUI::NewFrame()
// {
//     ImGui_ImplGlfw_NewFrame();
// 	ImGui::NewFrame();
//  
// 	// ImGui::Begin("Vulkan Texture Test");
// 	// ImGui::Text("pointer = %p", my_texture.DS);
// 	// ImGui::Text("size = %d x %d", my_texture.Width, my_texture.Height);
// 	// ImGui::Image((ImTextureID)my_texture.DS, ImVec2(my_texture.Width, my_texture.Height));
// 	// ImGui::End();
//     // ImGui::Begin("Camera");
//
//     // Camera* camera = Singleton<Camera>::Instance();
// 	
//     // ImGui::InputFloat3("Camera Position", camera->position);
//     // ImGui::InputFloat3("Camera Rotation", camera->rotation);
//
//     // ImGui::End();
//
//     // ImGui::ShowDemoWindow();
// }

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

void VulkanGUI::Resize(uint32_t width, uint32_t height)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)(width), (float)(height));	
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
				// Bind DescriptorSet with font or user texture
				VkDescriptorSet descSet[1] = { descriptorSet };
				if (pcmd->TextureId != nullptr)
					descSet[0] = (VkDescriptorSet)pcmd->TextureId;
			
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, descSet, 0, nullptr);
				vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
				indexOffset += pcmd->ElemCount;
			}
			vertexOffset += cmd_list->VtxBuffer.Size;
		}
	}
}

void VulkanGUI::InitVulkanResource()
{
	PrepareFontTexture();
	// need font texture
	SetupDescriptors();
	
	CreatePipelineCache();
	PrepareShaders();
	PrepareRenderPass();
	// need shaders & descriptor & renderpass (optional)
	PreparePipelines();
}

void VulkanGUI::PrepareFontTexture()
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
}

void VulkanGUI::SetupDescriptors()
{
	// Descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
	};
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 10);
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
}

void VulkanGUI::CreatePipelineCache()
{
	// Pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = vks::initializers::PipelineCacheCreateInfo();
	CheckVulkanResult(vkCreatePipelineCache(vulkanDevice->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanGUI::PrepareShaders()
{
	std::string vertShader = vks::helper::GetShaderBasePath() + "/ui/ui.vert.spv";
	std::string fragShader = vks::helper::GetShaderBasePath() + "/ui/ui.frag.spv";

	// load shader code
	vertShaderModule = vks::utils::LoadShader(vertShader.c_str(),vulkanDevice->logicalDevice);
	fragShaderModule = vks::utils::LoadShader(fragShader.c_str(),vulkanDevice->logicalDevice);
}

void VulkanGUI::PrepareRenderPass()
{
	// use external renderpass
	if(renderPass != VK_NULL_HANDLE) return;

	standaloneRenderPass = true;

	auto graphicSetting = Singleton<GraphicSettings>::Instance();
	if (graphicSetting->requiresStencil)
		vks::utils::GetSupportedDepthStencilFormat(vulkanDevice->physicalDevice, &depthFormat);
	else 
		vks::utils::GetSupportedDepthFormat(vulkanDevice->physicalDevice, &depthFormat);

	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = vulkanSwapChain->colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	dependencies[0].dependencyFlags = 0;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependencies[1].dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	CheckVulkanResult(vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass));
}

void VulkanGUI::PreparePipelines()
{
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
		vks::initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	
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

	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	
	CheckVulkanResult(vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

	vkDestroyShaderModule(vulkanDevice->logicalDevice, vertShaderModule, nullptr);
	vkDestroyShaderModule(vulkanDevice->logicalDevice, fragShaderModule, nullptr);
}

void VulkanGUI::CleanUpVulkanResource()
{
	vertexBuffer.Destroy();
	indexBuffer.Destroy();

	fontImageTexture.Destroy();

	if(standaloneRenderPass)
		vkDestroyRenderPass(vulkanDevice->logicalDevice,renderPass,nullptr);
	
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