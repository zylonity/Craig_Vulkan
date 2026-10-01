#pragma once
#include "Craig/StateMachine/Craig_StateMachine.hpp"

#include <string>

namespace Craig { class GameServices; }

namespace Game {

	// the game's flow, seperate from the engine's edit/play/pause
	enum class GameStateId {
		Boot,		// straight to the main menu for now
		MainMenu,
		Loading,	// loads GameContext::levelToLoad then goes in to it
		InLevel,
		PauseMenu,	// pushed on top of InLevel, popping it goes back to the level
	};

	// what every game state gets to touch
	struct GameContext {
		Craig::GameServices* pServices = nullptr;
		std::string firstLevelPath; // where Start on the main menu goes
		std::string levelToLoad; // set this then change to Loading
	};

	class GameState : public Craig::State<GameContext, GameStateId> {

	public:
		// whether physics/gameplay run while this is on top
		// off by default so menus don't have stuff falling about
		virtual bool runsWorld() const { return false; }
		// stand in UI until there's a real one (editor builds only)
		virtual void drawImGui(GameContext& context) {}
	};

	using GameStateMachine = Craig::StateMachine<GameContext, GameStateId>;

	//===============================================================================

	class BootState : public GameState {
	public:
		const char* getName() const override { return "Boot"; }
		void update(GameContext& context, const float& deltaTime) override;
	};

	class MainMenuState : public GameState {
	public:
		const char* getName() const override { return "MainMenu"; }
		void onEnter(GameContext& context, const State* pFrom) override;
		void update(GameContext& context, const float& deltaTime) override;
		void drawImGui(GameContext& context) override;
	private:
		void startGame(GameContext& context);
	};

	class LoadingState : public GameState {
	public:
		const char* getName() const override { return "Loading"; }
		void onEnter(GameContext& context, const State* pFrom) override;
		void update(GameContext& context, const float& deltaTime) override;
		void drawImGui(GameContext& context) override;
	private:
		bool m_shownOneFrame = false; // so the loading screen actually gets drawn first
	};

	class InLevelState : public GameState {
	public:
		const char* getName() const override { return "InLevel"; }
		bool runsWorld() const override { return true; }
		void onEnter(GameContext& context, const State* pFrom) override;
		void update(GameContext& context, const float& deltaTime) override;
	};

	class PauseMenuState : public GameState {
	public:
		const char* getName() const override { return "PauseMenu"; }
		void onEnter(GameContext& context, const State* pFrom) override;
		void update(GameContext& context, const float& deltaTime) override;
		void drawImGui(GameContext& context) override;
	private:
		void quitToMenu();
	};

	// adds every state and which ones can go to which
	void buildGameStates(GameStateMachine& machine);

}
