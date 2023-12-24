#include<iostream>
#include<DeferredPBR.h>
#include <VulkanHelper.h>
#include <VulkanGLTFModel.h>
#include <VulkanInitializers.h>

#include <Camera.h>
#include <imgui.h>
#include <Singleton.hpp>
#include <GraphicSettings.hpp>
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

DeferredPBR::~DeferredPBR()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class
	gltfModel.reset();

	// mrt pass resource
	mrtRenderPass.reset();

	if(pipelines.offscreen != VK_NULL_HANDLE)
		vkDestroyPipeline(device,pipelines.offscreen,nullptr);

	if(pipelines.offscreenWireframe != VK_NULL_HANDLE)
		vkDestroyPipeline(device,pipelines.offscreenWireframe,nullptr);
	
	vkDestroyPipelineLayout(device, mrtPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, mrtDescriptorSetLayout_Vertex, nullptr);

	// lighting
	if(pipelines.lighting != VK_NULL_HANDLE)
		vkDestroyPipeline(device,pipelines.lighting,nullptr);
	vkDestroyPipelineLayout(device,lightingPipelineLayout,nullptr);
	vkDestroyDescriptorSetLayout(device,lightingDescriptorSetLayout,nullptr);

	shaderData.buffer.Destroy();
	lightingUbo.buffer.Destroy();
}

void DeferredPBR::InitFondation()
{
	VulkanApplicationBase::InitFondation();
	
	Camera* camera = Singleton<Camera>::Instance();
	// camera->flipY = true;
	// camera->type = Camera::CameraType::firstperson;
	// camera->position = { 1.0f, 0.75f, 0.0f };
	// camera->SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));
	// camera->SetPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);

	// toy
	camera->type = Camera::lookat;
	camera->SetPosition(glm::vec3(0.065f,-0.03f,-2.7f));
	camera->SetRotation(glm::vec3(187.0f, 40.0f, 0.0f));
	camera->SetPerspective(60.0f, (float)viewportWidth / (float)viewportHeight, 0.1f, 256.0f);	
}

void DeferredPBR::SetupMrtRenderPass()
{
	mrtRenderPass = std::make_unique<vks::VulkanRenderPass>("mrtRenderPass",vulkanDevice.get());
	const uint32_t imageWidth = swapChain->imageExtent.width;
	const uint32_t imageHeight = swapChain->imageExtent.height;
	mrtRenderPass->Init(imageWidth, imageHeight, swapChain->imageCount);
    
	// Four attachments (3 color, 1 depth)
	vks::AttachmentCreateInfo attachmentInfo = {};
	attachmentInfo.width = imageWidth;
	attachmentInfo.height = imageHeight;
	attachmentInfo.layerCount = 1;
	attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// Color attachments
	// Attachment 0: (World space) Positions
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.name ="G_WorldPosition";
	attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	mrtRenderPass->AddAttachment(attachmentInfo);

	// Attachment 1: (World space) Normals
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.name ="G_WorldNormal";
	attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	mrtRenderPass->AddAttachment(attachmentInfo);

	// Attachment 2: Albedo (color)
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.name ="G_Color";
	attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	mrtRenderPass->AddAttachment(attachmentInfo);

	// Attachment 3: Roughness
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.name ="G_Roughness";
	attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	mrtRenderPass->AddAttachment(attachmentInfo);

	// Attachment 4: Emissive
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.name ="G_Emissive";
	attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	mrtRenderPass->AddAttachment(attachmentInfo);

	// // Attachment 5: Occlusion
	// attachmentInfo.binding = 5;
	// attachmentInfo.name ="G_Occlusion";
	// attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	// mrtRenderPass->AddAttachment(attachmentInfo);

	// Attachment 6: Depth
	attachmentInfo.name ="Depth";
	attachmentInfo.binding = mrtRenderPass->AttachmentCount();
	attachmentInfo.format = depthFormat;
	attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	mrtRenderPass->AddAttachment(attachmentInfo);

	VkFilter magFiler = VK_FILTER_NEAREST;
	VkFilter minFiler = VK_FILTER_NEAREST;
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	// VkFilter magFiler = VK_FILTER_LINEAR;
	// VkFilter minFiler = VK_FILTER_LINEAR;
	// VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	mrtRenderPass->AddSampler(magFiler,minFiler,addressMode);
	
	mrtRenderPass->CreateRenderPass();
	mrtRenderPass->CreateDescriptorSet();
}

