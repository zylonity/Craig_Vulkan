#pragma once

#include <stdint.h>

//App Data
constexpr char kSDL_WindowName[] = "Craig's Vulkan Engine";
constexpr int kSDL_WindowWidth = 1280;
constexpr int kSDL_WindowHeight = 720;

constexpr char kVK_AppName[] = "Craig's Vulkan Engine";
constexpr uint32_t kVK_AppVersion = 1;
constexpr char kVK_EngineName[] = "Craig";
constexpr uint32_t kVK_EngineVersion = 1;

constexpr float kClearColour[4] = { 1.0f, 0.5f, 0.0f, 1.0f }; // Clear colour for the render target
constexpr int kMaxFramesInFlight = 2; //How many frames the GPU should deal with at a time

#define CRAIG_ENABLE_PROFILER 0 // 1 prints how long each part of the frame takes every second (Craig_Profiler.hpp)

constexpr uint32_t kMaxLODForDebugging = 16;
constexpr uint32_t kMaxNumObjects = 4096;

constexpr uint32_t kVertexAttributeDescriptors = 4;

//Scenes
constexpr char kScenesDirectory[] = "data/scenes";
constexpr char kDefaultScenePath[] = "data/scenes/TestScene1.json"; // Scene loaded on startup

//Physics
// Jolt's scratch memory per step, going over it aborts ("TempAllocator: Out of memory")
constexpr uint32_t kPhysicsTempAllocatorSize = 10 * 1024 * 1024; // 10 MB

enum CraigError {
	CRAIG_SUCCESS = 0,
	CRAIG_FAIL = 1,
	CRAIG_CLOSED = 2,
	CRAIG_FILE_NOT_FOUND = 3,
	CRAIG_NO_NAME = 4,
	CRAIG_DUPLICATE_NAME = 5,
};
