
#include <glm/glm.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.hpp>

#if defined(IMGUI_ENABLED)
#include "../External/Imgui/imgui.h"
#include "../External/Imgui/imgui_impl_sdl3.h"
#endif

#include "Craig_Window.hpp"
#include "Craig_Camera.hpp"
#include "Craig_Logger.hpp"

CraigError Craig::Window::init() {

	CraigError ret = CRAIG_SUCCESS;

	int sdlRetInt = SDL_Init(SDL_INIT_VIDEO);
	assert(sdlRetInt == true && "Could not initialize SDL.");

	mp_SDL_Window = SDL_CreateWindow(kSDL_WindowName, kSDL_WindowWidth, kSDL_WindowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY); // Native res on retina screens, otherwise macOS has to upscale every frame
	assert(mp_SDL_Window != NULL && "Could not create SDL window.");

	// Get WSI extensions from SDL (we can add more if we like - we just can't remove these)
	// SDL3 returns its own array (owned by SDL, don't free it) and writes the count
	const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&m_SDL_ExtensionCount);
	if (sdlExtensions == nullptr) {
		Craig::Logger::engine().critical("SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
	}
	assert(sdlExtensions != nullptr && "Could not get the required instance extensions from SDL.");

	mv_SDL_Extensions.assign(sdlExtensions, sdlExtensions + m_SDL_ExtensionCount);

#if defined(__APPLE__)
	mv_SDL_Extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif


	return ret;
}

CraigError Craig::Window::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		m_currentCamera->processSDLEvent(event, mp_SDL_Window);
#if defined(IMGUI_ENABLED)
		ImGui_ImplSDL3_ProcessEvent(&event);
#endif

		switch (event.type) {

		case SDL_EVENT_QUIT:
			ret = CRAIG_CLOSED; // Set the return code to fail to indicate that the window should close
			continue;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: // Moving between retina and non retina screens
			m_resizeNeeded = true;
			continue;

		case SDL_EVENT_WINDOW_MINIMIZED:
			m_resizeNeeded = false;
			continue;

		case SDL_EVENT_KEY_UP:
			if (event.key.key == SDLK_TAB) {
				m_mouseLocked = !m_mouseLocked;
				SDL_SetWindowRelativeMouseMode(mp_SDL_Window, m_mouseLocked);
				
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
	SDL_GetWindowSizeInPixels(mp_SDL_Window, &w, &h);
	return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}

CraigError Craig::Window::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	SDL_DestroyWindow(mp_SDL_Window);
	SDL_Quit();

	return ret;
}

