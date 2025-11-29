cmake_minimum_required(VERSION 4.0)
project(Craig_Vulkan)

#set(CMAKE_BUILD_TYPE Release)
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math -DNDEBUG")

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)
#find_package( REQUIRED)

set(CMAKE_CXX_STANDARD 20)

include_directories(Craig_Vulkan)
include_directories(Craig_Vulkan/Craig)
include_directories(Craig_Vulkan/External)
include_directories(Craig_Vulkan/External/Imgui)

add_executable(Craig_Vulkan
        Craig_Vulkan/Craig/Craig_Camera.cpp
        Craig_Vulkan/Craig/Craig_Camera.hpp
        Craig_Vulkan/Craig/Craig_Constants.hpp
        Craig_Vulkan/Craig/Craig_Editor.cpp
        Craig_Vulkan/Craig/Craig_Editor.hpp
        Craig_Vulkan/Craig/Craig_Framework.cpp
        Craig_Vulkan/Craig/Craig_Framework.hpp
        Craig_Vulkan/Craig/Craig_Renderer.cpp
        Craig_Vulkan/Craig/Craig_Renderer.hpp
        Craig_Vulkan/Craig/Craig_ResourceManager.cpp
        Craig_Vulkan/Craig/Craig_ResourceManager.hpp
        Craig_Vulkan/Craig/Craig_Scene.cpp
        Craig_Vulkan/Craig/Craig_Scene.hpp
        Craig_Vulkan/Craig/Craig_SceneManager.cpp
        Craig_Vulkan/Craig/Craig_SceneManager.hpp
        Craig_Vulkan/Craig/Craig_ShaderCompilation.cpp
        Craig_Vulkan/Craig/Craig_ShaderCompilation.hpp
        Craig_Vulkan/Craig/Craig_Window.cpp
        Craig_Vulkan/Craig/Craig_Window.hpp
        Craig_Vulkan/External/Imgui/imconfig.h
        Craig_Vulkan/External/Imgui/imgui.cpp
        Craig_Vulkan/External/Imgui/imgui.h
        Craig_Vulkan/External/Imgui/imgui_demo.cpp
        Craig_Vulkan/External/Imgui/imgui_draw.cpp
        Craig_Vulkan/External/Imgui/imgui_impl_sdl2.cpp
        Craig_Vulkan/External/Imgui/imgui_impl_sdl2.h
        Craig_Vulkan/External/Imgui/imgui_impl_vulkan.cpp
        Craig_Vulkan/External/Imgui/imgui_impl_vulkan.h
        Craig_Vulkan/External/Imgui/imgui_internal.h
        Craig_Vulkan/External/Imgui/imgui_tables.cpp
        Craig_Vulkan/External/Imgui/imgui_widgets.cpp
        Craig_Vulkan/External/Imgui/imstb_rectpack.h
        Craig_Vulkan/External/Imgui/imstb_textedit.h
        Craig_Vulkan/External/Imgui/imstb_truetype.h
        Craig_Vulkan/External/stb_image.h
        Craig_Vulkan/External/tiny_obj_loader.h
        Craig_Vulkan/External/vk_mem_alloc.h
        Craig_Vulkan/main.cpp)

target_compile_definitions(Craig_Vulkan PUBLIC IMGUI_ENABLED _DEBUG)

# Link against Vulkan
target_link_libraries(Craig_Vulkan PUBLIC
        Vulkan::Vulkan
        SDL2::SDL2
        glm::glm
)


function(compile_hlsl OUTPUT INPUT PROFILE)
    add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND dxc
            -spirv
            -T ${PROFILE}
            -E main
            -Fo ${OUTPUT}
            ${INPUT}
            DEPENDS ${INPUT}
            COMMENT "Compiling ${INPUT}"
    )
endfunction()

# Where your HLSL lives (YOUR PROJECT)
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/Craig_Vulkan/data/shaders)

# Compile vertex + fragment shaders
compile_hlsl(
        ${SHADER_SRC_DIR}/vert.spv
        ${SHADER_SRC_DIR}/VertexShader.vert
        vs_6_4
)

compile_hlsl(
        ${SHADER_SRC_DIR}/frag.spv
        ${SHADER_SRC_DIR}/FragmentShader.frag
        ps_6_4
)

# Create shader build target
add_custom_target(Shaders ALL
        DEPENDS
        ${SHADER_SRC_DIR}/vert.spv
        ${SHADER_SRC_DIR}/frag.spv
)

# Make the main program depend on shaders
add_dependencies(Craig_Vulkan Shaders)