void DeferredPBR::SetupLightingRenderPass()
{
	lightingRenderPass = std::make_unique<vks::VulkanRenderPass>("lightingRenderPass", vulkanDevice.get());
	const uint32_t imageWidth = swapChain->imageExtent.width;
	const uint32_t imageHeight = swapChain->imageExtent.height;
	lightingRenderPass->Init(imageWidth, imageHeight, swapChain->imageCount);

	// Four attachments (1 color)
	vks::AttachmentCreateInfo attachmentInfo = {};
	attachmentInfo.width = imageWidth;
	attachmentInfo.height = imageHeight;
	attachmentInfo.layerCount = 1;
	attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	// Color attachments
	attachmentInfo.binding = 0;
	attachmentInfo.name ="Final";
	attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	lightingRenderPass->AddAttachment(attachmentInfo);
	
	VkFilter magFiler = VK_FILTER_NEAREST;
	VkFilter minFiler = VK_FILTER_NEAREST;
	VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	lightingRenderPass->AddSampler(magFiler,minFiler,addressMode);
	
	lightingRenderPass->CreateRenderPass();
	lightingRenderPass->CreateDescriptorSet();
}

void DeferredPBR::Prepare()
{
    VulkanApplicationBase::Prepare();

	// render pass
	SetupMrtRenderPass();
	SetupLightingRenderPass();
	
    LoadAsset();
	PrepareUniformBuffers();
	SetupDescriptorSets();

	// prepare pipelines
	PrepareMrtPipeline();
	PrepareLightingPipeline();
	prepared = true;
}

void DeferredPBR::LoadAsset()
{
    gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
	const uint32_t descriptorBindingFlags  =
		vks::geometry::DescriptorBindingFlags::ImageBaseColor |
			vks::geometry::DescriptorBindingFlags::ImageNormalMap;
	const uint32_t gltfLoadingFlags = vks::geometry::FileLoadingFlags::FlipY;
	// | vks::geometry::PreTransformVertices;
	gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/DamagedHelmet/DamagedHelmet.gltf",
		vulkanDevice.get(), queue, gltfLoadingFlags, descriptorBindingFlags,1);
	// gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/Sponza/glTF/sponza.gltf",
	// 	vulkanDevice.get(), queue, gltfLoadingFlags, descriptorBindingFlags,1);
}

void DeferredPBR::PrepareUniformBuffers()
{
    // Vertex shader uniform buffer block
    CheckVulkanResult(vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &shaderData.buffer,
        sizeof(shaderData.values)));

    // Map persistent
    CheckVulkanResult(shaderData.buffer.Map());

	// lighting uniform buffer
	CheckVulkanResult(vulkanDevice->CreateBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
				&lightingUbo.buffer,
				sizeof(lightingUbo.values)));
	
	// Map persistent
	CheckVulkanResult(lightingUbo.buffer.Map());

    UpdateUniformBuffers();
}

void DeferredPBR::UpdateUniformBuffers()
{
	Camera* camera = Singleton<Camera>::Instance();
    shaderData.values.projection = camera->matrices.perspective;
    shaderData.values.view = camera->matrices.view;
    memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));
	
	for(uint32_t i = 0; i<gltfModel->lights.size();i++)
	{
		vks::geometry::Light light = gltfModel->lights[i];
		lightingUbo.values.lights[i].color = light.color;
		lightingUbo.values.lights[i].intensity = light.intensity;
		lightingUbo.values.lights[i].position = light.position;
	}

	// add default lights
	if(gltfModel->lights.size() == 0)
	{
		glm::vec3 lightPos = glm::vec3(0.0f,-1.85776f,0.0f);
		lightingUbo.values.lights[0].color = glm::vec4(1.0f);
		lightingUbo.values.lights[0].intensity = 1.0f;
		lightingUbo.values.lights[0].position = glm::vec4(lightPos,1.0f);

		lightPos = glm::vec3(0.149955f,-0.3774542f,2.68973f);
		lightingUbo.values.lights[1].color = glm::vec4(1.0f);
		lightingUbo.values.lights[1].intensity = 1.0f;
		lightingUbo.values.lights[1].position = glm::vec4(lightPos,1.0f);
	}
	
	lightingUbo.values.viewPos = camera->viewPos;
	memcpy(lightingUbo.buffer.mapped, &lightingUbo.values, sizeof(lightingUbo.values));

}

