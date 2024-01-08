#include<iostream>
#include<DeferredPBR.h>
#include <VulkanHelper.h>
#include <VulkanGLTFModel.h>
#include <VulkanInitializers.h>

#include <Camera.h>
#include <imgui.h>
#include <Singleton.hpp>
#include <GraphicSettings.hpp>

#include <MathUtils.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

DeferredPBR::~DeferredPBR() {
    // Clean up used Vulkan resources
    // Note : Inherited destructor cleans up resources stored in base class
    gltfModel.reset();
    skybox.reset();

    // mrt pass resource
    mrtRenderPass.reset();

    if (pipelines.offscreen != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipelines.offscreen, nullptr);
    if (pipelines.offscreenWireframe != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipelines.offscreenWireframe, nullptr);

    if (mrtPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, mrtPipelineLayout, nullptr);
    if (mrtDescriptorSetLayout_Vertex != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, mrtDescriptorSetLayout_Vertex, nullptr);

    // lighting
    if (pipelines.lighting != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipelines.lighting, nullptr);
    if (lightingPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, lightingPipelineLayout, nullptr);
    if (lightingDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, lightingDescriptorSetLayout, nullptr);

    // skybox
    if (pipelines.skybox != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipelines.skybox, nullptr);
    if (skyboxPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, skyboxPipelineLayout, nullptr);
    if (skyboxDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, skyboxDescriptorSetLayout, nullptr);

    // postprocess
    if (pipelines.postprocess != VK_NULL_HANDLE)
        vkDestroyPipeline(device, pipelines.postprocess, nullptr);
    if (postprocessPipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, postprocessPipelineLayout, nullptr);
    if (postprocessDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, postprocessDescriptorSetLayout, nullptr);

    shaderData.buffer.Destroy();
    lightingUbo.buffer.Destroy();
    skyboxUbo.buffer.Destroy();

    if (irradianceCubeMap != nullptr)
        irradianceCubeMap->Destroy();

    if (preFilteringCubeMap != nullptr)
        preFilteringCubeMap->Destroy();

    if (specularBRDFLut != nullptr)
        specularBRDFLut->Destroy();

    if (environmentCubeMap != nullptr)
        environmentCubeMap->Destroy();
}

void DeferredPBR::InitFondation() {
    VulkanApplicationBase::InitFondation();

    Camera *camera = Singleton<Camera>::Instance();
    // camera->flipY = true;
    // camera->type = Camera::CameraType::firstperson;
    // camera->position = { 1.0f, 0.75f, 0.0f };
    // camera->SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));
    // camera->SetPerspective(60.0f, (float)width / (float)height, 0.1f, 256.0f);

    // toy
    camera->type = Camera::lookat;
    camera->SetPosition(glm::vec3(0.065f, -0.03f, -2.7f));
    camera->SetRotation(glm::vec3(187.0f, 40.0f, 0.0f));
    camera->SetPerspective(60.0f, (float) viewportWidth / (float) viewportHeight, 0.1f, 256.0f);
}

void DeferredPBR::SetupMrtRenderPass() {
    mrtRenderPass = std::make_unique<vks::VulkanRenderPass>("mrtRenderPass", vulkanDevice.get());
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
    attachmentInfo.name = "G_WorldPosition";
    attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 1: (World space) Normals
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_WorldNormal";
    attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 2: Albedo (color)
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_Color";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 3: Roughness
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_Roughness";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 4: Emissive
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_Emissive";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 5: Occlusion
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_Occlusion";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 6: G_Depth
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.name = "G_Depth";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // Attachment 7: Depth
    attachmentInfo.name = "Depth";
    attachmentInfo.binding = mrtRenderPass->AttachmentCount();
    attachmentInfo.format = depthFormat;
    attachmentInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    mrtRenderPass->AddAttachment(attachmentInfo);

    // VkFilter magFiler = VK_FILTER_NEAREST;
    // VkFilter minFiler = VK_FILTER_NEAREST;
    // VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    VkFilter magFiler = VK_FILTER_LINEAR;
    VkFilter minFiler = VK_FILTER_LINEAR;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    mrtRenderPass->AddSampler(magFiler, minFiler, addressMode);

    mrtRenderPass->CreateRenderPass();
    mrtRenderPass->CreateDescriptorSet();
}

void DeferredPBR::SetupLightingRenderPass() {
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
    attachmentInfo.name = "LightingResult";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    lightingRenderPass->AddAttachment(attachmentInfo);

    VkFilter magFiler = VK_FILTER_NEAREST;
    VkFilter minFiler = VK_FILTER_NEAREST;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    lightingRenderPass->AddSampler(magFiler, minFiler, addressMode);

    lightingRenderPass->CreateRenderPass();
    lightingRenderPass->CreateDescriptorSet();
}

void DeferredPBR::SetupSkyboxRenderPass() {
    skyboxRenderPass = std::make_unique<vks::VulkanRenderPass>("skyboxRenderPass", vulkanDevice.get());
    const uint32_t imageWidth = swapChain->imageExtent.width;
    const uint32_t imageHeight = swapChain->imageExtent.height;
    skyboxRenderPass->Init(imageWidth, imageHeight, swapChain->imageCount);

    vks::AttachmentCreateInfo attachmentInfo = {};
    attachmentInfo.width = imageWidth;
    attachmentInfo.height = imageHeight;
    attachmentInfo.layerCount = 1;
    attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // Color attachments
    attachmentInfo.binding = skyboxRenderPass->AttachmentCount();
    attachmentInfo.name = "skybox";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    skyboxRenderPass->AddAttachment(attachmentInfo);

    VkFilter magFiler = VK_FILTER_NEAREST;
    VkFilter minFiler = VK_FILTER_NEAREST;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    skyboxRenderPass->AddSampler(magFiler, minFiler, addressMode);

    skyboxRenderPass->CreateRenderPass();
    skyboxRenderPass->CreateDescriptorSet();
}

