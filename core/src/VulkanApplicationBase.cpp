#include <iostream>
#include <vector>
#include <VulkanApplicationBase.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <VulkanHelper.h>
#include <VulkanDebug.h>
#include <VulkanDevice.h>

#include <VulkanFrontend.h>
#include <VulkanInitializers.h>
#include <VulkanUtils.h>

#include <Camera.h>
#include <Singleton.hpp>
#include <VulkanImGUI.h>
#include <GraphicSettings.hpp>
#include <backends/imgui_impl_glfw.h>

#include "InputManager.h"

VulkanApplicationBase::VulkanApplicationBase(std::string applicationName,uint32_t width, uint32_t height)
{
    this->title = applicationName;
    this->name = applicationName;
    this->width = width;
    this->height = height;
}

VulkanApplicationBase::~VulkanApplicationBase()
{
    CheckVulkanResult(vkDeviceWaitIdle(device));
    
    swapChain.reset();

    if (descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    if (renderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(device, renderPass, nullptr);

    for (uint32_t i = 0; i < frameBuffers.size(); i++)
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);

    for (auto& shaderModule : shaderModules)
        vkDestroyShaderModule(device, shaderModule, nullptr);

    // depth image info
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);

	// offscreen pass resource
	offscreenPass.reset();
	
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
    
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
    vkDestroyCommandPool(device,cmdPool,nullptr);

    // barrier
    for(auto& semaphore : semaphores)
    {
        vkDestroySemaphore(device, semaphore.presentComplete, nullptr);
        vkDestroySemaphore(device, semaphore.renderComplete, nullptr);
    }
    for (auto& fence : waitFences) 
        vkDestroyFence(device, fence, nullptr);
    
    vulkanDevice.reset();
    if (Singleton<GraphicSettings>::Instance()->validation)
        vks::debug::freeDebugCallback(instance);
    vkDestroyInstance(instance,nullptr);
}

void VulkanApplicationBase::InitFondation()
{
	Singleton<InputManager>::Init();
}

bool VulkanApplicationBase::InitVulkan()
{
    SetupWindows();

    auto graphicSettings = Singleton<GraphicSettings>::Instance();
    
    CheckVulkanResult(CreateInstance(graphicSettings->validation));

    // If requested, we enable the default validation layers for debugging
    if (graphicSettings->validation)
        vks::debug::setupDebugging(instance);

    // create surface from frontend
    CreateWindowsSurface();

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    CheckVulkanResult(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    if (gpuCount == 0) {
        vks::helper::ExitFatal("No device with Vulkan support found", -1);
        return false;
    }
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    CheckVulkanResult(vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));

    // select default GPU in most of time
    int selectedDevice = 0;
    physicalDevice = physicalDevices[selectedDevice];

    if(physicalDevice == VK_NULL_HANDLE)
        vks::helper::ExitFatal("failed to find a suitable GPU!",-1);

    // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    // derived class can override this to set actual features (based on above readings) to enable for logical device creation
    GetEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    vulkanDevice = std::make_unique<vks::VulkanDevice>(physicalDevice);
    
    // derived class can enable extensions based on the list of supported extensions read from the physical device
    GetEnabledExtensions();
    
    CheckVulkanResult(vulkanDevice->CreateLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain));
    device = vulkanDevice->logicalDevice;

    // Get a graphics queue from the device
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

    // Find a suitable depth and/or stencil format
    VkBool32 validFormat{ false };
    // Sample that make use of stencil will require a depth + stencil format, so we select from a different list
    if (Singleton<GraphicSettings>::Instance()->requiresStencil) {
        validFormat = vks::utils::GetSupportedDepthStencilFormat(physicalDevice, &depthFormat);
    } else {
        validFormat = vks::utils::GetSupportedDepthFormat(physicalDevice, &depthFormat);
    }
    assert(validFormat);

    swapChain = std::make_unique<VulkanSwapChain>(instance,physicalDevice,device);
    
    return true;
}

bool VulkanApplicationBase::SetupWindows()
{
    // glfw
#ifdef USE_FRONTEND_GLFW
    glfwSetErrorCallback(vks::frontend::GLFW::GlfwErrorCallback);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title.c_str(), nullptr, nullptr);
    if(!glfwVulkanSupported())
    {
        vks::helper::ExitFatal("GLFW: Vulkan Not Supported\n",-1);
        return true;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, vks::frontend::GLFW::FrameBufferResizeCallback);
    glfwSetMouseButtonCallback(window, vks::frontend::GLFW::MouseButtonCallback);
    glfwSetKeyCallback(window, vks::frontend::GLFW::KeyBoardCallback);
	glfwSetCursorPosCallback(window, vks::frontend::GLFW::CursorPosCallback);
	
