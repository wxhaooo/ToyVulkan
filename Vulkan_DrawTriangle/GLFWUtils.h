#pragma once
#include <cstdio>

#include "DrawTraiangleVkApplication.h"


namespace GLFW
{
    static void GlfwErrorCallback(int error, const char* description)
    {
        fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    }

    static void FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
	{
        auto app = reinterpret_cast<DrawTriangleVkApplication*>(glfwGetWindowUserPointer(window));
        app->ReCreateVulkanResource();
    }
};
