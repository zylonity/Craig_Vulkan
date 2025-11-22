#pragma once

#include <vector>
#include <SDL2/SDL.h>

#include "Craig_Constants.hpp"


namespace Craig {

	//Forward declaration
	class Camera;

	class Window {

	public:

		struct WindowExtent {
			uint32_t width;
			uint32_t height;
		};

		CraigError init();
		CraigError update(const float& deltaTime);
		CraigError terminate();

		// Getters
		std::vector<const char*>& getExtensionsVector() { return mv_SDL_Extensions; }
		SDL_Window* getSDLWindow() const { return mp_SDL_Window; }
		WindowExtent getDrawableExtent() const;
		const bool isResizeNeeded() const { return m_resizeNeeded; }
		void finishedResize() { m_resizeNeeded = false; }

		//Setters
		void setCameraRef(Camera* camera) { m_currentCamera = camera; }
	private:
		SDL_Window* mp_SDL_Window = nullptr; // SDL Window handle
		unsigned m_SDL_ExtensionCount; // Number of elements in the extension array (Number of extensions in use?)
		std::vector<const char*> mv_SDL_Extensions; // Array of extensions required by SDL for Vulkan
		
		Camera* m_currentCamera;

		bool m_resizeNeeded = false;
		bool m_mouseLocked = false;


	};



}