#endif

    return true;
}

void VulkanApplicationBase::CreateWindowsSurface()
{
#if USE_FRONTEND_GLFW
    CheckVulkanResult(glfwCreateWindowSurface(instance,window,nullptr,&surface));
#endif
}

void VulkanApplicationBase::DestroyWindows()
{
#if USE_FRONTEND_GLFW
    if(window != nullptr)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
#endif
}

VkResult VulkanApplicationBase::CreateInstance(bool enableValidation)
{
    auto graphicSettings = Singleton<GraphicSettings>::Instance();
    
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = name.c_str();
    appInfo.pEngineName = name.c_str();
    appInfo.apiVersion = apiVersion;

//     std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };
//     // Enable surface extensions depending on os
// #if defined(_WIN32)
//     instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
// #endif

    // equal to the below
    std::vector<const char*> instanceExtensions;
    uint32_t extensionsCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionsCount);
    for (uint32_t i = 0; i < extensionsCount; i++)
        instanceExtensions.push_back(glfwExtensions[i]);

    // Get extensions supported by the instance and store for later use
    uint32_t extCount = 0;
    CheckVulkanResult(vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr));
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties extension : extensions)
            {
                supportedInstanceExtensions.emplace_back(extension.extensionName);
            }
        }
    }

    // Enabled requested instance extensions
    if (enabledInstanceExtensions.size() > 0) 
    {
        for (const char * enabledExtension : enabledInstanceExtensions) 
        {
            // Output message if requested extension is not available
            if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.emplace_back(enabledExtension);
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    // Enable the debug utils extension if available (e.g. when debugging tools are present)
    if (graphicSettings->validation || std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end()) {
        instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (instanceExtensions.size() > 0)
    {
        instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (graphicSettings->validation)
    {
        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        CheckVulkanResult(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        CheckVulkanResult(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data()));
        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties)
        {
            if (strcmp(layer.layerName, validationLayerName) == 0)
            {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent)
        {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        } else
        {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    VkResult result = CheckVulkanResult(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    // If the debug utils extension is present we set up debug functions, so samples an label objects for debugging
    if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end())
    {
        vks::debugutils::setup(instance);
    }

    return result;
}

void VulkanApplicationBase::Prepare()
{
    InitSwapchain();
    SetupSwapChain();
    CreateCommandPool();
    CreateCommandBuffers();
    CreateSynchronizationPrimitives();

    // default on-screen vulkan resource
    SetupDefaultDepthStencil();
    SetupDefaultRenderPass();
    SetupDefaultFrameBuffer();

	// default off-screen vulkan resource
	SetupOffscreenResource();
	SetupOffscreenRenderPass();
	SetupOffscreenFrameBuffer();
	
	CreateDefaultPipelineCache();
    
    SetupCamera();

    auto graphicSettings = Singleton<GraphicSettings>::Instance();
    if(graphicSettings->enableGUI)
    {
        ImGUICreateInfo imGUICreateInfo;
        imGUICreateInfo.instance = instance;
        imGUICreateInfo.vulkanDevice = vulkanDevice.get();
        imGUICreateInfo.vulkanSwapChain = swapChain.get();
        imGUICreateInfo.glfwWindow = window;
        imGUICreateInfo.copyQueue = queue;
        
        if(graphicSettings->standaloneGUI)
	        imGUICreateInfo.renderPass = VK_NULL_HANDLE;
        else
            imGUICreateInfo.renderPass = renderPass;

        // gui = std::make_unique<VulkanGUI>(imGUICreateInfo);
        gui = Singleton<VulkanGUI>::Instance(imGUICreateInfo);
    }
}

void VulkanApplicationBase::InitSwapchain()
{
    swapChain->Init(window,surface);
}

void VulkanApplicationBase::CreateCommandPool()
{
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = swapChain->queueNodeIndex;
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    CheckVulkanResult(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));   
}

void VulkanApplicationBase::SetupSwapChain()
{
    auto graphicSettings = Singleton<GraphicSettings>::Instance();

    swapChain->Create(&width,&height,graphicSettings->vsync,graphicSettings->fullscreen);
    maxFrameInFlight = swapChain->imageCount;
}

void VulkanApplicationBase::CreateCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    drawCmdBuffers.resize(maxFrameInFlight);

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
        vks::initializers::CommandBufferAllocateInfo(
            cmdPool,
            VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            static_cast<uint32_t>(drawCmdBuffers.size()));

    CheckVulkanResult(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));   
}

void VulkanApplicationBase::CreateSynchronizationPrimitives()
{
    // Create synchronization objects
    semaphores.resize(maxFrameInFlight);
    waitFences.resize(maxFrameInFlight);
    VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::SemaphoreCreateInfo();
    for(auto& semaphore:semaphores)
    {
        // Create a semaphore used to synchronize image presentation
        // Ensures that the image is displayed before we start submitting new commands to the queue
        CheckVulkanResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore.presentComplete));
        // Create a semaphore used to synchronize command submission
        // Ensures that the image is not presented until all commands have been submitted and executed
        CheckVulkanResult(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphore.renderComplete));
    }
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCreateInfo = vks::initializers::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    for (auto& fence : waitFences) {
        CheckVulkanResult(vkCreateFence(device, &fenceCreateInfo, nullptr, &fence));
    }
}