void DeferredPBR::SetupPostprocessRenderPass()
{
    postprocessRenderPass = std::make_unique<vks::VulkanRenderPass>("PostprocessRenderPass", vulkanDevice.get());
    const uint32_t imageWidth = swapChain->imageExtent.width;
    const uint32_t imageHeight = swapChain->imageExtent.height;
    postprocessRenderPass->Init(imageWidth, imageHeight, swapChain->imageCount);

    vks::AttachmentCreateInfo attachmentInfo = {};
    attachmentInfo.width = imageWidth;
    attachmentInfo.height = imageHeight;
    attachmentInfo.layerCount = 1;
    attachmentInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // Color attachments
    attachmentInfo.binding = postprocessRenderPass->AttachmentCount();
    attachmentInfo.name = "Result";
    attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    postprocessRenderPass->AddAttachment(attachmentInfo);

    VkFilter magFiler = VK_FILTER_NEAREST;
    VkFilter minFiler = VK_FILTER_NEAREST;
    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    postprocessRenderPass->AddSampler(magFiler, minFiler, addressMode);

    postprocessRenderPass->CreateRenderPass();
    postprocessRenderPass->CreateDescriptorSet();
}

void DeferredPBR::BakingIrradianceCubeMap() {
    irradianceCubeMap = std::make_unique<vks::TextureCubeMap>();
    environmentCubeMap = std::make_unique<vks::TextureCubeMap>();
    environmentCubeMap->LoadFromKtxFile(vks::helper::GetAssetPath() + "/textures/hdr/dark_room_cube.ktx",
                                        VK_FORMAT_R16G16B16A16_SFLOAT, vulkanDevice.get(), queue);

    auto tStart = std::chrono::high_resolution_clock::now();
    const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const int32_t dim = 64;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VkImageCreateInfo imageCI = vks::initializers::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    CheckVulkanResult(vkCreateImage(device, &imageCI, nullptr, &irradianceCubeMap->image));
    VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, irradianceCubeMap->image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &irradianceCubeMap->deviceMemory));
    CheckVulkanResult(vkBindImageMemory(device, irradianceCubeMap->image, irradianceCubeMap->deviceMemory, 0));
    // Image view
    VkImageViewCreateInfo viewCI = vks::initializers::ImageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = irradianceCubeMap->image;
    CheckVulkanResult(vkCreateImageView(device, &viewCI, nullptr, &irradianceCubeMap->view));
    // Sampler
    VkSamplerCreateInfo samplerCI = vks::initializers::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = static_cast<float>(numMips);
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    CheckVulkanResult(vkCreateSampler(device, &samplerCI, nullptr, &irradianceCubeMap->sampler));

    irradianceCubeMap->descriptor.imageView = irradianceCubeMap->view;
    irradianceCubeMap->descriptor.sampler = irradianceCubeMap->sampler;
    irradianceCubeMap->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    irradianceCubeMap->device = vulkanDevice.get();

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();
    VkRenderPass renderpass;
    CheckVulkanResult(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

    struct {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        VkFramebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer
    {
        // Color attachment
        VkImageCreateInfo imageCreateInfo = vks::initializers::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = dim;
        imageCreateInfo.extent.height = dim;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CheckVulkanResult(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

        VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
        CheckVulkanResult(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

        VkImageViewCreateInfo colorImageView = vks::initializers::ImageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = format;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreen.image;
        CheckVulkanResult(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

        VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::FramebufferCreateInfo();
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 1;
        fbufCreateInfo.pAttachments = &offscreen.view;
        fbufCreateInfo.width = dim;
        fbufCreateInfo.height = dim;
        fbufCreateInfo.layers = 1;
        CheckVulkanResult(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

        VkCommandBuffer layoutCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        vks::utils::SetImageLayout(
                layoutCmd,
                offscreen.image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vulkanDevice->FlushCommandBuffer(layoutCmd, queue, true);
    }

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
            setLayoutBindings);
    CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
            descriptorpool, &descriptorsetlayout, 1);
    CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
    VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
            descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &environmentCubeMap->descriptor);
    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

    // Pipeline layout
    struct PushBlock {
        glm::mat4 mvp;
        // Sampling deltas
        float deltaPhi = (2.0f * math::pi) / 180.0f;
        float deltaTheta = (0.5f * math::pi) / 64.0f;
    } pushBlock;

    VkPipelineLayout pipelinelayout;
    std::vector<VkPushConstantRange> pushConstantRanges = {
            vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 sizeof(PushBlock), 0),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState =
            vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::PipelineColorBlendStateCreateInfo(
            1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::PipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::VertexInputBindingDescription(0, sizeof(vks::geometry::VulkanGLTFModel::Vertex),
                                                             VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, pos)),
            // Location 0: Position
            vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        normal)),
            // Location 1: Normal
            vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),
            // Location 2: Texture coordinates
    };
    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();
    vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.renderPass = renderpass;
    pipelineCI.pVertexInputState = &vertexInputStateCI;

    shaderStages[0] = LoadShader(vks::helper::GetShaderBasePath() + "deferred/filtercube.vert.spv",
                                 VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = LoadShader(vks::helper::GetShaderBasePath() + "deferred/irradiancecube.frag.spv",
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipeline pipeline;
    CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render

    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.2f, 0.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = offscreen.framebuffer;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    std::vector<glm::mat4> matrices = {
            // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkViewport viewport = vks::initializers::Viewport((float) dim, (float) dim, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::Rect2D(dim, dim, 0, 0);

    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = numMips;
    subresourceRange.layerCount = 6;

    // Change image layout for all cubemap faces to transfer destination
    vks::utils::SetImageLayout(
            cmdBuf,
            irradianceCubeMap->image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

    for (uint32_t m = 0; m < numMips; m++) {
        for (uint32_t f = 0; f < 6; f++) {
            viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
            viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
            vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

            // Render scene from cube face's point of view
            vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Update shader push constant block
            pushBlock.mvp = glm::perspective(math::pi / 2.0f, 1.0f, 0.1f, 512.0f) * matrices[f];

            vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushBlock), &pushBlock);

            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0,
                                    NULL);

            skybox->Draw(cmdBuf, 0, false, pipelinelayout, 0);

            vkCmdEndRenderPass(cmdBuf);

            vks::utils::SetImageLayout(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};

            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    irradianceCubeMap->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);

            // Transform framebuffer color attachment back
            vks::utils::SetImageLayout(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    vks::utils::SetImageLayout(
            cmdBuf,
            irradianceCubeMap->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);

    vulkanDevice->FlushCommandBuffer(cmdBuf, queue);

    vkQueueWaitIdle(queue);

    vkDestroyRenderPass(device, renderpass, nullptr);
    vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
    vkFreeMemory(device, offscreen.memory, nullptr);
    vkDestroyImageView(device, offscreen.view, nullptr);
    vkDestroyImage(device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(device, descriptorpool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating irradiance cube with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
}

void DeferredPBR::BakingPreFilteringCubeMap() {
    preFilteringCubeMap = std::make_unique<vks::TextureCubeMap>();
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    const int32_t dim = 512;
    const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

    // Pre-filtered cube map
    // Image
    VkImageCreateInfo imageCI = vks::initializers::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = numMips;
    imageCI.arrayLayers = 6;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    CheckVulkanResult(vkCreateImage(device, &imageCI, nullptr, &preFilteringCubeMap->image));
    VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, preFilteringCubeMap->image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &preFilteringCubeMap->deviceMemory));
    CheckVulkanResult(vkBindImageMemory(device, preFilteringCubeMap->image, preFilteringCubeMap->deviceMemory, 0));
    // Image view
    VkImageViewCreateInfo viewCI = vks::initializers::ImageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = numMips;
    viewCI.subresourceRange.layerCount = 6;
    viewCI.image = preFilteringCubeMap->image;
    CheckVulkanResult(vkCreateImageView(device, &viewCI, nullptr, &preFilteringCubeMap->view));
    // Sampler
    VkSamplerCreateInfo samplerCI = vks::initializers::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = static_cast<float>(numMips);
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    CheckVulkanResult(vkCreateSampler(device, &samplerCI, nullptr, &preFilteringCubeMap->sampler));

    preFilteringCubeMap->descriptor.imageView = preFilteringCubeMap->view;
    preFilteringCubeMap->descriptor.sampler = preFilteringCubeMap->sampler;
    preFilteringCubeMap->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    preFilteringCubeMap->device = vulkanDevice.get();

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();
    VkRenderPass renderpass;
    CheckVulkanResult(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

    struct {
        VkImage image;
        VkImageView view;
        VkDeviceMemory memory;
        VkFramebuffer framebuffer;
    } offscreen;

    // Offfscreen framebuffer
    {
        // Color attachment
        VkImageCreateInfo imageCreateInfo = vks::initializers::ImageCreateInfo();
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format = format;
        imageCreateInfo.extent.width = dim;
        imageCreateInfo.extent.height = dim;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CheckVulkanResult(vkCreateImage(device, &imageCreateInfo, nullptr, &offscreen.image));

        VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, offscreen.image, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits,
                                                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreen.memory));
        CheckVulkanResult(vkBindImageMemory(device, offscreen.image, offscreen.memory, 0));

        VkImageViewCreateInfo colorImageView = vks::initializers::ImageViewCreateInfo();
        colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        colorImageView.format = format;
        colorImageView.flags = 0;
        colorImageView.subresourceRange = {};
        colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorImageView.subresourceRange.baseMipLevel = 0;
        colorImageView.subresourceRange.levelCount = 1;
        colorImageView.subresourceRange.baseArrayLayer = 0;
        colorImageView.subresourceRange.layerCount = 1;
        colorImageView.image = offscreen.image;
        CheckVulkanResult(vkCreateImageView(device, &colorImageView, nullptr, &offscreen.view));

        VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::FramebufferCreateInfo();
        fbufCreateInfo.renderPass = renderpass;
        fbufCreateInfo.attachmentCount = 1;
        fbufCreateInfo.pAttachments = &offscreen.view;
        fbufCreateInfo.width = dim;
        fbufCreateInfo.height = dim;
        fbufCreateInfo.layers = 1;
        CheckVulkanResult(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &offscreen.framebuffer));

        VkCommandBuffer layoutCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        vks::utils::SetImageLayout(
                layoutCmd,
                offscreen.image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vulkanDevice->FlushCommandBuffer(layoutCmd, queue, true);
    }

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 0),
    };
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
            setLayoutBindings);
    CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
            descriptorpool, &descriptorsetlayout, 1);
    CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));
    VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
            descriptorset, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &environmentCubeMap->descriptor);
    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);

    // Pipeline layout
    struct PushBlock {
        glm::mat4 mvp;
        float roughness;
        uint32_t numSamples = 1024u;
    } pushBlock;

    VkPipelineLayout pipelinelayout;
    std::vector<VkPushConstantRange> pushConstantRanges = {
            vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                                 sizeof(PushBlock), 0),
    };
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = pushConstantRanges.data();
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState =
            vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::PipelineColorBlendStateCreateInfo(
            1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::PipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables);
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages =
            {
                    LoadShader(vks::helper::GetShaderBasePath() + "deferred/filtercube.vert.spv",
                               VK_SHADER_STAGE_VERTEX_BIT),
                    LoadShader(vks::helper::GetShaderBasePath() + "deferred/prefilterenvmap.frag.spv",
                               VK_SHADER_STAGE_FRAGMENT_BIT),
            };

    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::VertexInputBindingDescription(0, sizeof(vks::geometry::VulkanGLTFModel::Vertex),
                                                             VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, pos)),
            // Location 0: Position
            vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        normal)),
            // Location 1: Normal
            vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),
            // Location 2: Texture coordinates
    };
    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();
    vertexInputStateCI.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.renderPass = renderpass;
    pipelineCI.pVertexInputState = &vertexInputStateCI;


    VkPipeline pipeline;
    CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render

    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.2f, 0.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();
    // Reuse render pass from example pass
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.framebuffer = offscreen.framebuffer;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;

    std::vector<glm::mat4> matrices = {
            // POSITIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_X
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Y
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // POSITIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            // NEGATIVE_Z
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
    };

    VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

    VkViewport viewport = vks::initializers::Viewport((float) dim, (float) dim, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::Rect2D(dim, dim, 0, 0);

    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = numMips;
    subresourceRange.layerCount = 6;

    // Change image layout for all cubemap faces to transfer destination
    vks::utils::SetImageLayout(
            cmdBuf,
            preFilteringCubeMap->image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            subresourceRange);

    for (uint32_t m = 0; m < numMips; m++) {
        pushBlock.roughness = (float) m / (float) (numMips - 1);
        for (uint32_t f = 0; f < 6; f++) {
            viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
            viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
            vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

            // Render scene from cube face's point of view
            vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Update shader push constant block
            pushBlock.mvp = glm::perspective(math::pi / 2.0f, 1.0f, 0.1f, 512.0f) * matrices[f];

            vkCmdPushConstants(cmdBuf, pipelinelayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof(PushBlock), &pushBlock);

            vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelinelayout, 0, 1, &descriptorset, 0,
                                    NULL);

            skybox->Draw(cmdBuf, 0, false, pipelinelayout, 0);

            vkCmdEndRenderPass(cmdBuf);

            vks::utils::SetImageLayout(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            // Copy region for transfer from framebuffer to cube face
            VkImageCopy copyRegion = {};

            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.mipLevel = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcOffset = {0, 0, 0};

            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.dstSubresource.baseArrayLayer = f;
            copyRegion.dstSubresource.mipLevel = m;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstOffset = {0, 0, 0};

            copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
            copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
            copyRegion.extent.depth = 1;

            vkCmdCopyImage(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    preFilteringCubeMap->image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &copyRegion);

            // Transform framebuffer color attachment back
            vks::utils::SetImageLayout(
                    cmdBuf,
                    offscreen.image,
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    vks::utils::SetImageLayout(
            cmdBuf,
            preFilteringCubeMap->image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);

    vulkanDevice->FlushCommandBuffer(cmdBuf, queue);
    vkQueueWaitIdle(queue);

    vkDestroyRenderPass(device, renderpass, nullptr);
    vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
    vkFreeMemory(device, offscreen.memory, nullptr);
    vkDestroyImageView(device, offscreen.view, nullptr);
    vkDestroyImage(device, offscreen.image, nullptr);
    vkDestroyDescriptorPool(device, descriptorpool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelinelayout, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating pre-filtered enivornment cube with " << numMips << " mip levels took " << tDiff << " ms" <<
              std::endl;
}

void DeferredPBR::BakingSpecularBRDFCubeMap() {
    specularBRDFLut = std::make_unique<vks::Texture2D>();
    auto tStart = std::chrono::high_resolution_clock::now();

    const VkFormat format = VK_FORMAT_R16G16_SFLOAT; // R16G16 is supported pretty much everywhere
    const int32_t dim = 512;

    // Image
    VkImageCreateInfo imageCI = vks::initializers::ImageCreateInfo();
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = format;
    imageCI.extent.width = dim;
    imageCI.extent.height = dim;
    imageCI.extent.depth = 1;
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    CheckVulkanResult(vkCreateImage(device, &imageCI, nullptr, &specularBRDFLut->image));
    VkMemoryAllocateInfo memAlloc = vks::initializers::MemoryAllocateInfo();
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, specularBRDFLut->image, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &specularBRDFLut->deviceMemory));
    CheckVulkanResult(vkBindImageMemory(device, specularBRDFLut->image, specularBRDFLut->deviceMemory, 0));
    // Image view
    VkImageViewCreateInfo viewCI = vks::initializers::ImageViewCreateInfo();
    viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewCI.format = format;
    viewCI.subresourceRange = {};
    viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewCI.subresourceRange.levelCount = 1;
    viewCI.subresourceRange.layerCount = 1;
    viewCI.image = specularBRDFLut->image;
    CheckVulkanResult(vkCreateImageView(device, &viewCI, nullptr, &specularBRDFLut->view));
    // Sampler
    VkSamplerCreateInfo samplerCI = vks::initializers::SamplerCreateInfo();
    samplerCI.magFilter = VK_FILTER_LINEAR;
    samplerCI.minFilter = VK_FILTER_LINEAR;
    samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCI.minLod = 0.0f;
    samplerCI.maxLod = 1.0f;
    samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    CheckVulkanResult(vkCreateSampler(device, &samplerCI, nullptr, &specularBRDFLut->sampler));

    specularBRDFLut->descriptor.imageView = specularBRDFLut->view;
    specularBRDFLut->descriptor.sampler = specularBRDFLut->sampler;
    specularBRDFLut->descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    specularBRDFLut->device = vulkanDevice.get();

    // FB, Att, RP, Pipe, etc.
    VkAttachmentDescription attDesc = {};
    // Color attachment
    attDesc.format = format;
    attDesc.samples = VK_SAMPLE_COUNT_1_BIT;
    attDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference colorReference = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;

    // Use subpass dependencies for layout transitions
    std::array<VkSubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassCI = vks::initializers::RenderPassCreateInfo();
    renderPassCI.attachmentCount = 1;
    renderPassCI.pAttachments = &attDesc;
    renderPassCI.subpassCount = 1;
    renderPassCI.pSubpasses = &subpassDescription;
    renderPassCI.dependencyCount = 2;
    renderPassCI.pDependencies = dependencies.data();

    VkRenderPass renderpass;
    CheckVulkanResult(vkCreateRenderPass(device, &renderPassCI, nullptr, &renderpass));

    VkFramebufferCreateInfo framebufferCI = vks::initializers::FramebufferCreateInfo();
    framebufferCI.renderPass = renderpass;
    framebufferCI.attachmentCount = 1;
    framebufferCI.pAttachments = &specularBRDFLut->view;
    framebufferCI.width = dim;
    framebufferCI.height = dim;
    framebufferCI.layers = 1;

    VkFramebuffer framebuffer;
    CheckVulkanResult(vkCreateFramebuffer(device, &framebufferCI, nullptr, &framebuffer));

    // Descriptors
    VkDescriptorSetLayout descriptorsetlayout;
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {};
    VkDescriptorSetLayoutCreateInfo descriptorsetlayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
            setLayoutBindings);
    CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorsetlayoutCI, nullptr, &descriptorsetlayout));

    // Descriptor Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
            vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)
    };
    VkDescriptorPoolCreateInfo descriptorPoolCI = vks::initializers::DescriptorPoolCreateInfo(poolSizes, 2);
    VkDescriptorPool descriptorpool;
    CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolCI, nullptr, &descriptorpool));

    // Descriptor sets
    VkDescriptorSet descriptorset;
    VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
            descriptorpool, &descriptorsetlayout, 1);
    CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorset));

    // Pipeline layout
    VkPipelineLayout pipelinelayout;
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(&descriptorsetlayout, 1);
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelinelayout));

    // Pipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::PipelineInputAssemblyStateCreateInfo(
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::PipelineRasterizationStateCreateInfo(
            VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    VkPipelineColorBlendAttachmentState blendAttachmentState =
            vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE);
    VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::PipelineColorBlendStateCreateInfo(
            1, &blendAttachmentState);
    VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportState = vks::initializers::PipelineViewportStateCreateInfo(1, 1);
    VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT);
    std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables);
    VkPipelineVertexInputStateCreateInfo emptyInputState = vks::initializers::PipelineVertexInputStateCreateInfo();
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages =
            {
                    shaderStages[0] = LoadShader(vks::helper::GetShaderBasePath() + "deferred/genbrdflut.vert.spv",
                                                 VK_SHADER_STAGE_VERTEX_BIT),
                    shaderStages[1] = LoadShader(vks::helper::GetShaderBasePath() + "deferred/genbrdflut.frag.spv",
                                                 VK_SHADER_STAGE_FRAGMENT_BIT),
            };

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo(pipelinelayout, renderpass);
    pipelineCI.pInputAssemblyState = &inputAssemblyState;
    pipelineCI.pRasterizationState = &rasterizationState;
    pipelineCI.pColorBlendState = &colorBlendState;
    pipelineCI.pMultisampleState = &multisampleState;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pDepthStencilState = &depthStencilState;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = shaderStages.data();
    pipelineCI.pVertexInputState = &emptyInputState;

    // Look-up-table (from BRDF) pipeline
    VkPipeline pipeline;
    CheckVulkanResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr, &pipeline));

    // Render
    VkClearValue clearValues[1];
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderpass;
    renderPassBeginInfo.renderArea.extent.width = dim;
    renderPassBeginInfo.renderArea.extent.height = dim;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = framebuffer;

    VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport = vks::initializers::Viewport((float) dim, (float) dim, 0.0f, 1.0f);
    VkRect2D scissor = vks::initializers::Rect2D(dim, dim, 0, 0);
    vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(cmdBuf, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmdBuf);
    vulkanDevice->FlushCommandBuffer(cmdBuf, queue);

    vkQueueWaitIdle(queue);

    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
    vkDestroyRenderPass(device, renderpass, nullptr);
    vkDestroyFramebuffer(device, framebuffer, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorpool, nullptr);

    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    std::cout << "Generating BRDF LUT took " << tDiff << " ms" << std::endl;
}

