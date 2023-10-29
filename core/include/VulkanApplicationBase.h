#pragma once
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <VulkanDevice.h>
#include <memory>
#include <GLFW/glfw3.h>

class VulkanApplicationBase
{
public:
    VulkanApplicationBase() = delete;
    VulkanApplicationBase(std::string applicationName,bool validation);

    virtual ~VulkanApplicationBase();

    /** @brief Encapsulated physical and logical vulkan device */
    std::unique_ptr<vks::VulkanDevice> vulkanDevice;

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

    bool InitVulkan();

#ifdef USE_FRONTEND_GLFW
    GLFWwindow* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
#endif

protected:
    // add different front end here,glfw,SDL.etc
    bool InitWindows();
    // create surface from surface
    void CreateWindowsSurface();
    void DestroyWindows();
    virtual VkResult CreateInstance(bool enableValidation);
    /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
    virtual void getEnabledFeatures();
    /** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
    virtual void getEnabledExtensions();

    // Vulkan instance, stores all per-application states
    VkInstance instance;
    std::vector<std::string> supportedInstanceExtensions;
    // Physical device (GPU) that Vulkan will use
    VkPhysicalDevice physicalDevice;
    // Stores physical device properties (for e.g. checking device limits)
    VkPhysicalDeviceProperties deviceProperties;
    // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
    VkPhysicalDeviceFeatures deviceFeatures;
    // Stores all available memory (type) properties for the physical device
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    /** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
    VkPhysicalDeviceFeatures enabledFeatures{};
    /** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
    std::vector<const char*> enabledDeviceExtensions;
    std::vector<const char*> enabledInstanceExtensions;
    /** @brief Optional pNext structure for passing extension structures to device creation */
    void* deviceCreatepNextChain = nullptr;
    /** @brief Logical device, application's view of the physical device (GPU) */
    VkDevice device = VK_NULL_HANDLE;
    // Handle to the device graphics queue that command buffers are submitted to
    VkQueue queue = VK_NULL_HANDLE;
};


