#pragma once
#include <vector>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

typedef struct _SwapChainBuffers
{
    VkImage image;
    VkImageView view;
} SwapChainBuffer;

class VulkanSwapChain
{
public:
    VkFormat colorFormat;
    VkColorSpaceKHR colorSpace;
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;	
    uint32_t imageCount;
    std::vector<VkImage> images;
    std::vector<SwapChainBuffer> buffers;
    std::vector<VkFramebuffer> frameBuffers;
    uint32_t queueNodeIndex = UINT32_MAX;
    VkExtent2D imageExtent;

    VulkanSwapChain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    ~VulkanSwapChain();
    
#if USE_FRONTEND_GLFW
    void Init(GLFWwindow* window, VkSurfaceKHR surface);
#endif
    // void Connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    void Create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
    void CreateFrameBuffers();
    VkResult AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
    VkResult QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

private:
    
    void Cleanup();

    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    GLFWwindow* window;
    VkSurfaceKHR surface;
};
