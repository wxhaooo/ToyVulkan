add_compile_definitions(GLFW_INCLUDE_VULKAN)

set(IMGUI_INCLUDE_DIR "./")
file(GLOB IMGUI_SOURCE "./*.cpp" "./backends/imgui_impl_vulkan.cpp" "./backends/imgui_impl_glfw.cpp")
file(GLOB IMGUI_HEADER "./*.h")

add_library(ImGUILib STATIC ${IMGUI_SOURCE})

target_include_directories(ImGUILib PUBLIC ${IMGUI_INCLUDE_DIR})
target_include_directories(ImGUILib PUBLIC ${GLFW3_INCLUDE_DIRS})
target_include_directories(ImGUILib PUBLIC ${Vulkan_INCLUDE_DIRS})

target_link_libraries(ImGUILib glfw)
target_link_libraries(ImGUILib ${Vulkan_LIBRARIES})