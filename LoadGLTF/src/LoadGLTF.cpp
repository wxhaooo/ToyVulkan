#include<iostream>
#include<LoadGLTF.h>
#include <VulkanHelper.h>
#include <VulkanGLTFModel.h>
#include <VulkanInitializers.h>

#include <Camera.h>
#include <Singleton.hpp>

LoadGLFT::~LoadGLFT()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(device, pipelines.solid, nullptr);
	if (pipelines.wireframe != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
	}

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);

	shaderData.buffer.Destroy();
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

void LoadGLFT::SetupCamera()
{
	VulkanApplicationBase::SetupCamera();
	Camera* camera = Singleton<Camera>::Instance();
	
	camera->flipY = true;
	camera->type = Camera::CameraType::lookat;
	
	// camera->SetLookAt(glm::vec3(0.0f, -0.1f, 1.0f),glm::vec3(0.0f,0.0f,0.0f));
	camera->SetPosition(glm::vec3(0.0f, -0.1f, -1.0f));
	camera->SetRotation(glm::vec3(0.0f, 45.0f, 0.0f));
	camera->SetPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);	
}

void LoadGLFT::LoadAsset()
{
    gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
    gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/FlightHelmet/glTF/FlightHelmet.gltf",vulkanDevice.get(),queue);
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
		// One combined image sampler per model image/texture
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(gltfModel->images.size())),
	};
	// One set for matrices and one per model image/texture
	const uint32_t maxSetCount = static_cast<uint32_t>(gltfModel->images.size()) + 1;
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, maxSetCount);
	CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	// Descriptor set layout for passing matrices
	VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));
	// Descriptor set layout for passing material textures
	setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));
	// Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
	std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI= vks::initializers::PipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
	CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));
	VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
	vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	// Descriptor sets for materials
	for (auto& image : gltfModel->images) {
		const VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
		CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &image.descriptorSet));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(image.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &image.texture.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}
}

void LoadGLFT::PreparePipelines()
{
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

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo(pipelineLayout, renderPass, 0);
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

	// Solid rendering pipeline
	CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.solid));

	// Wire frame rendering pipeline
	if (deviceFeatures.fillModeNonSolid) {
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCI.lineWidth = 1.0f;
		CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.wireframe));
	}	
}

void LoadGLFT::BuildCommandBuffers(VkCommandBuffer commandBuffer)
{
	const VkViewport viewport = vks::initializers::Viewport((float)width, (float)height, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::Rect2D(width, height, 0, 0);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	// Bind scene matrices descriptor to set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.wireframe : pipelines.solid);
	gltfModel->Draw(commandBuffer, pipelineLayout);
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