void VulkanApplicationBase::SetupDefaultDepthStencil()
{
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = depthFormat;
    imageCI.extent = { width, height, 1 };
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    CheckVulkanResult(vkCreateImage(device, &imageCI, nullptr, &depthStencil.image));
    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);

    VkMemoryAllocateInfo memAllloc{};
    memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAllloc.allocationSize = memReqs.size;
    memAllloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    CheckVulkanResult(vkAllocateMemory(device, &memAllloc, nullptr, &depthStencil.mem));
    CheckVulkanResult(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCI.image = depthStencil.image;
    imageViewCI.format = depthFormat;
    imageViewCI.subresourceRange.baseMipLevel = 0;
    imageViewCI.subresourceRange.levelCount = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount = 1;
    imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
    if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
        imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    CheckVulkanResult(vkCreateImageView(device, &imageViewCI, nullptr, &depthStencil.view));
}

void VulkanApplicationBase::SetupDefaultRenderPass()
{
    std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain->colorFormat;
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

	CheckVulkanResult(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanApplicationBase::ViewChanged()
{
    
}

void VulkanApplicationBase::WindowResized()
{
    
}

void VulkanApplicationBase::BuildCommandBuffers(VkCommandBuffer commandBuffer)
{
    
}

void VulkanApplicationBase::DestroyCommandBuffers()
{
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanApplicationBase::CreateDefaultPipelineCache()
{
    VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    CheckVulkanResult(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanApplicationBase::SetupDefaultFrameBuffer()
{
    VkImageView attachments[2];

    // Depth/Stencil attachment is the same for all frame buffers
    attachments[1] = depthStencil.view;

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = renderPass;
    frameBufferCreateInfo.attachmentCount = 2;
    frameBufferCreateInfo.pAttachments = attachments;
    frameBufferCreateInfo.width = width;
    frameBufferCreateInfo.height = height;
    frameBufferCreateInfo.layers = 1;

    // Create frame buffers for every swap chain image
    frameBuffers.resize(maxFrameInFlight);
    for (uint32_t i = 0; i < frameBuffers.size(); i++)
    {
        attachments[0] = swapChain->buffers[i].view;
        CheckVulkanResult(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanApplicationBase::SetupOffscreenResource()
{
	if(offscreenPass == nullptr)
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
	VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::DescriptorPoolCreateInfo(poolSizes, maxFrameInFlight);
	CheckVulkanResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &offscreenPass->descriptorPool));
	VkDescriptorSetLayoutBinding setLayoutBinding = vks::initializers::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::DescriptorSetLayoutCreateInfo(&setLayoutBinding, 1);
	CheckVulkanResult(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &offscreenPass->descriptorSetLayout));
	
	// Descriptor set for scene matrices
	VkDescriptorSetAllocateInfo allocInfo = vks::initializers::DescriptorSetAllocateInfo(offscreenPass->descriptorPool, &offscreenPass->descriptorSetLayout, 1);
	
	for(uint32_t i = 0; i < maxFrameInFlight; i++)
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
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass->color[i].mem));
		CheckVulkanResult(vkBindImageMemory(device, offscreenPass->color[i].image, offscreenPass->color[i].mem, 0));

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
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		CheckVulkanResult(vkAllocateMemory(device, &memAlloc, nullptr, &offscreenPass->depth[i].mem));
		CheckVulkanResult(vkBindImageMemory(device, offscreenPass->depth[i].image, offscreenPass->depth[i].mem, 0));

		VkImageViewCreateInfo depthStencilView = vks::initializers::ImageViewCreateInfo();
		depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		depthStencilView.format = depthFormat;
		depthStencilView.flags = 0;
		depthStencilView.subresourceRange = {};
		depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
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

void VulkanApplicationBase::SetupOffscreenRenderPass()
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

	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

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

void VulkanApplicationBase::SetupOffscreenFrameBuffer()
{
	offscreenPass->frameBuffer.resize(maxFrameInFlight);
	for(uint32_t i = 0; i < maxFrameInFlight; i++)
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

void VulkanApplicationBase::NextFrame()
{
	auto tStart = std::chrono::high_resolution_clock::now();
	if(viewUpdated)
	{
		viewUpdated = false;
		ViewChanged();
	}

	Render();
	frameCounter++;
    auto tEnd = std::chrono::high_resolution_clock::now();
    auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
    frameTimer = (float)tDiff / 1000.0f;
	
	// update camera
	Camera* camera = Singleton<Camera>::Instance();
	camera->Update(frameTimer);
	if (camera->Moving())
	{
		viewUpdated = true;
	}
	
    // Convert to clamped timer value
    if (!paused)
    {
        timer += timerSpeed * frameTimer;
        if (timer > 1.0f)
            timer -= 1.0f;
    }
    float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());
    if (fpsTimer > 1000.0f)
    {
        lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
        frameCounter = 0;
        lastTimestamp = tEnd;
    }
    tPrevEnd = tEnd;
    // update UI
}

void VulkanApplicationBase::RenderLoop()
{
    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if(prepared)
            NextFrame();
    }

    auto graphicSettings = Singleton<GraphicSettings>::Instance();
    if(graphicSettings->enableGUI)
        Singleton<VulkanGUI>::Reset();
    
    glfwDestroyWindow(window);
    glfwTerminate();
}

void VulkanApplicationBase::ReCreateVulkanResource()
{
    if (!prepared) return;
    prepared = false;

    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    CheckVulkanResult(vkDeviceWaitIdle(device));
    this->width = width;
    this->height = height;
	
    SetupSwapChain();
    // Recreate the frame buffers
    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);
    SetupDefaultDepthStencil();
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
        vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
    }
    SetupDefaultFrameBuffer();

	// offscreen pass
	offscreenPass->DestroyResource();
	SetupOffscreenResource();
	SetupOffscreenFrameBuffer();
    // ui overlay resize
	gui->Resize(width,height);
	
    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    DestroyCommandBuffers();
    CreateCommandBuffers();
	
    // SRS - Recreate fences in case number of swapchain images has changed on resize
    for(auto& semaphore : semaphores)
    {
        vkDestroySemaphore(device, semaphore.presentComplete, nullptr);
        vkDestroySemaphore(device, semaphore.renderComplete, nullptr);
    }
    for (auto& fence : waitFences) {
        vkDestroyFence(device, fence, nullptr);
    }
    CreateSynchronizationPrimitives();

    CheckVulkanResult(vkDeviceWaitIdle(device));

    Camera* camera = Singleton<Camera>::Instance();

    if ((width > 0.0f) && (height > 0.0f)) {
        camera->UpdateAspectRatio((float)width / (float)height);
    }

    // Notify derived class
    WindowResized();
    ViewChanged();

    currentFrame = 0;
    prepared = true;
}

void VulkanApplicationBase::GetEnabledExtensions()
{
    
}

void VulkanApplicationBase::GetEnabledFeatures()
{
    // Fill mode non solid is required for wireframe display
    if (deviceFeatures.fillModeNonSolid)
        enabledFeatures.fillModeNonSolid = VK_TRUE;
}

VkPipelineShaderStageCreateInfo VulkanApplicationBase::LoadShader(std::string fileName, VkShaderStageFlagBits stage)
{
    VkPipelineShaderStageCreateInfo shaderStage = {};
    shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device);
#else
    shaderStage.module = vks::utils::LoadShader(fileName.c_str(), device);
#endif
    shaderStage.pName = "main";
    assert(shaderStage.module != VK_NULL_HANDLE);
    shaderModules.push_back(shaderStage.module);
    return shaderStage;
}

