#pragma once
#include <GLFW/glfw3.h>

namespace vks
{
    namespace frontend
    {
        namespace GLFW
        {
            void GlfwErrorCallback(int error, const char* description);

            void FrameBufferResizeCallback(GLFWwindow* window, int width, int height);

            void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

            void CursorPosCallback(GLFWwindow* window, double xPos, double yPos);
            
            void KeyBoardCallback(GLFWwindow* window, int key, int scanCode, int action, int mods);
        }
    }    
}
