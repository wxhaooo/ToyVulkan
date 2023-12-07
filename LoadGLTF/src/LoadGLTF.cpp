#include<iostream>
#include<LoadGLTF.h>
#include <VulkanHelper.h>
#include <VulkanGLTFModel.h>
#include <VulkanInitializers.h>

#include <Camera.h>
#include <imgui.h>
#include <Singleton.hpp>
#include <GraphicSettings.hpp>
#include <VulkanRenderPass.h>
#include <VulkanUtils.h>

LoadGLFT::~LoadGLFT()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class

	gltfModel.reset();
	
	vkDestroyPipeline(device, pipelines.onscreen, nullptr);
	if (pipelines.onScreenWireframe != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.onScreenWireframe, nullptr);
	}
	vkDestroyPipeline(device, pipelines.offscreen, nullptr);
	if (pipelines.offscreenWireframe != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.offscreenWireframe, nullptr);
	}

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, MVPDescriptorSetLayout, nullptr);

	shaderData.buffer.Destroy();
}

void LoadGLFT::InitFondation()
{
	VulkanApplicationBase::InitFondation();
	
	Camera* camera = Singleton<Camera>::Instance();
	// set flipY true, if gltfLoadingFlags != vks::geometry::FileLoadingFlags::FlipY;
	// camera->flipY = true;
	// camera->type = Camera::CameraType::firstperson;

	// camera->position = { 1.0f, 0.75f, 0.0f };
	// camera->SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));
	// camera->SetPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);

	camera->type = Camera::CameraType::lookat;
	camera->flipY = true;
	camera->SetLookAt(glm::vec3(0.0f, -0.1f, 1.0f),glm::vec3(0.0f,0.0f,0.0f));
	camera->SetPosition(glm::vec3(0.0f, -0.1f, -1.0f));
	camera->SetRotation(glm::vec3(0.0f, 45.0f, 0.0f));
	camera->SetPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);	
}

void LoadGLFT::Prepare()
{
    VulkanApplicationBase::Prepare();
    LoadAsset();
	PrepareUniformBuffers();
	SetupDescriptors();
	PreparePipelines();
	prepared = true;
}

void LoadGLFT::LoadAsset()
{
    gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
	vks::geometry::descriptorBindingFlags  = vks::geometry::DescriptorBindingFlags::ImageBaseColor;
	const uint32_t gltfLoadingFlags = vks::geometry::FileLoadingFlags::FlipY;
	// gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/rubbertoy/rubbertoy.gltf",
		// vulkanDevice.get(), queue, gltfLoadingFlags);
	// gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/sponza_ktx/sponza.gltf",
		// vulkanDevice.get(), queue, gltfLoadingFlags);
    gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/FlightHelmet/glTF/FlightHelmet.gltf",
    	vulkanDevice.get(),queue);
}

void LoadGLFT::PrepareUniformBuffers()
{
    // Vertex shader uniform buffer block
    CheckVulkanResult(vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &shaderData.buffer,
        sizeof(shaderData.values)));

    // Map persistent
    CheckVulkanResult(shaderData.buffer.Map());

    UpdateUniformBuffers();
}

void LoadGLFT::UpdateUniformBuffers()
{
	Camera* camera = Singleton<Camera>::Instance();
    shaderData.values.projection = camera->matrices.perspective;
    shaderData.values.model = camera->matrices.view;
    shaderData.values.viewPos = camera->viewPos;
    memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
}

