cmake_minimum_required(VERSION 3.20)

set(VCPKG_DIR "D:/Utility/")

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(ToyVulkan)

set(CMAKE_CXX_STANDARD 17)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Eigen3 REQUIRED)

add_subdirectory(3rd/imgui)
add_subdirectory(core)
add_subdirectory(VulkanTutorial)
add_subdirectory(LoadGLTF)