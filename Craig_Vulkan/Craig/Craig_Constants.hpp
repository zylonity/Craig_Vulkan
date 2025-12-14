#pragma once

#include <stdint.h>

//App Data
constexpr char kSDL_WindowName[] = "Craig's Vulkan Engine";
constexpr int kSDL_WindowWidth = 1280;
constexpr int kSDL_WindowHeight = 720;

constexpr char kVK_AppName[] = "Craig's Vulkan Engine";
constexpr uint32_t kVK_AppVersion = 1;
constexpr char kVK_EngineName[] = "Craig";
constexpr uint32_t kVK_EngineVersion = 2;

constexpr float kClearColour[4] = { 1.0f, 0.5f, 0.0f, 1.0f }; // Clear colour for the render target
constexpr int kMaxFramesInFlight = 2; //How many frames the GPU should deal with at a time


enum CraigError {
	CRAIG_SUCCESS = 0,
	CRAIG_FAIL = 1,
	CRAIG_CLOSED = 2
};

