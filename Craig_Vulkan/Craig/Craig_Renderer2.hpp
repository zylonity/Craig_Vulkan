#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <vector>
#include <SDL2/SDL_vulkan.h>
#include "../External/vk_mem_alloc.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Craig_Constants.hpp"
#include "Craig_Camera.hpp"
#include "Craig_ResourceManager.hpp"

namespace Craig {

	//Forward declarations
	class Window;
	class SceneManager;

	class Renderer2 {

	public:
		CraigError init(Window* CurrentWindowPtr);
		CraigError update(const float& deltaTime);
		CraigError terminate();

	private:

		Window* mp_CurrentWindow = nullptr; // Pointer to the current window
		void initVulkan();
		void createInstance();
		

	};

}