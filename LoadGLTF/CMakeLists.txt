file(GLOB LoadGLTFSource "./src/*.cpp")
file(GLOB LoadGLTFHeader ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)

add_executable(LoadGLTF ${LoadGLTFSource} ${LoadGLTFHeader})

target_include_directories(LoadGLTF PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(LoadGLTF PUBLIC ${Vulkan_INCLUDE_DIRS})
target_include_directories(LoadGLTF PUBLIC ${GLFW3_INCLUDE_DIRS})
target_include_directories(LoadGLTF PUBLIC ${EIGEN3_INCLUDE_DIR})
target_include_directories(LoadGLTF PUBLIC ${IMGUI_INCLUDE_DIR})
target_include_directories(LoadGLTF PUBLIC ${CORE_INCLUDE_DIR})

target_link_libraries(LoadGLTF glfw)
target_link_libraries(LoadGLTF ${Vulkan_LIBRARIES} ImGUILib)
target_link_libraries(LoadGLTF CoreLib)

set_property(TARGET LoadGLTF PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")