void LoadGLFT::SetupDescriptors()
{
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
	};
	// One set for matrices and one per model image/texture
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 1);
	CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	// Descriptor set layout for passing matrices
	VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &MVPDescriptorSetLayout));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &MVPDescriptorSetLayout, 1);
	CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void LoadGLFT::PreparePipelines()
{
	// create pipeline layout
	// Pipeline layout using both descriptor sets (set 0 = matrices,set 1 = sampler)
	std::vector<VkDescriptorSetLayout> setLayouts;
	setLayouts.push_back(MVPDescriptorSetLayout);
	setLayouts.push_back(vks::geometry::descriptorSetLayoutImage);
	// setLayouts.push_back(MVPDescriptorSetLayout);
	// std::array<VkDescriptorSetLayout, 1> setLayouts = { MVPDescriptorSetLayout };
	VkPipelineLayoutCreateInfo pipelineLayoutCI= vks::initializers::PipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));
	
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	VkPipelineColorBlendAttachmentState blendAttachmentStateCI = vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(1, &blendAttachmentStateCI);
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
	// Vertex input bindings and attributes
	const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		vks::initializers::VertexInputBindingDescription(0, sizeof(vks::geometry::VulkanGLTFModel::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, pos)),	// Location 0: Position
		vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, normal)),// Location 1: Normal
		vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),	// Location 2: Texture coordinates
		vks::initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, color)),	// Location 3: Color
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();
	vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		LoadShader(vks::helper::GetShaderBasePath() + "gltfloading/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(vks::helper::GetShaderBasePath() + "gltfloading/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	GraphicSettings* graphicSettings = Singleton<GraphicSettings>::Instance();
	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo();
	pipelineCI.layout = pipelineLayout;
	pipelineCI.renderPass = renderPass;
	pipelineCI.pVertexInputState = &vertexInputStateCI;
	pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
	pipelineCI.pRasterizationState = &rasterizationStateCI;
	pipelineCI.pColorBlendState = &colorBlendStateCI;
	pipelineCI.pMultisampleState = &multisampleStateCI;
	pipelineCI.pViewportState = &viewportStateCI;
	pipelineCI.pDepthStencilState = &depthStencilStateCI;
	pipelineCI.pDynamicState = &dynamicStateCI;
	pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCI.pStages = shaderStages.data();
	pipelineCI.flags = 0;

	// Solid rendering pipeline
	CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.onscreen));

	// Wire frame rendering pipeline
	if (deviceFeatures.fillModeNonSolid) {
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCI.lineWidth = 1.0f;
		CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.onScreenWireframe));
	}
	
	// offscreen rendering pipeline
	if(graphicSettings->standaloneGUI)
	{
		pipelineCI.renderPass = offscreenPass->renderPass;
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
		CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));

		if (deviceFeatures.fillModeNonSolid) {
			rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
			rasterizationStateCI.lineWidth = 1.0f;
			CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreenWireframe));
		}	
	}
}

void LoadGLFT::BuildCommandBuffers(VkCommandBuffer commandBuffer)
{
	GraphicSettings* graphicSettings = Singleton<GraphicSettings>::Instance();
	const VkViewport viewport = vks::initializers::Viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::Rect2D(width, height, 0, 0);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	// Bind scene matrices descriptor to set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	if(!graphicSettings->standaloneGUI)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.onScreenWireframe : pipelines.onscreen);
	else
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.offscreenWireframe : pipelines.offscreen);
	gltfModel->Draw(commandBuffer, vks::geometry::RenderFlags::BindImages, pipelineLayout,1);
}

void LoadGLFT::NewGUIFrame()
{
	if(ImGui::Begin("UI_View",nullptr, ImGuiWindowFlags_ForwardBackend))
	{
		if (ImGui::BeginTabBar("UI_ViewTabRoot",ImGuiTabBarFlags_None))
		{
			if(ImGui::BeginTabItem("UI_ViewTab_1"))
			{
				ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
				ImGui::Image((ImTextureID)offscreenPass->descriptorSet[currentFrame], ImVec2(viewportPanelSize.x, viewportPanelSize.y));
				ImGui::EndTabItem();
			}

			if(ImGui::BeginTabItem("UI_ViewTab_2"))
			{
				ImGui::Text("This is the Avocado tab!\nblah blah blah blah blah");
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	
	ImGui::ShowDemoWindow();
}

void LoadGLFT::ViewChanged()
{
	UpdateUniformBuffers();
}

void LoadGLFT::Render()
{
	RenderFrame();
	Camera* camera = Singleton<Camera>::Instance();
    if(camera->updated)
	    UpdateUniformBuffers();
}

