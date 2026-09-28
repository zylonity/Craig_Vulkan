cmake_minimum_required(VERSION 4.0)
project(Craig_Vulkan)

#add the libraries i'm using
#on mac, installed with homebrew
#on linux, installed with the package manager (dnf in my case)
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 20)

#get all my files to build
file(GLOB_RECURSE CRAIG_SOURCES
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/Renderer/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/*.hpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/Renderer/*.hpp
)

file(GLOB_RECURSE IMGUI_SOURCES
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui/*.h
)

add_executable(Craig_Vulkan
        ${CRAIG_SOURCES}
        ${IMGUI_SOURCES}
        Craig_Vulkan/main.cpp
)

target_include_directories(Craig_Vulkan PRIVATE
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui
)

#Imgui flag for CLION, id rather set this in the IDE
option(ENABLE_IMGUI "Enable ImGui" OFF)
if(ENABLE_IMGUI)
    target_compile_definitions(Craig_Vulkan PUBLIC IMGUI_ENABLED)
endif()

target_compile_definitions(Craig_Vulkan PUBLIC
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
)

# Link against Vulkan
target_link_libraries(Craig_Vulkan PUBLIC
        Vulkan::Vulkan
        SDL3::SDL3
        glm::glm
)


# GLSL shaders get compiled to SPIR-V with glslc (ships with the Vulkan SDK) before building
# glslc picks the shader stage from the file extension (.vert / .frag)
find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/bin $ENV{VULKAN_SDK}/Bin REQUIRED)

function(compile_glsl OUTPUT INPUT)
    message(STATUS "Shader: ${INPUT} -> ${OUTPUT}")
    add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${GLSLC}
            -o ${OUTPUT}
            ${INPUT}
            DEPENDS ${INPUT}
            COMMENT "Compiling ${INPUT}"
            VERBATIM
    )
endfunction()

set(SHADER_DIR ${CMAKE_SOURCE_DIR}/Craig_Vulkan/data/shaders)

compile_glsl(${SHADER_DIR}/vert.spv ${SHADER_DIR}/VertexShader.vert)
compile_glsl(${SHADER_DIR}/frag.spv ${SHADER_DIR}/FragmentShader.frag)

add_custom_target(Shaders ALL
        DEPENDS
        ${SHADER_DIR}/vert.spv
        ${SHADER_DIR}/frag.spv
)

# Make the main program depend on shaders
add_dependencies(Craig_Vulkan Shaders)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)