void DeferredPBR::Prepare() {
    VulkanApplicationBase::Prepare();

    LoadAsset();

    // baking cubemap
    BakingIrradianceCubeMap();
    BakingPreFilteringCubeMap();
    BakingSpecularBRDFCubeMap();

    // render pass
    SetupMrtRenderPass();
    SetupLightingRenderPass();
    SetupSkyboxRenderPass();
    SetupPostprocessRenderPass();

    PrepareUniformBuffers();
    SetupDescriptorSets();

    // prepare pipelines
    PrepareMrtPipeline();
    PrepareLightingPipeline();
    PrepareSkyboxPipeline();
    PreparePostprocessPipeline();
    prepared = true;
}

void DeferredPBR::LoadAsset() {
    // skybox
    uint32_t gltfLoadingFlags = vks::geometry::FileLoadingFlags::FlipY |
                                vks::geometry::FileLoadingFlags::PreTransformVertices;
    uint32_t descriptorBindingFlags = vks::geometry::DescriptorBindingFlags::ImageBaseColor |
                                      vks::geometry::DescriptorBindingFlags::ImageNormalMap;

    skybox = std::make_unique<vks::geometry::VulkanGLTFModel>();
    skybox->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/Cube/Cube.gltf", vulkanDevice.get(),
                         queue, gltfLoadingFlags, 0, 1);

    // model
    gltfLoadingFlags = vks::geometry::FileLoadingFlags::FlipY;

    gltfModel = std::make_unique<vks::geometry::VulkanGLTFModel>();
    // gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/cerberus/cerberus.gltf",
    // vulkanDevice.get(), queue, gltfLoadingFlags, descriptorBindingFlags, 1);
    gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/DamagedHelmet/DamagedHelmet.gltf",
                            vulkanDevice.get(), queue, gltfLoadingFlags, descriptorBindingFlags, 1);

    // gltfModel->LoadGLTFFile(vks::helper::GetAssetPath() + "/models/Sponza/glTF/sponza.gltf",
    // 	vulkanDevice.get(), queue, gltfLoadingFlags, descriptorBindingFlags,1);
}