void DeferredPBR::SetupDescriptorSets()
{
	if(mrtDescriptorSetLayout_Vertex != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(device,mrtDescriptorSetLayout_Vertex,nullptr);

	if(lightingDescriptorSetLayout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(device,lightingDescriptorSetLayout,nullptr);

	if(descriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(device,descriptorPool,nullptr);
		
	// for mrt pass
	std::vector<VkDescriptorPoolSize> poolSizes = {
		// 1 for mrt pass => vertex shader, 1 for lighting fragment shader
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100),
		// 3 for lighting pass => fragment shader
		vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100),

	};
	// 1 for mrt pass, 1 for lighting pass
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 100 * maxFrameInFlight);
	CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

	// Descriptor set layout for passing matrices
	VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &mrtDescriptorSetLayout_Vertex));

	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &mrtDescriptorSetLayout_Vertex, 1);
	mrtDescriptorSets_Vertex.resize(maxFrameInFlight);
	for(uint32_t i=0; i < maxFrameInFlight; i++)
	{
		CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &mrtDescriptorSets_Vertex[i]));
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(mrtDescriptorSets_Vertex[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}
	// for lighting pass
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),
		// vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
		vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),
	};
	
	descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(setLayoutBindings);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &lightingDescriptorSetLayout));
	allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool, &lightingDescriptorSetLayout, 1);
	lightingDescriptorSets.resize(maxFrameInFlight);
	for(uint32_t i=0; i < maxFrameInFlight; i++)
	{
		CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &lightingDescriptorSets[i]));
		vks::FrameBuffer* frameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(i);
		// attachment
		for(uint32_t s = 0; s < frameBuffer->attachments.size(); s++)
		{
			if(frameBuffer->attachments[s].IsDepthStencil()) continue;
			VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(lightingDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, s, &frameBuffer->attachments[s].descriptor);
			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
		}
		// lighting uniform buffer
		VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(lightingDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			mrtRenderPass->ColorAttachmentCount(), &lightingUbo.buffer.descriptor);
		vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
	}
}

void DeferredPBR::PrepareMrtPipeline()
{
	// create pipeline layout
	// Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
	std::vector<VkDescriptorSetLayout> setLayouts;
	setLayouts.push_back(mrtDescriptorSetLayout_Vertex);
	setLayouts.push_back(vks::geometry::descriptorSetLayoutImage);
	VkPipelineLayoutCreateInfo pipelineLayoutCI= vks::initializers::PipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
	CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &mrtPipelineLayout));

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates =
	{
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
		// vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
	};
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(blendAttachmentStates.size(), blendAttachmentStates.data());
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
		vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),	// Location 1: Texture coordinates
		vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, color)),	// Location 2: Color
		vks::initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, normal)),// Location 3: Normal
		vks::initializers::VertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vks::geometry::VulkanGLTFModel::Vertex, tangent)),	// Location 4: Tangent
	};
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();
	vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		LoadShader(vks::helper::GetShaderBasePath() + "deferred/mrt.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(vks::helper::GetShaderBasePath() + "deferred/mrt.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo();
	pipelineCI.layout = mrtPipelineLayout;
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
	
	// deferred rendering pipeline
	pipelineCI.renderPass = mrtRenderPass->renderPass;
	rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
	CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));

	if (deviceFeatures.fillModeNonSolid) {
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCI.lineWidth = 1.0f;
		CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreenWireframe));
	}
}

void DeferredPBR::PrepareLightingPipeline()
{
	// create pipeline layout
	std::vector<VkDescriptorSetLayout> setLayouts = {lightingDescriptorSetLayout};
	VkPipelineLayoutCreateInfo pipelineLayoutCI= vks::initializers::PipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
	// We will use push constants to push the local matrices of a primitive to the vertex shader
	// VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 0;
	CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &lightingPipelineLayout));

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
	VkPipelineRasterizationStateCreateInfo rasterizationStateCI = vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
	std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates =
	{
		vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
	};
	VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(blendAttachmentStates.size(), blendAttachmentStates.data());
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
	VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
	VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
	const std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
	VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();

	const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
		LoadShader(vks::helper::GetShaderBasePath() + "deferred/lighting.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
		LoadShader(vks::helper::GetShaderBasePath() + "deferred/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo();
	pipelineCI.layout = lightingPipelineLayout;
	pipelineCI.renderPass = lightingRenderPass->renderPass;
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
	CheckVulkanResult(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipelines.lighting));
}

