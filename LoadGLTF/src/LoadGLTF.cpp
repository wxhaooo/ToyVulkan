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

LoadGLFT::~LoadGLFT()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class

	gltfModel.reset();
    
	// offscreen pass resource
	offscreenPass.reset();
	
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
	camera->SetPerspective(60.0f, (float)windowsWidth / (float)windowsHeight, 0.1f, 256.0f);	
}

void LoadGLFT::Prepare()
{
    VulkanApplicationBase::Prepare();

	SetupOffscreenResource();
	SetupOffscreenRenderPass();
	SetupOffscreenFrameBuffer();
	
    LoadAsset();
	PrepareUniformBuffers();
	SetupDescriptors();
	PreparePipelines();
	prepared = true;
}

void LoadGLFT::LoadAsset()
{
    gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
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

void LoadGLFT::SetupOffscreenResource()
{
    if (offscreenPass == nullptr)
        offscreenPass = std::make_unique<vks::OffscreenPass>();

    offscreenPass->device = device;
    offscreenPass->width = static_cast<int32_t>(swapChain->imageExtent.width);
    offscreenPass->height = static_cast<int32_t>(swapChain->imageExtent.height);

    offscreenPass->color.resize(maxFrameInFlight);
    offscreenPass->depth.resize(maxFrameInFlight);
    offscreenPass->sampler.resize(maxFrameInFlight);
    offscreenPass->descriptor.resize(maxFrameInFlight);
    offscreenPass->descriptorSet.resize(maxFrameInFlight);

    std::vector<VkDescriptorPoolSize> poolSizes = {
        vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(
        poolSizes, maxFrameInFlight);
    CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &offscreenPass->descriptorPool));
    VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
        &setLayoutBinding, 1);
    CheckVulkanResult(
        vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &offscreenPass->descriptorSetLayout));

    // Descriptor set for scene matrices
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
        offscreenPass->descriptorPool, &offscreenPass->descriptorSetLayout, 1);

    for (uint32_t i = 0; i < maxFrameInFlight; i++)
    {
        // Color attachment
        VkImageCreateInfo image = vks::initializers::ImageCreateInfo();
        image.imageType = VK_IMAGE_TYPE_2D;
        image.format = swapChain->colorFormat;
        image.extent.width = offscreenPass->width;
        image.extent.height = offscreenPass->height;
        image.extent.depth = 1;
        image.mipLevels = 1;
        image.arrayLayers = 1;
        image.samples = VK_SAMPLE_COUNT_1_BIT;
        image.tiling = VK_IMAGE_TILING_OPTIMAL;
        // We will sample directly from the color attachment
        image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

        VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;

        CheckVulkanResult(vkCreateImage(device, &image, nullptr, &offscreenPass->color[i].image));
        vkGetImageMemoryRequirements(device, offscreenPass->color[i].image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass->color[i].memory));
        CheckVulkanResult(vkBindImageMemory(device, offscreenPass->color[i].image, offscreenPass->color[i].memory, 0));

        VkImageViewCreateInfo colorImageView = vks::initializers::ImageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = swapChain->colorFormat;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreenPass->color[i].image;
        CheckVulkanResult(vkCreateImageView(device, &colorImageView, nullptr, &offscreenPass->color[i].view));

        // Depth stencil attachment
        image.format = depthFormat;
        image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        CheckVulkanResult(vkCreateImage(device, &image, nullptr, &offscreenPass->depth[i].image));
        vkGetImageMemoryRequirements(device, offscreenPass->depth[i].image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass->depth[i].memory));
        CheckVulkanResult(vkBindImageMemory(device, offscreenPass->depth[i].image, offscreenPass->depth[i].memory, 0));

        VkImageViewCreateInfo depthStencilView = vks::initializers::ImageViewCreateInfo();
        depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilView.format = depthFormat;
        depthStencilView.flags = 0;
        depthStencilView.subresourceRange = {};
        depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
        {
            depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        depthStencilView.subresourceRange.baseMipLevel = 0;
        depthStencilView.subresourceRange.levelCount = 1;
        depthStencilView.subresourceRange.baseArrayLayer = 0;
        depthStencilView.subresourceRange.layerCount = 1;
        depthStencilView.image = offscreenPass->depth[i].image;
        CheckVulkanResult(vkCreateImageView(device, &depthStencilView, nullptr, &offscreenPass->depth[i].view));

        // Create sampler to sample from the attachment in the fragment shader
        VkSamplerCreateInfo samplerInfo = vks::initializers::SamplerCreateInfo();
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = samplerInfo.addressModeU;
        samplerInfo.addressModeW = samplerInfo.addressModeU;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        CheckVulkanResult(vkCreateSampler(device, &samplerInfo, nullptr, &offscreenPass->sampler[i]));

        // Fill a descriptor for later use in a descriptor set
        offscreenPass->descriptor[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        offscreenPass->descriptor[i].imageView = offscreenPass->color[i].view;
        offscreenPass->descriptor[i].sampler = offscreenPass->sampler[i];

        CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &offscreenPass->descriptorSet[i]));
        VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(offscreenPass->descriptorSet[i],
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &offscreenPass->descriptor[i]);
        vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
    }
}