void DeferredPBR::PrepareUniformBuffers() {
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

    // skybox uniform buffer
    CheckVulkanResult(vulkanDevice->CreateBuffer(
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &skyboxUbo.buffer,
            sizeof(skyboxUbo.values)));

    // Map persistent
    CheckVulkanResult(skyboxUbo.buffer.Map());

    UpdateUniformBuffers();
}

void DeferredPBR::UpdateUniformBuffers() {
    Camera *camera = Singleton<Camera>::Instance();
    shaderData.values.projection = camera->matrices.perspective;
    shaderData.values.view = camera->matrices.view;
    memcpy(shaderData.buffer.mapped, &shaderData.values, sizeof(shaderData.values));

    for (uint32_t i = 0; i < gltfModel->lights.size(); i++) {
        vks::geometry::Light light = gltfModel->lights[i];
        lightingUbo.values.lights[i].color = light.color;
        lightingUbo.values.lights[i].intensity = light.intensity;
        lightingUbo.values.lights[i].position = light.position;
    }

    // add default lights
    if (gltfModel->lights.size() == 0) {
        // y杞翠笌Houdini y杞寸浉鍙
        glm::vec3 lightPos = glm::vec3(0.0f, -1.85776f, 0.0f);
        lightingUbo.values.lights[0].color = glm::vec4(1.0f);
        lightingUbo.values.lights[0].intensity = 1.0f;
        lightingUbo.values.lights[0].position = glm::vec4(lightPos, 1.0f);

        lightPos = glm::vec3(0.149955f, -0.3774542f, 2.68973f);
        lightingUbo.values.lights[1].color = glm::vec4(1.0f);
        lightingUbo.values.lights[1].intensity = 1.0f;
        lightingUbo.values.lights[1].position = glm::vec4(lightPos, 1.0f);
    }

    lightingUbo.values.viewPos = glm::vec4(camera->position, 1.0f);
    lightingUbo.values.viewMat = camera->matrices.view;
    memcpy(lightingUbo.buffer.mapped, &lightingUbo.values, sizeof(lightingUbo.values));

    // skybox uniform buffer
    skyboxUbo.values.model = glm::scale(glm::mat4(1.0f),glm::vec3(10,10,10));
    skyboxUbo.values.view = camera->matrices.view;
    skyboxUbo.values.projection = camera->matrices.perspective;
    memcpy(skyboxUbo.buffer.mapped, &skyboxUbo.values, sizeof(skyboxUbo.values));
}

