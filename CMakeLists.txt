cmake_minimum_required(VERSION 4.0)
project(Craig_Vulkan)

#add the libraries i'm using
#on mac, installed with homebrew
#on linux, installed with the package manager (dnf in my case)
find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)

set(CMAKE_CXX_STANDARD 20)

#jolt physics, pulled from github and built with the project
#its CMakeLists lives in Build/ instead of the repo root, hence SOURCE_SUBDIR
include(FetchContent)

#turn off the bits of jolt i'm not using
set(JPH_USE_VK OFF)                          #gpu compute backends, need dxc to build and i don't need them
set(JPH_USE_DX12 OFF)
set(JPH_USE_MTL OFF)
set(JPH_USE_CPU_COMPUTE OFF)
set(ENABLE_OBJECT_STREAM OFF)                #jolt's own serialization, scenes are json already
set(ENABLE_INSTALL OFF)
set(ENABLE_ALL_WARNINGS OFF)                 #otherwise it builds with warnings as errors
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)     #windows: match the runtime the main exe uses
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(USE_ASSERTS ON)
endif()

FetchContent_Declare(
        JoltPhysics
        GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG v5.6.0
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR Build
)
FetchContent_MakeAvailable(JoltPhysics)

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
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui
)

# SYSTEM so third party headers (VMA) don't flood the build with warnings
target_include_directories(Craig_Vulkan SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External
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
        Jolt
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