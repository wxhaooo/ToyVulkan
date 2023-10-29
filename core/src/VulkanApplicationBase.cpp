#include <iostream>
#include <vector>
#include <VulkanApplicationBase.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#include <VulkanHelper.h>
#include <VulkanDebug.h>
#include <VulkanDevice.h>

#include "VulkanFrontend.h"

VulkanApplicationBase::VulkanApplicationBase(std::string applicationName, bool validation)
{
    title = applicationName;
    name = applicationName;
    settings.validation = validation;
}

VulkanApplicationBase::~VulkanApplicationBase()
{
    if(surface)
        vkDestroySurfaceKHR(instance,surface,nullptr);
    
    vulkanDevice.reset();
    if (settings.validation)
        vks::debug::freeDebugCallback(instance);
    vkDestroyInstance(instance,nullptr);
    DestroyWindows();
}

bool VulkanApplicationBase::InitVulkan()
{
    // init front end (add different front end here,glfw,SDL.etc)
    InitWindows();
    
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
        vks::helper::exitFatal("No device with Vulkan support found", -1);
        return false;
    }
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    CheckVulkanResult(vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data()));

    // select default GPU in most of time
    int selectedDevice = 0;
    physicalDevice = physicalDevices[selectedDevice];

    if(physicalDevice == VK_NULL_HANDLE)
        vks::helper::exitFatal("failed to find a suitable GPU!",-1);

    // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

    // derived class can override this to set actual features (based on above readings) to enable for logical device creation
    getEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    vulkanDevice = std::make_unique<vks::VulkanDevice>(physicalDevice);
    
    // derived class can enable extensions based on the list of supported extensions read from the physical device
    getEnabledExtensions();
    
    CheckVulkanResult(vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain));
    device = vulkanDevice->logicalDevice;
    
    return true;
}

bool VulkanApplicationBase::InitWindows()
{
    // glfw
#ifdef USE_FRONTEND_GLFW
    glfwSetErrorCallback(vks::frontend::GLFW::GlfwErrorCallback);
    if (!glfwInit()) return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(1280, 720, title.c_str(), nullptr, nullptr);
    if(!glfwVulkanSupported())
    {
        vks::helper::exitFatal("GLFW: Vulkan Not Supported\n",-1);
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

    // get glfw extensions
    std::vector<const char*> instanceExtensions;
    uint32_t extensionsCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionsCount);
    for (uint32_t i = 0; i < extensionsCount; i++)
        instanceExtensions.emplace_back(glfwExtensions[i]);

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

void VulkanApplicationBase::getEnabledExtensions()
{
}

void VulkanApplicationBase::getEnabledFeatures()
{
    
}



