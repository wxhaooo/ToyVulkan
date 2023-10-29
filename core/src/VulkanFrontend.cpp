#include <cstdio>
#include <GLFW/glfw3.h>
#include <VulkanFrontend.h>
#include <VulkanApplicationBase.h>

namespace vks
{
    namespace frontend
    {
        namespace GLFW
        {
            void GlfwErrorCallback(int error, const char* description)
            {
                fprintf(stderr, "GLFW Error %d: %s\n", error, description);
            }

            void FrameBufferResizeCallback(GLFWwindow* window, int width, int height)
            {
                auto app = reinterpret_cast<VulkanApplicationBase*>(glfwGetWindowUserPointer(window));
                // app->ReCreateVulkanResource();
            }

            void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
            {
	    
            }

            void KeyBoardCallback(GLFWwindow* window, int key, int scanCode, int action, int mods)
            {
                // auto app = reinterpret_cast<VulkanTutorial*>(glfwGetWindowUserPointer(window));
                // if (app == nullptr) return;
                //
                // // Camera* camera = Singleton<Camera>::Instance();
                // if (key == GLFW_KEY_W)
                //     camera->keys.up = (action == GLFW_PRESS);
                // else if (key == GLFW_KEY_S)
                //     camera->keys.down = (action == GLFW_PRESS);
                // else if (key == GLFW_KEY_A)
                //     camera->keys.left = (action == GLFW_PRESS);
                // else if (key == GLFW_KEY_D)
                //     camera->keys.right = (action == GLFW_PRESS);
            }        
        }
    }    
}