void DeferredPBR::SetupDescriptorSets() {
    if (mrtDescriptorSetLayout_Vertex != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, mrtDescriptorSetLayout_Vertex, nullptr);

    if (lightingDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, lightingDescriptorSetLayout, nullptr);

    if(postprocessDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, postprocessDescriptorSetLayout, nullptr);

    if (skyboxDescriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(device, skyboxDescriptorSetLayout, nullptr);

    if (descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    std::vector<VkDescriptorPoolSize> poolSizes = {
            // 1 for mrt pass => vertex shader, 1 for lighting fragment shader
            vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100),
            // 3 for lighting pass => fragment shader
            vks::initializers::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100),

    };
    // for mrt pass
    {
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(
                poolSizes, 100 * maxFrameInFlight);
        CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

        // Descriptor set layout for passing matrices
        VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
                &setLayoutBinding, 1);
        CheckVulkanResult(
                vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &mrtDescriptorSetLayout_Vertex));

        // Descriptor set for scene matrices
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(
                descriptorPool, &mrtDescriptorSetLayout_Vertex, 1);
        mrtDescriptorSets_Vertex.resize(maxFrameInFlight);
        for (uint32_t i = 0; i < maxFrameInFlight; i++) {
            CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &mrtDescriptorSets_Vertex[i]));
            VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    mrtDescriptorSets_Vertex[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &shaderData.buffer.descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
        }
    }
    // for lighting pass
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
                {
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 0),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 1),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 2),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 3),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 4),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 5),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 6),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 7),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 8),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT, 9),
                        vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                      VK_SHADER_STAGE_FRAGMENT_BIT,
                                                                      10),
                };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
                setLayoutBindings);
        CheckVulkanResult(
                vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &lightingDescriptorSetLayout));
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool,
                                                                                             &lightingDescriptorSetLayout,
                                                                                             1);
        lightingDescriptorSets.resize(maxFrameInFlight);
        for (uint32_t i = 0; i < maxFrameInFlight; i++) {
            CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &lightingDescriptorSets[i]));
            vks::FrameBuffer *frameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(i);
            // attachment
            int binding = 0;
            for (uint32_t s = 0; s < frameBuffer->attachments.size(); s++) {
                if (frameBuffer->attachments[s].IsDepthStencil()) continue;
                VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                        lightingDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding,
                        &frameBuffer->attachments[s].descriptor);
                vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
                binding++;
            }

            // IrradianceCube
            VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    lightingDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    binding, &irradianceCubeMap->descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;

            // PreFilteringCube
            writeDescriptorSet = vks::initializers::WriteDescriptorSet(lightingDescriptorSets[i],
                                                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                       binding, &preFilteringCubeMap->descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;

            // SpecularBRDFCube
            writeDescriptorSet = vks::initializers::WriteDescriptorSet(lightingDescriptorSets[i],
                                                                       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                                       binding, &specularBRDFLut->descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;

            // lighting uniform buffer
            writeDescriptorSet = vks::initializers::WriteDescriptorSet(lightingDescriptorSets[i],
                                                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                                       binding, &lightingUbo.buffer.descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;
        }
    }

    // for skybox pass
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
        {
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                                          VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
                setLayoutBindings.data(), setLayoutBindings.size());
        CheckVulkanResult(
                vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &skyboxDescriptorSetLayout));
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool,
                                                                                             &skyboxDescriptorSetLayout,
                                                                                             1);
        skyboxDescriptorSets.resize(maxFrameInFlight);

        for (uint32_t i = 0; i < maxFrameInFlight; i++) {

            CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &skyboxDescriptorSets[i]));
            int binding = 0;

            // update uniform buffer
            VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    skyboxDescriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, binding,
                    &skyboxUbo.buffer.descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;

            // update env cube map
            writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    skyboxDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding,
                    &environmentCubeMap->descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;
        }
    }

