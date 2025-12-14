
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
#include "Craig_Camera.hpp"

CraigError Craig::Window::init() {

	CraigError ret = CRAIG_SUCCESS;

	int sdlRetInt = SDL_Init(SDL_INIT_VIDEO);
	assert(sdlRetInt == 0 && "Could not initialize SDL.");

	mp_SDL_Window = SDL_CreateWindow(kSDL_WindowName, SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, kSDL_WindowWidth, kSDL_WindowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	assert(mp_SDL_Window != NULL && "Could not create SDL window.");

	// Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
	bool sdlRetBool = SDL_Vulkan_GetInstanceExtensions(mp_SDL_Window, &m_SDL_ExtensionCount, NULL); // Get the number of required instance extensions
	assert(sdlRetBool && "Could not get the number of required instance extensions from SDL.");

	mv_SDL_Extensions.resize(m_SDL_ExtensionCount); // Resize the vector to hold the required extensions

	sdlRetBool = SDL_Vulkan_GetInstanceExtensions(mp_SDL_Window, &m_SDL_ExtensionCount, mv_SDL_Extensions.data()); // Get the names of the required instance extensions
	assert(sdlRetBool && "Could not get the names of required instance extensions from SDL.");

#if defined(__APPLE__)
	mv_SDL_Extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif


	return ret;
}

CraigError Craig::Window::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		m_currentCamera->processSDLEvent(event);
#if defined(IMGUI_ENABLED)
		//ImGui_ImplSDL2_ProcessEvent(&event);
#endif

		switch (event.type) {

		case SDL_QUIT:
			ret = CRAIG_CLOSED; // Set the return code to fail to indicate that the window should close
			continue;

		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
				m_resizeNeeded = true;
			}
			continue;

		case SDL_KEYUP:
			if (event.key.keysym.sym == SDLK_TAB) {
				m_mouseLocked = !m_mouseLocked;

				if (m_mouseLocked) {
					SDL_SetRelativeMouseMode(SDL_TRUE);
					
				}
				else {
					SDL_SetRelativeMouseMode(SDL_FALSE);
				}
				
			}
			continue;

		default:

			continue;
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

