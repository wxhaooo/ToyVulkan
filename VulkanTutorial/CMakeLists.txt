add_compile_definitions(GLFW_INCLUDE_VULKAN)
add_compile_definitions(GLM_FORCE_RADIANS)
add_compile_definitions(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)

find_package(libigl CONFIG REQUIRED)

file(GLOB VulkanTutorialSource "./src/*.cpp")
file(GLOB VulkanTutorialHeader ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_executable(VulkanTutorial ${VulkanTutorialSource} ${VulkanTutorialHeader})

target_include_directories(VulkanTutorial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(VulkanTutorial PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(VulkanTutorial PUBLIC ${GLFW3_INCLUDE_DIRS})
target_include_directories(VulkanTutorial PUBLIC ${EIGEN3_INCLUDE_DIR})
target_include_directories(VulkanTutorial PUBLIC ${IMGUI_INCLUDE_DIR})

target_link_libraries(VulkanTutorial glfw)
target_link_libraries(VulkanTutorial ${Vulkan_LIBRARIES} ImGUILib)
target_link_libraries(VulkanTutorial igl::igl_core)
# target_link_libraries(VulkanTutorial igl::igl_core igl_copyleft::igl_copyleft_core)

set_property(TARGET VulkanTutorial PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
