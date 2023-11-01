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

VulkanApplicationBase::VulkanApplicationBase(std::string applicationName,uint32_t width, uint32_t height, bool validation)
{
    this->title = applicationName;
    this->name = applicationName;
    this->width = width;
    this->height = height;
    this->settings.validation = validation;
}

VulkanApplicationBase::~VulkanApplicationBase()
{
    swapChain.reset();

    vkDestroyImageView(device, depthStencil.view, nullptr);
    vkDestroyImage(device, depthStencil.image, nullptr);
    vkFreeMemory(device, depthStencil.mem, nullptr);
    
    vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
    vkDestroyCommandPool(device,cmdPool,nullptr);

    // barrier
    for(auto& semaphore : semaphores)
    {
        vkDestroySemaphore(device, semaphore.presentComplete, nullptr);
        vkDestroySemaphore(device, semaphore.renderComplete, nullptr);
    }
    for (auto& fence : waitFences) {
        vkDestroyFence(device, fence, nullptr);
    }
    
    vulkanDevice.reset();
    if (settings.validation)
        vks::debug::freeDebugCallback(instance);
    vkDestroyInstance(instance,nullptr);
    DestroyWindows();
}

bool VulkanApplicationBase::InitVulkan()
{
    SetupWindows();
    
    CheckVulkanResult(CreateInstance(settings.validation));

    // If requested, we enable the default validation layers for debugging
    if (settings.validation)
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
    if (requiresStencil) {
        validFormat = vks::utils::getSupportedDepthStencilFormat(physicalDevice, &depthFormat);
    } else {
        validFormat = vks::utils::getSupportedDepthFormat(physicalDevice, &depthFormat);
    }
    assert(validFormat);

    swapChain = std::make_unique<VulkanSwapChain>(instance,physicalDevice,device);
    
    // // Set up submit info structure
    // // Semaphores will stay the same during application lifetime
    // // Command buffer submission info is set by each example
    // submitInfo = vks::initializers::SubmitInfo();
    // submitInfo.pWaitDstStageMask = &submitPipelineStages;
    // submitInfo.waitSemaphoreCount = 1;
    // submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    // submitInfo.signalSemaphoreCount = 1;
    // submitInfo.pSignalSemaphores = &semaphores.renderComplete;
    
    return true;
}

bool VulkanApplicationBase::SetupWindows()
{
    // glfw
#ifdef USE_FRONTEND_GLFW
    glfwSetErrorCallback(vks::frontend::GLFW::GlfwErrorCallback);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if(!glfwVulkanSupported())
    {
        vks::helper::ExitFatal("GLFW: Vulkan Not Supported\n",-1);
        return true;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, vks::frontend::GLFW::FrameBufferResizeCallback);
    glfwSetMouseButtonCallback(window, vks::frontend::GLFW::MouseButtonCallback);
    glfwSetKeyCallback(window, vks::frontend::GLFW::KeyBoardCallback);
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
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
}

VkResult VulkanApplicationBase::CreateInstance(bool enableValidation)
{
    this->settings.validation = enableValidation;

#if _DEBUG
    this->settings.validation = true;
#endif

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
    if (settings.validation || std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end()) {
        instanceExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (instanceExtensions.size() > 0)
    {
        instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (settings.validation)
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
    SetupDepthStencil();
}

void VulkanApplicationBase::InitSwapchain()
{
    swapChain->InitSurface(surface); 
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
    swapChain->Create(&width,&height,settings.vsync,settings.fullscreen);
}

void VulkanApplicationBase::CreateCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    drawCmdBuffers.resize(swapChain->imageCount);

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
    semaphores.resize(swapChain->imageCount);
    waitFences.resize(swapChain->imageCount);
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

void VulkanApplicationBase::SetupDepthStencil()
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

void VulkanApplicationBase::SetupFrameBuffer()
{
    
}

void VulkanApplicationBase::SetupRenderPass()
{
    
}

void VulkanApplicationBase::GetEnabledExtensions()
{
}

void VulkanApplicationBase::GetEnabledFeatures()
{
    
}



