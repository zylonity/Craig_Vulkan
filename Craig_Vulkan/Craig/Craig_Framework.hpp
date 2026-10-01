#pragma once

//idk why but it doesn't work unless defined right here.
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "Craig_Constants.hpp"
#include "Craig_EngineModes.hpp"
#include "Craig_Game.hpp"
#include <chrono>
namespace Craig {
	
	//Forward declarations
	class Window;
	class Renderer;
	class ImguiEditor;
	class SceneManager;
	class PhysicsEngine;


	class Framework {

	public:
		// call before init()
		// optional, without one it's just an editor
		void setGame(Craig::Game* pGame) { mp_Game = pGame; }

		CraigError init();
		CraigError update();
		CraigError terminate();
	private:

		Craig::Window* mp_Window			 = nullptr;
		Craig::Renderer* mp_Renderer		 = nullptr;
		Craig::SceneManager* mp_SceneManager = nullptr;
		Craig::PhysicsEngine* mp_PhysicsEngine = nullptr;
		Craig::Game* mp_Game = nullptr; // not owned, main.cpp is

		// edit/play/pause etc, decides what ticks each frame
		Craig::EngineModeMachine m_engineModes{ "Engine" };
		Craig::GameServices m_gameServices;

#if defined(IMGUI_ENABLED)
		Craig::ImguiEditor* mp_ImguiEditor = nullptr;
#endif
		
		float getElapsedTime();
		std::chrono::steady_clock::time_point m_LastFrameTime;
	};



}