//     for postprocess pass
    {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
        {
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          VK_SHADER_STAGE_FRAGMENT_BIT, 2)
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(
                setLayoutBindings.data(), setLayoutBindings.size());
        CheckVulkanResult(
                vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &postprocessDescriptorSetLayout));
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(descriptorPool,
                                                                                             &postprocessDescriptorSetLayout,
                                                                                             1);
        postprocessDescriptorSets.resize(maxFrameInFlight);

        for (uint32_t i = 0; i < maxFrameInFlight; i++) {
            CheckVulkanResult(vkAllocateDescriptorSets(device, &allocInfo, &postprocessDescriptorSets[i]));
            vks::FrameBuffer *frameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(i);
            int binding = 0;
            // update mrt depth
            for (uint32_t s = 0; s < frameBuffer->attachments.size(); s++) {
                if (frameBuffer->attachments[s].name != "G_Depth") continue;
                VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                        postprocessDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding,
                        &frameBuffer->attachments[s].descriptor);
                vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
                binding++;
            }
            // update light result
            vks::FrameBuffer *lightingFrameBuffer = lightingRenderPass->vulkanFrameBuffer->GetFrameBuffer(i);
            vks::FramebufferAttachment &lightingResultAttachment = lightingFrameBuffer->attachments[0];
            VkWriteDescriptorSet writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    postprocessDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding,
                    &lightingResultAttachment.descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;

            // update env cube map
            vks::FrameBuffer *skyboxFrameBuffer = skyboxRenderPass->vulkanFrameBuffer->GetFrameBuffer(i);
            auto skyboxResultAttachment = skyboxFrameBuffer->attachments[0];
            writeDescriptorSet = vks::initializers::WriteDescriptorSet(
                    postprocessDescriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding,
                    &skyboxResultAttachment.descriptor);
            vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
            binding++;
        }
    }
}

