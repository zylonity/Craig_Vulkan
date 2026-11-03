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

#HiFi-LoFi, MIT - https://github.com/HiFi-LoFi/FFTConvolver (f2cdeb04c42141d2caec19ca4f137398b2a76b85)
#Partitioned convolution, so the reflected impulse response can be applied without doing it sample by sample
add_library(FFTConvolver STATIC
        Craig_Vulkan/External/FFTConvolver/AudioFFT.cpp
        Craig_Vulkan/External/FFTConvolver/FFTConvolver.cpp
        Craig_Vulkan/External/FFTConvolver/TwoStageFFTConvolver.cpp
        Craig_Vulkan/External/FFTConvolver/Utilities.cpp
)
#Keep the DSP optimised in Debug too, the audio callback still has a real-time deadline
if(NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(FFTConvolver PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()

#Valve Corporation, Apache-2.0 - Steam Audio v4.8.1 (0da18255cca520771f363ee01f100572b39a308e)
#Only the bit that turns an energy field into an impulse response, not the simulator itself
set(STEAM_DSP_DIR ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/SteamAudio)
add_library(SteamAudioReconstruction STATIC
        ${STEAM_DSP_DIR}/reconstructor.cpp
        ${STEAM_DSP_DIR}/energy_field.cpp
        ${STEAM_DSP_DIR}/impulse_response.cpp
        ${STEAM_DSP_DIR}/iir.cpp
        ${STEAM_DSP_DIR}/array_math.cpp
        ${STEAM_DSP_DIR}/memory_allocator.cpp
        ${STEAM_DSP_DIR}/standalone_memory.cpp
        ${STEAM_DSP_DIR}/math_functions.cpp
        ${STEAM_DSP_DIR}/bands.cpp
        ${STEAM_DSP_DIR}/air_absorption.cpp
)
target_precompile_headers(SteamAudioReconstruction PRIVATE <algorithm> <cmath> <cstring>)
if(NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(SteamAudioReconstruction PRIVATE $<$<CONFIG:Debug>:-O2>)
endif()

#get all my files to build
file(GLOB_RECURSE CRAIG_SOURCES
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/Renderer/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/*.hpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Craig/Renderer/*.hpp
)

#the game itself, kept out of Craig/ so the engine never depends on it
file(GLOB_RECURSE GAME_SOURCES
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Game/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/Game/*.hpp
)

file(GLOB_RECURSE IMGUI_SOURCES
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui/*.cpp
        ${CMAKE_SOURCE_DIR}/Craig_Vulkan/External/Imgui/*.h
)

add_executable(Craig_Vulkan
        ${CRAIG_SOURCES}
        ${GAME_SOURCES}
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

#anything after INPUT gets passed straight to glslc (ARGN), e.g. --target-env for ray queries
function(compile_glsl OUTPUT INPUT)
    message(STATUS "Shader: ${INPUT} -> ${OUTPUT}")
    add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${GLSLC} ${ARGN}
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
compile_glsl(${SHADER_DIR}/skyVert.spv ${SHADER_DIR}/SkyVertexShader.vert)
compile_glsl(${SHADER_DIR}/skyFrag.spv ${SHADER_DIR}/SkyFragmentShader.frag)
#the ray query shader targets vulkan1.2, the rest stay on the default
compile_glsl(${SHADER_DIR}/rayQueryTest.spv ${SHADER_DIR}/RayQueryTest.comp --target-env=vulkan1.2)
compile_glsl(${SHADER_DIR}/SoundPropagation.spv ${SHADER_DIR}/SoundPropagation.comp --target-env=vulkan1.2)
#the shader #includes this, so it has to rebuild when it changes too
add_custom_command(OUTPUT ${SHADER_DIR}/SoundPropagation.spv APPEND
        DEPENDS ${SHADER_DIR}/SteamAudioPropagation.glsl)

add_custom_target(Shaders ALL
        DEPENDS
        ${SHADER_DIR}/vert.spv
        ${SHADER_DIR}/frag.spv
        ${SHADER_DIR}/skyVert.spv
        ${SHADER_DIR}/skyFrag.spv
        ${SHADER_DIR}/rayQueryTest.spv
        ${SHADER_DIR}/SoundPropagation.spv
)

# Make the main program depend on shaders
add_dependencies(Craig_Vulkan Shaders)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)