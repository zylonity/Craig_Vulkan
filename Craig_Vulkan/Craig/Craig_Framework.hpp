#pragma once

//idk why but it doesn't work unless defined right here.
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "Craig_Constants.hpp"
#include <chrono>
namespace Craig {
	
	//Forward declarations
	class Window;
	class Renderer2;
	class ImguiEditor;
	class SceneManager;


	class Framework {

	public:
		CraigError init();
		CraigError update();
		CraigError terminate();
	private:

		Craig::Window* mp_Window			= nullptr;
		Craig::Renderer2* mp_Renderer		= nullptr;
		Craig::SceneManager* mp_SceneManager = nullptr;

#if defined(IMGUI_ENABLED)
		Craig::ImguiEditor* mp_ImguiEditor = nullptr;
#endif
		
		float getElapsedTime();
		std::chrono::steady_clock::time_point m_LastFrameTime;
	};



}