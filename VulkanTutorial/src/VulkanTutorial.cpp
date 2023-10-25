#include <iostream>

#include <GLFW/glfw3.h>
#include "GLFWUtils.h"

#include "VulkanTutorial.h"
#include "Mesh.h"


int main(int argc, char* argv[])
{
    std::unique_ptr<VulkanTutorial> drawTriangleVkApp =
        std::make_unique<VulkanTutorial>();

    glfwSetErrorCallback(GLFW::GlfwErrorCallback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan with GLFW", nullptr, nullptr);
    if(!glfwVulkanSupported())
    {
        std::cerr << "GLFW: Vulkan Not Supported\n";
        return -1;
    }

    glfwSetWindowUserPointer(window, drawTriangleVkApp.get());
    glfwSetFramebufferSizeCallback(window, GLFW::FrameBufferResizeCallback);
    glfwSetMouseButtonCallback(window, GLFW::MouseButtonCallback);
    glfwSetKeyCallback(window, GLFW::KeyBoardCallback);

    // get glfw extensions
    std::vector<const char*> extensions;
    uint32_t extensionsCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionsCount);
    for (uint32_t i = 0; i < extensionsCount; i++)
        extensions.push_back(glfwExtensions[i]);

#if _DEBUG
    std::copy(VkUtils::DefaultDebugExtensions.begin(),
        VkUtils::DefaultDebugExtensions.end(), std::back_inserter(extensions));
#endif

    drawTriangleVkApp->InitVulkan(extensions, window);

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        drawTriangleVkApp->Run();
    }

    drawTriangleVkApp->CleanUp();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
