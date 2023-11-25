#include <cstdio>
#include <imgui_internal.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include <VulkanFrontend.h>
#include <VulkanApplicationBase.h>
#include <Singleton.hpp>
#include <InputManager.h>
#include <Camera.h>

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
                app->ReCreateVulkanResource();
            }

            void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
            {
                // don't pass mouse and keyboard presses further if an ImGui widget is active
                // auto& io = ImGui::GetIO();
                // if (io.WantCaptureMouse) return;

                ImGuiContext* currentContext  = ImGui::GetCurrentContext();
                ImGuiWindow* hoveredWindow =  currentContext->HoveredWindow;
                if(hoveredWindow == nullptr || !hoveredWindow->ForwardBackend) return;
                auto app = reinterpret_cast<VulkanApplicationBase*>(glfwGetWindowUserPointer(window));
                // if(button == GLFW_MOUSE_BUTTON_LAST && action == GLFW_PRESS)
                // {
                //     Camera* camera = Singleton<Camera>::Instance();
                //     // VulkanGUI* gui = Singleton<VulkanGUI>::Instance();
                // }
            }

            void CursorPosCallback(GLFWwindow* window, double xPos, double yPos)
            {
                ImGuiContext* currentContext  = ImGui::GetCurrentContext();
                ImGuiWindow* hoveredWindow =  currentContext->HoveredWindow;
                if(hoveredWindow == nullptr || !hoveredWindow->ForwardBackend) return;

                VulkanApplicationBase* app = reinterpret_cast<VulkanApplicationBase*>(glfwGetWindowUserPointer(window));
                int leftMouseButtonState = glfwGetMouseButton(window,GLFW_MOUSE_BUTTON_LEFT);
                InputManager* inputManager = Singleton<InputManager>::Instance();
                if(leftMouseButtonState == GLFW_PRESS)
                {
                    Camera* camera = Singleton<Camera>::Instance();
                    
                    glm::vec2 cachedMousePos = inputManager->mousePos;
                    // float dx = cachedMousePos.x - static_cast<float>(xPos);
                    // float dy = cachedMousePos.y - static_cast<float>(yPos);
                    int32_t dx = static_cast<int32_t>(cachedMousePos.x - xPos);
                    int32_t dy = static_cast<int32_t>(cachedMousePos.y - yPos);

                    camera->Rotate(glm::vec3(dy * camera->rotationSpeed, -dx * camera->rotationSpeed, 0.0f));
                    app->viewUpdated = true;
                }
                inputManager->mousePos = glm::vec2(xPos,yPos);
            }

            void KeyBoardCallback(GLFWwindow* window, int key, int scanCode, int action, int mods)
            {
                // don't pass mouse and keyboard presses further if an ImGui widget is active
                auto& io = ImGui::GetIO();
                if (io.WantCaptureKeyboard) return;
                
                auto app = reinterpret_cast<VulkanApplicationBase*>(glfwGetWindowUserPointer(window));
                
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
