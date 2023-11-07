#pragma once
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <vulkan/vulkan_core.h>
#include <VulkanDevice.h>
#include <GLFW/glfw3.h>
#include <VulkanSwapChain.h>

class VulkanApplicationBase
{
public:
    VulkanApplicationBase() = delete;
    VulkanApplicationBase(std::string applicationName, uint32_t width, uint32_t height, bool validation);

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

    struct {
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
    } depthStencil;

    uint32_t width;
    uint32_t height;
#ifdef USE_FRONTEND_GLFW
    GLFWwindow* window = nullptr;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
#endif

    bool prepared = false;
    bool viewUpdated = false;
    /** @brief Last frame time measured using a high performance timer (if available) */
    float frameTimer = 1.0f;
    // Defines a frame rate independent timer value clamped from -1.0...1.0
    // For use in animations, rotations, etc.
    float timer = 0.0f;
    // Multiplier for speeding up (or slowing down) the global timer
    float timerSpeed = 0.25f;
    bool paused = false;

    bool InitVulkan();
    /** @brief Prepares all Vulkan resources and functions required to run the sample */
    virtual void Prepare();
    /** @brief Entry point for the main render loop */
    void RenderLoop();

protected:
    // Frame counter to display fps
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;
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
    // Depth buffer format (selected during Vulkan initialization)
    VkFormat depthFormat;
    // Command buffer pool
    VkCommandPool cmdPool;
    /** @brief Pipeline stages used to wait at for graphics queue submissions */
    VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // Contains command buffers and semaphores to be presented to the queue
    VkSubmitInfo submitInfo;
    // Command buffers used for rendering
    std::vector<VkCommandBuffer> drawCmdBuffers;
    // Global render pass for frame buffer writes
    VkRenderPass renderPass = VK_NULL_HANDLE;
    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer>frameBuffers;
    // Active frame buffer index
    uint32_t currentBuffer = 0;
    // Descriptor set pool
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    // List of shader modules created (stored for cleanup)
    std::vector<VkShaderModule> shaderModules;
    // Pipeline cache object
    VkPipelineCache pipelineCache;
    // Wraps the swap chain to present images (framebuffers) to the windowing system
    std::unique_ptr<VulkanSwapChain> swapChain;
    // Synchronization semaphores
    struct Semaphores{
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    };
    std::vector<Semaphores> semaphores;
    std::vector<VkFence> waitFences;
    bool requiresStencil{ false };

    // add different front end here,glfw,SDL.etc
    bool SetupWindows();
    // create surface from surface
    void CreateWindowsSurface();
    void DestroyWindows();
    virtual VkResult CreateInstance(bool enableValidation);
    /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
    virtual void GetEnabledFeatures();
    /** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
    virtual void GetEnabledExtensions();
    /** @brief (Virtual) Setup default depth and stencil views */
    virtual void SetupDepthStencil();
    /** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
    virtual void SetupFrameBuffer();
    /** @brief (Virtual) Setup a default renderpass */
    virtual void SetupRenderPass();
    virtual void Render() = 0;


private:
    void InitSwapchain();
    void CreateCommandPool();
    void SetupSwapChain();
    void CreateCommandBuffers();
    void CreateSynchronizationPrimitives();
    void CreatePipelineCache();
    void NextFrame();
};


