file(GLOB DeferredPBRSource "./src/*.cpp")
file(GLOB DeferredPBRHeader ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_executable(DeferredPBR ${DeferredPBRSource} ${DeferredPBRHeader})

target_include_directories(DeferredPBR PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(DeferredPBR PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(DeferredPBR PUBLIC ${GLFW3_INCLUDE_DIRS})
target_include_directories(DeferredPBR PUBLIC ${EIGEN3_INCLUDE_DIR})
target_include_directories(DeferredPBR PUBLIC ${IMGUI_INCLUDE_DIR})
target_include_directories(DeferredPBR PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(DeferredPBR glfw)
target_link_libraries(DeferredPBR ${Vulkan_LIBRARIES} ImGUILib)
target_link_libraries(DeferredPBR CoreLib)

set_property(TARGET DeferredPBR PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")