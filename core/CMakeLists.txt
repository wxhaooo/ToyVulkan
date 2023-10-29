add_compile_definitions(GLFW_INCLUDE_VULKAN)
add_compile_definitions(USE_FRONTEND_GLFW)

file(GLOB CORE_SOURCE "./src/*.cpp")
file(GLOB CORE_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

set(CORE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(CoreLib STATIC ${CORE_SOURCE} ${CORE_HEADER})

target_include_directories(CoreLib PUBLIC ${IMGUI_INCLUDE_DIR})
target_include_directories(CoreLib PUBLIC ${GLFW3_INCLUDE_DIRS})
target_include_directories(CoreLib PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(CoreLib PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(CoreLib glfw)
target_link_libraries(CoreLib ${Vulkan_LIBRARIES})