void DeferredPBR::BuildCommandBuffers(VkCommandBuffer commandBuffer)
{
	const VkViewport viewport = vks::initializers::Viewport((float)viewportWidth, (float)viewportHeight, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	// Bind scene matrices descriptor to set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrtPipelineLayout, 0, 1, &mrtDescriptorSets_Vertex[currentFrame], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.offscreenWireframe : pipelines.offscreen);
	gltfModel->Draw(commandBuffer,vks::geometry::RenderFlags::BindImages, true, mrtPipelineLayout,1);
}

void DeferredPBR::PrepareRenderPass(VkCommandBuffer commandBuffer)
{
	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

	vks::FrameBuffer* mrtFrameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
	renderPassBeginInfo.renderPass = mrtRenderPass->renderPass;
	renderPassBeginInfo.framebuffer = mrtFrameBuffer->frameBuffer;

	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = {viewportWidth, viewportHeight};

	std::vector<VkClearValue> clearValues
	{
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		{0.0f, 0.0f, 0.0f, 1.0f},
		// {0.0f, 0.0f, 0.0f, 1.0f},
	};
	
	VkClearValue depthClearValue;
	depthClearValue.depthStencil = {1.0f,0};
	clearValues.push_back(depthClearValue);

	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	BuildCommandBuffers(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);

	// lighting renderPass
	vks::FrameBuffer* lightingFrameBuffer = lightingRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
	renderPassBeginInfo.renderPass = lightingRenderPass->renderPass;
	renderPassBeginInfo.framebuffer = lightingFrameBuffer->frameBuffer;
	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = {viewportWidth, viewportHeight};
	
	std::array<VkClearValue, 2> clearValues1{};
	clearValues1[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearValues1[1].color = {0.0f, 0.0f, 0.0f, 1.0f};
	
	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();
	
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	const VkViewport viewport = vks::initializers::Viewport((float)viewportWidth, (float)viewportHeight, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);
	
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout, 0, 1, &lightingDescriptorSets[currentFrame], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.lighting);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
	vkCmdEndRenderPass(commandBuffer);
}

void DeferredPBR::ReCreateVulkanResource_Child()
{
	mrtRenderPass.reset();
	SetupMrtRenderPass();

	lightingRenderPass.reset();
	SetupLightingRenderPass();

	SetupDescriptorSets();
}

void DeferredPBR::NewGUIFrame()
{
	if(ImGui::Begin("UI_GBuffer_View"))
	{
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		vks::FrameBuffer* frameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
		float scale = std::min(viewportPanelSize.x / (float)viewportWidth, viewportPanelSize.y / (float)viewportHeight); 
		for(uint32_t i=0; i < frameBuffer->attachments.size(); i++)
		{
			if(!frameBuffer->attachments[i].HasDepth() && !frameBuffer->attachments[i].HasStencil())
				ImGui::Image((ImTextureID)frameBuffer->attachments[i].descriptorSet,ImVec2(viewportWidth * scale, viewportHeight * scale));
		}
	
		ImGui::End();
	}

	if(ImGui::Begin("UI_View",nullptr, ImGuiWindowFlags_ForwardBackend))
	{
		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		vks::FrameBuffer* frameBuffer = lightingRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
		float scale = std::min(viewportPanelSize.x / (float)viewportWidth, viewportPanelSize.y / (float)viewportHeight);
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 imageSize = ImVec2(viewportWidth * scale, viewportHeight * scale);
		ImGui::SetCursorPos(ImVec2((windowSize.x - imageSize.x) * 0.5f,(windowSize.y - imageSize.y) * 0.5f));
		ImGui::Image((ImTextureID)frameBuffer->attachments[0].descriptorSet,imageSize);
		ImGui::End();
	}

	if(ImGui::Begin("UI_Status"))
	{
		if (ImGui::CollapsingHeader("Camera"))
		{
			Camera* camera = Singleton<Camera>::Instance();
			ImGui::Text("Camera Position: (%g, %g, %g)",
				camera->position.x,camera->position.y,camera->position.z);
			ImGui::Text("Camera Rotation: (%g, %g, %g)",
				camera->rotation.x,camera->rotation.y,camera->rotation.z);
		}
	
		if(ImGui::CollapsingHeader("Performance"))
		{
			
		}
	
		ImGui::End();
	}
	
	ImGui::ShowDemoWindow();
}

void DeferredPBR::ViewChanged()
{
	UpdateUniformBuffers();
}

void DeferredPBR::Render()
{
	RenderFrame();
	Camera* camera = Singleton<Camera>::Instance();
	
    if(camera->updated)
	    UpdateUniformBuffers();
}