void DeferredPBR::PrepareMrtPipeline() {
    // create pipeline layout
    // Pipeline layout using both descriptor sets (set 0 = matrices, set 1 = material)
    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.push_back(mrtDescriptorSetLayout_Vertex);
    setLayouts.push_back(vks::geometry::descriptorSetLayoutImage);
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(
            setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // We will use push constants to push the local matrices of a primitive to the vertex shader
    VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(
            VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    // Push constant ranges are part of the pipeline layout
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &mrtPipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
            vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
            vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                                                    VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates =
            {
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            };
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(
            blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::VertexInputBindingDescription(0, sizeof(vks::geometry::VulkanGLTFModel::Vertex),
                                                             VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, pos)),
            // Location 0: Position
            vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),
            // Location 1: Texture coordinates
            vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, color)),
            // Location 2: Color
            vks::initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        normal)),
            // Location 3: Normal
            vks::initializers::VertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        tangent)),
            // Location 4: Tangent
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
        CheckVulkanResult(
                vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCI, nullptr,
                                          &pipelines.offscreenWireframe));
    }
}

void DeferredPBR::PrepareLightingPipeline() {
    // create pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts = {lightingDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(
            setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // We will use push constants to push the local matrices of a primitive to the vertex shader
    // VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    // Push constant ranges are part of the pipeline layout
    pipelineLayoutCI.pushConstantRangeCount = 0;
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &lightingPipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
            vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
            vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                                                    VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
    std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates =
            {
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            };
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(
            blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
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

void DeferredPBR::PrepareSkyboxPipeline() {
    // create pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts = {skyboxDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(
            setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // We will use push constants to push the local matrices of a primitive to the vertex shader
    // VkPushConstantRange pushConstantRange = vks::initializers::PushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
    // Push constant ranges are part of the pipeline layout
    pipelineLayoutCI.pushConstantRangeCount = 0;
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &skyboxPipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
            vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
            vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                                                    VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
//    rasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
    std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates =
            {
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            };
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(
            blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);

    // Vertex input bindings and attributes
    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::VertexInputBindingDescription(0, sizeof(vks::geometry::VulkanGLTFModel::Vertex),
                                                             VK_VERTEX_INPUT_RATE_VERTEX),
    };
    const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            // Location 0: Position
            vks::initializers::VertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, pos)),
            // Location 1: Texture coordinates
            vks::initializers::VertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, uv)),
            // Location 2: Color
            vks::initializers::VertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex, color)),
            // Location 3: Normal
            vks::initializers::VertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        normal)),
            // Location 4: Tangent
            vks::initializers::VertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32A32_SFLOAT,
                                                               offsetof(vks::geometry::VulkanGLTFModel::Vertex,
                                                                        tangent)),
    };

    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();

    vertexInputStateCI.pVertexBindingDescriptions = vertexInputBindings.data();
    vertexInputStateCI.vertexBindingDescriptionCount = vertexInputBindings.size();
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();
    vertexInputStateCI.vertexAttributeDescriptionCount = vertexInputAttributes.size();

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            LoadShader(vks::helper::GetShaderBasePath() + "deferred/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            LoadShader(vks::helper::GetShaderBasePath() + "deferred/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo();
    pipelineCI.layout = skyboxPipelineLayout;
    pipelineCI.renderPass = skyboxRenderPass->renderPass;
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
    CheckVulkanResult(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipelines.skybox));
}

