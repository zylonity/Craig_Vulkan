#pragma once
#include <SDL3/SDL_scancode.h>

#include <array>

namespace Craig {

	// keyboard state for gameplay
	// Window::update() refreshes it every frame
	// scancodes are physical keys so WASD stays put on other layouts
	class Input {

	public:
		void update();

		// held down right now
		bool isKeyDown(SDL_Scancode key) const;
		// went down this frame, so holding a key doesn't fire every frame
		bool wasKeyPressed(SDL_Scancode key) const;

		//===============================================================================
		// Singleton Implementations
		static Input& getInstance()
		{
			static Input instance; // Guaranteed to be destroyed.
			return instance;
		}
		// Make deleted functions public for nicer error messages (~ Scott Myers)
		Input(Input const&) = delete;			// Copy constructor
		void operator=(Input const&) = delete;	// Assignment Operator
		//===============================================================================
	private:

		//===============================================================================
		// Singleton Implementations (Banned functions to prevent a new instance)
		Input() {}	// Default Constructor private so can only be called from within
		//===============================================================================

		// typing in an editor text box shouldn't also pause the game
		bool isTypingInEditor() const;

		std::array<bool, SDL_SCANCODE_COUNT> m_currentKeys{};
		std::array<bool, SDL_SCANCODE_COUNT> m_previousKeys{};
	};

}
