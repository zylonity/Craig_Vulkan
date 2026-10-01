#include "Craig_Input.hpp"

#include <SDL3/SDL_keyboard.h>

#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

void Craig::Input::update()
{
	m_previousKeys = m_currentKeys;

	// SDL owns this and keeps it updated, no need to free it
	int keyCount = 0;
	const bool* pKeys = SDL_GetKeyboardState(&keyCount);
	for (int i = 0; i < keyCount && i < SDL_SCANCODE_COUNT; i++)
	{
		m_currentKeys[i] = pKeys[i];
	}
}

bool Craig::Input::isKeyDown(SDL_Scancode key) const
{
	return !isTypingInEditor() && m_currentKeys[key];
}

bool Craig::Input::wasKeyPressed(SDL_Scancode key) const
{
	return !isTypingInEditor() && m_currentKeys[key] && !m_previousKeys[key];
}

bool Craig::Input::isTypingInEditor() const
{
#if defined(IMGUI_ENABLED)
	// WantTextInput not WantCaptureKeyboard, that one's true basically all the time
	return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
#else
	return false;
#endif
}
