#pragma once

#include <vector>
#include <SDL2/SDL.h>

#include "Craig_Constants.hpp"


namespace Craig {


	class Window {

	public:

		struct WindowExtent {
			uint32_t width;
			uint32_t height;
		};

		CraigError init();
		CraigError update();
		CraigError terminate();

		// Getters
		std::vector<const char*>& getExtensionsVector() { return mv_SDL_Extensions; }
		SDL_Window* getSDLWindow() const { return mp_SDL_Window; }
		WindowExtent getDrawableExtent() const;
		const bool isResizeNeeded() const { return m_resizeNeeded; }
		void finishedResize() { m_resizeNeeded = false; }

	private:
		SDL_Window* mp_SDL_Window = nullptr; // SDL Window handle
		unsigned m_SDL_ExtensionCount; // Number of elements in the extension array (Number of extensions in use?)
		std::vector<const char*> mv_SDL_Extensions; // Array of extensions required by SDL for Vulkan

		bool m_resizeNeeded = false;
		


	};



}