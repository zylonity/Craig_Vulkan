
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
#include "Craig_Input.hpp"

CraigError Craig::Window::init() {

	CraigError ret = CRAIG_SUCCESS;

	int sdlRetInt = SDL_Init(SDL_INIT_VIDEO);
	if (sdlRetInt != true) {
		Craig::Logger::engine().critical("SDL_Init failed: {}", SDL_GetError());
	}
	assert(sdlRetInt == true && "Could not initialize SDL.");

	const int sdlVersion = SDL_GetVersion();
	Craig::Logger::engine().info("SDL {}.{}.{} up, video driver: {}", SDL_VERSIONNUM_MAJOR(sdlVersion), SDL_VERSIONNUM_MINOR(sdlVersion), SDL_VERSIONNUM_MICRO(sdlVersion), SDL_GetCurrentVideoDriver());

	mp_SDL_Window = SDL_CreateWindow(kSDL_WindowName, kSDL_WindowWidth, kSDL_WindowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY); // Native res on retina screens, otherwise macOS has to upscale every frame
	if (mp_SDL_Window == NULL) {
		Craig::Logger::engine().critical("SDL_CreateWindow failed: {}", SDL_GetError());
	}
	assert(mp_SDL_Window != NULL && "Could not create SDL window.");

	// Pixel size is bigger than the window size on retina/high DPI screens
	const WindowExtent pixelSize = getDrawableExtent();
	Craig::Logger::engine().info("Window created: {} x {} ({} x {} pixels)", kSDL_WindowWidth, kSDL_WindowHeight, pixelSize.width, pixelSize.height);

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

	// imgui's answer from last frame, it doesn't change while we pump events anyway
	bool mouseOverEditor = false;
#if defined(IMGUI_ENABLED)
	mouseOverEditor = ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureMouse;
#endif

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		m_currentCamera->processSDLEvent(event, mp_SDL_Window);

		bool hideFromImGui = false;

		// tab locks the mouse, unless you're typing then it's imgui's (next field)
		if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_TAB && !Craig::Input::getInstance().isTypingInEditor())
		{
			if (!event.key.repeat)
			{
				setMouseLocked(!m_mouseLocked);
			}
			hideFromImGui = true; // or imgui tabs through every widget as well
		}

		// hold right click over the scene to fly, let go to get the cursor back
		if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT && !m_mouseLocked && !mouseOverEditor)
		{
			setMouseLocked(true);
			m_rightClickFlying = true;
		}
		else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT && m_rightClickFlying)
		{
			setMouseLocked(false);
			m_rightClickFlying = false;
		}

#if defined(IMGUI_ENABLED)
		// locked = flying, imgui shouldn't see the mouse wandering around and clicking stuff
		// wheel still goes through so scrolling works while flying
		const bool isMouseEvent = event.type == SDL_EVENT_MOUSE_MOTION
			|| event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
			|| event.type == SDL_EVENT_MOUSE_BUTTON_UP;
		if (!hideFromImGui && !(m_mouseLocked && isMouseEvent))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
		}
#endif

		switch (event.type) {

		case SDL_EVENT_QUIT:
			Craig::Logger::engine().info("Window closed, quitting");
			ret = CRAIG_CLOSED; // Set the return code to fail to indicate that the window should close
			continue;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: // Moving between retina and non retina screens
			// trace since dragging the window edge fires this loads
			Craig::Logger::engine().trace("Window resized to {} x {}", event.window.data1, event.window.data2);
			m_resizeNeeded = true;
			continue;

		case SDL_EVENT_WINDOW_MINIMIZED:
			Craig::Logger::engine().debug("Window minimised");
			m_resizeNeeded = false;
			continue;

		default:

			continue;
		}
	}

	// after the events are pumped so SDL's keyboard state is up to date
	Craig::Input::getInstance().update();

	return ret;
}

void Craig::Window::setMouseLocked(bool locked) {

	m_mouseLocked = locked;
	SDL_SetWindowRelativeMouseMode(mp_SDL_Window, locked);

	// camera only hears keys while locked, so a WASD release after unlocking never reaches it and you drift forever
	if (m_currentCamera != nullptr)
	{
		m_currentCamera->getVelocity() = glm::vec3(0.0f);
	}

#if defined(IMGUI_ENABLED)
	// NoMouse stops whatever was under the cursor staying hovered
	// NoMouseCursorChange stops imgui's backend un-hiding the cursor every frame
	ImGuiIO& io = ImGui::GetIO();
	constexpr ImGuiConfigFlags kMouseFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
	if (locked)
	{
		io.ConfigFlags |= kMouseFlags;
	}
	else
	{
		io.ConfigFlags &= ~kMouseFlags;
	}
#endif

	Craig::Logger::engine().debug("Mouse {}", locked ? "locked" : "unlocked");
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
	Craig::Logger::engine().debug("Window destroyed and SDL shut down");

	return ret;
}

