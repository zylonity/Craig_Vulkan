#include "CraigGame.hpp"
#include "Craig/Craig_Scene.hpp"

#include <cassert>

CraigError Game::CraigGame::init(Craig::GameServices* pServices)
{
	CraigError ret = CRAIG_SUCCESS;

	assert(pServices != nullptr && "Game needs the engine's services");
	m_states.getContext().pServices = pServices;
	buildGameStates(m_states);

	return ret;
}

CraigError Game::CraigGame::terminate()
{
	CraigError ret = CRAIG_SUCCESS;

	// should already be stopped, just in case
	m_states.stop();

	return ret;
}

void Game::CraigGame::onPlayStarted(bool startedFromEditor)
{
	GameContext& context = m_states.getContext();

	// whatever's open is the level
	context.firstLevelPath = context.pServices->getCurrentScene()->getScenePath();

	// from the editor you want the scene you've got open, not the menus every damn time
	m_states.start(startedFromEditor ? GameStateId::InLevel : GameStateId::Boot);
}

void Game::CraigGame::onPlayStopped()
{
	m_states.stop();
}

CraigError Game::CraigGame::update(const float& deltaTime)
{
	CraigError ret = CRAIG_SUCCESS;

	// same order as the engine modes
	m_states.applyRequests();
	m_states.update(deltaTime);

	return ret;
}

bool Game::CraigGame::isWorldRunning() const
{
	const GameState* pState = getCurrentState();
	return pState != nullptr && pState->runsWorld();
}

void Game::CraigGame::drawImGui()
{
	if (GameState* pState = getCurrentState())
	{
		pState->drawImGui(m_states.getContext());
	}
}

Game::GameState* Game::CraigGame::getCurrentState() const
{
	// everything in here is a GameState, buildGameStates only adds those
	return static_cast<GameState*>(m_states.getCurrent());
}