void DeferredPBR::PreparePostprocessPipeline()
{
    // create pipeline layout
    std::vector<VkDescriptorSetLayout> setLayouts = {postprocessDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutCI = vks::initializers::PipelineLayoutCreateInfo(
            setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // Push constant ranges are part of the pipeline layout
    pipelineLayoutCI.pushConstantRangeCount = 0;
    CheckVulkanResult(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &postprocessPipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCI =
            vks::initializers::PipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
    VkPipelineRasterizationStateCreateInfo rasterizationStateCI =
            vks::initializers::PipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
                                                                    VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
    rasterizationStateCI.cullMode = VK_CULL_MODE_FRONT_BIT;
    std::array<VkPipelineColorBlendAttachmentState, 1> blendAttachmentStates =
            {
                    vks::initializers::PipelineColorBlendAttachmentState(0xf, VK_FALSE),
            };
    VkPipelineColorBlendStateCreateInfo colorBlendStateCI = vks::initializers::PipelineColorBlendStateCreateInfo(
            blendAttachmentStates.size(), blendAttachmentStates.data());
    VkPipelineDepthStencilStateCreateInfo depthStencilStateCI = vks::initializers::PipelineDepthStencilStateCreateInfo(
            VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
    VkPipelineViewportStateCreateInfo viewportStateCI = vks::initializers::PipelineViewportStateCreateInfo(1, 1, 0);
    VkPipelineMultisampleStateCreateInfo multisampleStateCI = vks::initializers::PipelineMultisampleStateCreateInfo(
            VK_SAMPLE_COUNT_1_BIT, 0);
    const std::vector<VkDynamicState> dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateCI = vks::initializers::PipelineDynamicStateCreateInfo(
            dynamicStateEnables.data(), static_cast<uint32_t>(dynamicStateEnables.size()), 0);
    VkPipelineVertexInputStateCreateInfo vertexInputStateCI = vks::initializers::PipelineVertexInputStateCreateInfo();

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            LoadShader(vks::helper::GetShaderBasePath() + "deferred/postprocess.vert.spv", VK_SHADER_STAGE_VERTEX_BIT),
            LoadShader(vks::helper::GetShaderBasePath() + "deferred/postprocess.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
    };

    VkGraphicsPipelineCreateInfo pipelineCI = vks::initializers::PipelineCreateInfo();
    pipelineCI.layout = postprocessPipelineLayout;
    pipelineCI.renderPass = postprocessRenderPass->renderPass;
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
    CheckVulkanResult(vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineCI, nullptr, &pipelines.postprocess));
}

void DeferredPBR::PrepareRenderPass(VkCommandBuffer commandBuffer) {
    // mrt render pass
    {
        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

        vks::FrameBuffer *mrtFrameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
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
                        {0.0f, 0.0f, 0.0f, 1.0f},
                        {1.0f, 1.0f, 1.0f, 1.0f},
                };

        VkClearValue depthClearValue;
        depthClearValue.depthStencil = {1.0f, 0};
        clearValues.push_back(depthClearValue);

        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport =
                vks::initializers::Viewport((float) viewportWidth, (float) viewportHeight, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // Bind scene matrices descriptor to set 0
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mrtPipelineLayout, 0, 1,
                                &mrtDescriptorSets_Vertex[currentFrame], 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          wireframe ? pipelines.offscreenWireframe : pipelines.offscreen);
        gltfModel->Draw(commandBuffer, vks::geometry::RenderFlags::BindImages, true, mrtPipelineLayout, 1);
        vkCmdEndRenderPass(commandBuffer);
    }

    // lighting renderPass
    {
        vks::FrameBuffer *lightingFrameBuffer = lightingRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

        renderPassBeginInfo.renderPass = lightingRenderPass->renderPass;
        renderPassBeginInfo.framebuffer = lightingFrameBuffer->frameBuffer;
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {viewportWidth, viewportHeight};

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].color = {0.0f, 0.0f, 0.0f, 1.0f};

        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport =
                vks::initializers::Viewport((float) viewportWidth, (float) viewportHeight, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightingPipelineLayout, 0, 1,
                                &lightingDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.lighting);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);
    }

    // skybox renderPass
    {
        vks::FrameBuffer *skyboxFrameBuffer = skyboxRenderPass->vulkanFrameBuffer->GetFrameBuffer(
                currentFrame);
        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

        renderPassBeginInfo.renderPass = skyboxRenderPass->renderPass;
        renderPassBeginInfo.framebuffer = skyboxFrameBuffer->frameBuffer;
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {viewportWidth, viewportHeight};

        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};

        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport =
                vks::initializers::Viewport((float) viewportWidth, (float) viewportHeight, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipelineLayout, 0, 1,
                                &skyboxDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
//        vkCmdDraw(commandBuffer,3,1,0,0);
        skybox->Draw(commandBuffer, 0, false, skyboxPipelineLayout, 0);
        vkCmdEndRenderPass(commandBuffer);
    }

    // postprocess renderPass
    {
        vks::FrameBuffer *postprocessFrameBuffer = postprocessRenderPass->vulkanFrameBuffer->GetFrameBuffer(
                currentFrame);
        VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

        renderPassBeginInfo.renderPass = postprocessRenderPass->renderPass;
        renderPassBeginInfo.framebuffer = postprocessFrameBuffer->frameBuffer;
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {viewportWidth, viewportHeight};

        std::array<VkClearValue, 1> clearValues{};
        clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};

        renderPassBeginInfo.clearValueCount = clearValues.size();
        renderPassBeginInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        const VkViewport viewport =
                vks::initializers::Viewport((float) viewportWidth, (float) viewportHeight, 0.0f, 1.0f);
        const VkRect2D scissor = vks::initializers::Rect2D(viewportWidth, viewportHeight, 0, 0);

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postprocessPipelineLayout, 0, 1,
                                &postprocessDescriptorSets[currentFrame], 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.postprocess);
        vkCmdDraw(commandBuffer,3,1,0,0);
        vkCmdEndRenderPass(commandBuffer);
    }
}

void DeferredPBR::ReCreateVulkanResource_Child() {
    mrtRenderPass.reset();
    SetupMrtRenderPass();

    lightingRenderPass.reset();
    SetupLightingRenderPass();

    skyboxRenderPass.reset();
    SetupSkyboxRenderPass();

    postprocessRenderPass.reset();
    SetupPostprocessRenderPass();

    SetupDescriptorSets();
}

void DeferredPBR::NewGUIFrame() {
    if (ImGui::Begin("UI_GBuffer_View")) {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        vks::FrameBuffer *frameBuffer = mrtRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
        float scale = std::min(viewportPanelSize.x / (float) viewportWidth,
                               viewportPanelSize.y / (float) viewportHeight);
        for (uint32_t i = 0; i < frameBuffer->attachments.size(); i++) {
            if (!frameBuffer->attachments[i].HasDepth() && !frameBuffer->attachments[i].HasStencil())
                ImGui::Image((ImTextureID) frameBuffer->attachments[i].descriptorSet,
                             ImVec2(viewportWidth * scale, viewportHeight * scale));
        }

        ImGui::End();
    }

    if (ImGui::Begin("UI_View", nullptr, ImGuiWindowFlags_ForwardBackend)) {
        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        vks::FrameBuffer* frameBuffer = postprocessRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
//        vks::FrameBuffer *frameBuffer = lightingRenderPass->vulkanFrameBuffer->GetFrameBuffer(currentFrame);
        float scale = std::min(viewportPanelSize.x / (float) viewportWidth,
                               viewportPanelSize.y / (float) viewportHeight);
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 imageSize = ImVec2(viewportWidth * scale, viewportHeight * scale);
        ImGui::SetCursorPos(ImVec2((windowSize.x - imageSize.x) * 0.5f, (windowSize.y - imageSize.y) * 0.5f));
        ImGui::Image((ImTextureID) frameBuffer->attachments[0].descriptorSet, imageSize);
        ImGui::End();
    }

    if (ImGui::Begin("UI_Status")) {
        if (ImGui::CollapsingHeader("Camera")) {
            Camera *camera = Singleton<Camera>::Instance();
            ImGui::Text("Camera Position: (%g, %g, %g)",
                        camera->position.x, camera->position.y, camera->position.z);
            ImGui::Text("Camera Rotation: (%g, %g, %g)",
                        camera->rotation.x, camera->rotation.y, camera->rotation.z);
        }

        if (ImGui::CollapsingHeader("Performance")) {
        }

        ImGui::End();
    }

    // ImGui::ShowDemoWindow();
}

void DeferredPBR::ViewChanged() {
    UpdateUniformBuffers();
}

void DeferredPBR::Render() {
    RenderFrame();
    Camera *camera = Singleton<Camera>::Instance();

    if (camera->updated)
        UpdateUniformBuffers();
}