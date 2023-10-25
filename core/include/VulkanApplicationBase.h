#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

class VulkanApplicationBase
{
public:
    VulkanApplicationBase() = delete;
    VulkanApplicationBase(std::string applicationName):title(applicationName),name(applicationName){}

    virtual ~VulkanApplicationBase() = default;

    struct Settings
    {
        bool validation = false;
        bool fullscreen = false;
        bool vsync = false;
        bool overlay = true;
    } settings;

    std::string title = "Vulkan Example";
    std::string name = "vulkanExample";

    uint32_t apiVersion = VK_API_VERSION_1_0;

    void InitVulkan();
    virtual VkResult CreateInstance(bool enableValidation);

protected:
    // Vulkan instance, stores all per-application states
    VkInstance instance = nullptr;
    std::vector<std::string> supportedInstanceExtensions;
    // for subclass use
    std::vector<const char*> enabledInstanceExtensions;
};


