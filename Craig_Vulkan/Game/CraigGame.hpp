#pragma once
#include "Craig/Craig_Game.hpp"
#include "GameStates.hpp"

namespace Game {

	// the actual game, the engine only sees it as a Craig::Game
	class CraigGame : public Craig::Game {

	public:
		CraigError init(Craig::GameServices* pServices) override;
		CraigError terminate() override;

		void onPlayStarted(bool startedFromEditor) override;
		void onPlayStopped() override;

		CraigError update(const float& deltaTime) override;
		bool isWorldRunning() const override;
		void drawImGui() override;

	private:
		GameState* getCurrentState() const;

		GameStateMachine m_states{ "Game" };
	};

}
