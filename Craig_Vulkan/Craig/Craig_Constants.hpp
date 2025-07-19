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



enum CraigError {
	CRAIG_SUCCESS = 0,
	CRAIG_FAIL = 1,
	CRAIG_CLOSED = 2
};

