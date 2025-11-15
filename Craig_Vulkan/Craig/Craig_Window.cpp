
#include <glm/glm.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#if defined(IMGUI_ENABLED)
#include "../External/Imgui/imgui.h"   
#include "../External/Imgui/imgui_impl_sdl2.h"
#endif

#include "Craig_Window.hpp"

CraigError Craig::Window::init() {

	CraigError ret = CRAIG_SUCCESS;

	int sdlRetInt = SDL_Init(SDL_INIT_VIDEO);
	assert(sdlRetInt == 0 && "Could not initialize SDL.");

	mp_SDL_Window = SDL_CreateWindow(kSDL_WindowName, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, kSDL_WindowWidth, kSDL_WindowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	assert(mp_SDL_Window != NULL && "Could not create SDL window.");

	// Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
	bool sdlRetBool = SDL_Vulkan_GetInstanceExtensions(mp_SDL_Window, &m_SDL_ExtensionCount, NULL); // Get the number of required instance extensions
	assert(sdlRetBool && "Could not get the number of required instance extensions from SDL.");

	mv_SDL_Extensions.resize(m_SDL_ExtensionCount); // Resize the vector to hold the required extensions

	sdlRetBool = SDL_Vulkan_GetInstanceExtensions(mp_SDL_Window, &m_SDL_ExtensionCount, mv_SDL_Extensions.data()); // Get the names of the required instance extensions
	assert(sdlRetBool && "Could not get the names of required instance extensions from SDL.");


	return ret;
}

CraigError Craig::Window::update() {

	CraigError ret = CRAIG_SUCCESS;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
#if defined(IMGUI_ENABLED)
		ImGui_ImplSDL2_ProcessEvent(&event);
#endif

		switch (event.type) {

		case SDL_QUIT:
			ret = CRAIG_CLOSED; // Set the return code to fail to indicate that the window should close
			break;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				m_resizeNeeded = true;
			}
			break;

		default:
			// Do nothing.
			break;
		}
	}


	return ret;
}

Craig::Window::WindowExtent Craig::Window::getDrawableExtent() const {
	int w = 0, h = 0;
	SDL_Vulkan_GetDrawableSize(mp_SDL_Window, &w, &h);
	return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}

CraigError Craig::Window::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	SDL_DestroyWindow(mp_SDL_Window);
	SDL_Quit();

	return ret;
}