void LoadGLFT::SetupOffscreenRenderPass()
{
    // Create a separate render pass for the offscreen rendering as it may differ from the one used for scene rendering
    std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
    // Color attachment
    attchmentDescriptions[0].format = swapChain->colorFormat;
    attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // Depth attachment
    attchmentDescriptions[1].format = depthFormat;
    attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthReference = {1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
    renderPassInfo.pAttachments = attchmentDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    CheckVulkanResult(vkCreateRenderPass(device, &renderPassInfo, nullptr, &offscreenPass->renderPass));
}

void LoadGLFT::SetupOffscreenFrameBuffer()
{
    offscreenPass->frameBuffer.resize(maxFrameInFlight);
    for (uint32_t i = 0; i < maxFrameInFlight; i++)
    {
        VkImageView attachments[2]; 
        attachments[0] = offscreenPass->color[i].view;
        attachments[1] = offscreenPass->depth[i].view;

        VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::FramebufferCreateInfo();
        fbufCreateInfo.renderPass = offscreenPass->renderPass;
        fbufCreateInfo.attachmentCount = 2;
        fbufCreateInfo.pAttachments = attachments;
        fbufCreateInfo.width = offscreenPass->width;
        fbufCreateInfo.height = offscreenPass->height;
        fbufCreateInfo.layers = 1;

        CheckVulkanResult(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreenPass->frameBuffer[i]));
    }
}

void LoadGLFT::ReCreateVulkanResource_Child()
{
	// offscreen pass
	offscreenPass->DestroyResource();
	SetupOffscreenResource();
	SetupOffscreenFrameBuffer();
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
	pipelineCI.renderPass = offscreenPass->renderPass;
	rasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
	CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreen));

	if (deviceFeatures.fillModeNonSolid) {
		rasterizationStateCI.polygonMode = VK_POLYGON_MODE_LINE;
		rasterizationStateCI.lineWidth = 1.0f;
		CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipelines.offscreenWireframe));
	}
}

void LoadGLFT::BuildCommandBuffers(VkCommandBuffer commandBuffer)
{
	const VkViewport viewport = vks::initializers::Viewport((float)windowsWidth, (float)windowsHeight, 0.0f, 1.0f);
	const VkRect2D scissor = vks::initializers::Rect2D(windowsWidth, windowsHeight, 0, 0);

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	// Bind scene matrices descriptor to set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? pipelines.offscreenWireframe : pipelines.offscreen);
	gltfModel->Draw(commandBuffer, vks::geometry::RenderFlags::BindImages,true, pipelineLayout,1);
}

void LoadGLFT::PrepareRenderPass(VkCommandBuffer commandBuffer)
{
	VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

	renderPassBeginInfo.renderPass = offscreenPass->renderPass;
	renderPassBeginInfo.framebuffer = offscreenPass->frameBuffer[currentFrame];

	renderPassBeginInfo.renderArea.offset = {0, 0};
	renderPassBeginInfo.renderArea.extent = {windowsWidth, windowsHeight};

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
	clearValues[1].depthStencil = {1.0f, 0};

	renderPassBeginInfo.clearValueCount = clearValues.size();
	renderPassBeginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	// offscreen pass
	BuildCommandBuffers(commandBuffer);
	vkCmdEndRenderPass(commandBuffer);
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

