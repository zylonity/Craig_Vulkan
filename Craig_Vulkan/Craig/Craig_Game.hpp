#pragma once
#include "Craig_Constants.hpp"

#include <string>

namespace Craig {

	//Forward declarations
	class Renderer;
	class SceneManager;
	class Scene;

	// the bits of the engine the game's allowed to poke
	class GameServices {

	public:
		void init(Craig::Renderer* pRenderer, Craig::SceneManager* pSceneManager);

		// swaps straight away, don't call it from inside a scene/game object loop
		CraigError loadScene(const std::string& scenePath);
		Craig::Scene* getCurrentScene() const;

		// editor: back to edit mode, no editor: closes the app
		// happens next frame
		void requestExit() { m_exitRequested = true; }
		// framework reads this once a frame
		bool consumeExitRequest() { const bool wasRequested = m_exitRequested; m_exitRequested = false; return wasRequested; }

	private:
		Craig::Renderer* mp_renderer = nullptr;
		Craig::SceneManager* mp_sceneManager = nullptr;
		bool m_exitRequested = false;
	};

	// what the engine needs from a game
	// the real one lives outside Craig/ and gets handed over in main.cpp
	// so the engine never includes game code
	class Game {

	public:
		virtual ~Game() = default;

		virtual CraigError init(Craig::GameServices* pServices) = 0;
		virtual CraigError terminate() = 0;

		// a play session starting/ending
		// startedFromEditor = start in the open level, not the main menu
		virtual void onPlayStarted(bool startedFromEditor) = 0;
		virtual void onPlayStopped() = 0;

		// only called while the engine mode runs gameplay
		virtual CraigError update(const float& deltaTime) = 0;

		// false freezes physics + gameplay components but update() still runs (menus, pause menu)
		virtual bool isWorldRunning() const = 0;

		// called inside the editor's ImGui frame
		// stand in menus until there's real game UI
		virtual void drawImGui() {}
	};

}