void VulkanApplicationBase::SetupCamera()
{
   
}

void VulkanApplicationBase::RenderFrame()
{
    PrepareFrame();
    // Set up submit info structure
    submitInfo = vks::initializers::SubmitInfo();
    submitInfo.pWaitDstStageMask = &submitPipelineStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores[currentFrame].presentComplete;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores[currentFrame].renderComplete;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &drawCmdBuffers[currentFrame];
    CheckVulkanResult(vkQueueSubmit(queue, 1, &submitInfo, waitFences[currentFrame]));
    SubmitFrame();
}

void VulkanApplicationBase::NewGUIFrame()
{
	
}

void VulkanApplicationBase::DrawDockingWindows(bool fullscreen, bool padding)
{
	ImGuiDockNodeFlags dockSpaceFlags = ImGuiDockNodeFlags_AutoHideTabBar;

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (fullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}
	else
		dockSpaceFlags &= ~ImGuiDockNodeFlags_PassthruCentralNode;

	if (!padding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("UI_Engine", nullptr, windowFlags);
	if (!padding)
		ImGui::PopStyleVar();

	if (fullscreen)
		ImGui::PopStyleVar(2);

	// Submit the DockSpace
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("UI_DockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockSpaceFlags);
	}
	ImGui::End();
}

void VulkanApplicationBase::PrepareFrame()
{
	GraphicSettings* graphicSettings = Singleton<GraphicSettings>::Instance();
    //wait pre-frame
    CheckVulkanResult(vkWaitForFences(device, 1, &waitFences[currentFrame], VK_TRUE, UINT64_MAX));
    // Acquire the next image from the swap chain
    CheckVulkanResult(swapChain->AcquireNextImage(semaphores[currentFrame].presentComplete, &currentImageIndex));
    // Only reset the fence if we are submitting work
    CheckVulkanResult(vkResetFences(device, 1, &waitFences[currentFrame]));

    // begin render pass
    VkCommandBufferBeginInfo beginInfo = vks::initializers::CommandBufferBeginInfo();
    CheckVulkanResult(vkBeginCommandBuffer(drawCmdBuffers[currentFrame], &beginInfo));

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::RenderPassBeginInfo();

	if(graphicSettings->standaloneGUI)
	{
		renderPassBeginInfo.renderPass = offscreenPass->renderPass;
		renderPassBeginInfo.framebuffer = offscreenPass->frameBuffer[currentFrame];
	}
	else
	{
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = frameBuffers[currentFrame];
	}
	
    renderPassBeginInfo.renderArea.offset = { 0, 0 };
    renderPassBeginInfo.renderArea.extent = {width,height};

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { 0.0f, 0.0f, 0.0f,1.0f };
    clearValues[1].depthStencil = { 1.0f, 0 };

    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();
    
    vkCmdBeginRenderPass(drawCmdBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    // offscreen pass
    BuildCommandBuffers(drawCmdBuffers[currentFrame]);

    // gui pass
    if(gui->standaloneRenderPass)
    {
        vkCmdEndRenderPass(drawCmdBuffers[currentFrame]);
        renderPassBeginInfo.renderPass = gui->renderPass;
    	renderPassBeginInfo.framebuffer = frameBuffers[currentFrame];
        vkCmdBeginRenderPass(drawCmdBuffers[currentFrame], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
	
	// update gui
	ImGui_ImplGlfw_NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::NewFrame();
	DrawDockingWindows();
	NewGUIFrame();
	ImGui::Render();
	gui->UpdateBuffer();
    gui->DrawFrame(drawCmdBuffers[currentFrame]);

	vkCmdEndRenderPass(drawCmdBuffers[currentFrame]);
    
    CheckVulkanResult(vkEndCommandBuffer(drawCmdBuffers[currentFrame]));
}

void VulkanApplicationBase::SubmitFrame()
{
    swapChain->QueuePresent(queue, currentImageIndex, semaphores[currentFrame].renderComplete);
    CheckVulkanResult(vkQueueWaitIdle(queue));
    
    currentFrame = (currentFrame + 1) % maxFrameInFlight;
}




