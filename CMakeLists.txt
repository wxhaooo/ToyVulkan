cmake_minimum_required(VERSION 3.20)

set(VCPKG_DIR "E:/Code/")

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(ToyVulkan)

set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(GLFW_INCLUDE_VULKAN)
add_compile_definitions(GLM_FORCE_RADIANS)
add_compile_definitions(GLM_FORCE_DEFAULT_ALIGNED_GENTYPES)
add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE)
add_compile_definitions(USE_FRONTEND_GLFW)
add_compile_definitions(TINYGLTF_IMPLEMENTATION)
add_compile_definitions(STB_IMAGE_IMPLEMENTATION)
add_compile_definitions(TINYGLTF_NO_STB_IMAGE_WRITE)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Eigen3 REQUIRED)

include_directories(external/ktx/include)
include_directories(external/ktx/other_include)
include_directories(external/tinygltf)
include_directories(external/imgui)

add_subdirectory(external/imgui)
add_subdirectory(core)
add_subdirectory(VulkanTutorial)
add_subdirectory(LoadGLTF)
add_subdirectory(DeferredPBR)