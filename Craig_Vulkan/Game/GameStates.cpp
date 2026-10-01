#include "GameStates.hpp"
#include "Craig/Craig_Game.hpp"
#include "Craig/Craig_Input.hpp"
#include "Craig/Craig_Logger.hpp"
#include "imgui.h"

#include <memory>

// stand in menu window, middle of the screen
static bool beginMenuWindow(const char* title)
{
	const ImVec2 centre(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
	ImGui::SetNextWindowPos(centre, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	return ImGui::Begin(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings);
}

//===============================================================================
// Boot

void Game::BootState::update(GameContext& context, const float& deltaTime)
{
	// settings, saves etc would load here, nothing yet
	requestChange(GameStateId::MainMenu);
}

//===============================================================================
// Main menu

void Game::MainMenuState::onEnter(GameContext& context, const State* pFrom)
{
	// no real UI yet, so at least log the keys
	Craig::Logger::engine().info("Main menu: Enter to start, Escape to quit");
}

void Game::MainMenuState::update(GameContext& context, const float& deltaTime)
{
	Craig::Input& input = Craig::Input::getInstance();
	if (input.wasKeyPressed(SDL_SCANCODE_RETURN))
	{
		startGame(context);
	}
	else if (input.wasKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		context.pServices->requestExit();
	}
}

void Game::MainMenuState::drawImGui(GameContext& context)
{
	if (beginMenuWindow("Main Menu"))
	{
		if (ImGui::Button("Start (Enter)"))
		{
			startGame(context);
		}
		if (ImGui::Button("Quit (Escape)"))
		{
			context.pServices->requestExit();
		}
	}
	ImGui::End();
}

void Game::MainMenuState::startGame(GameContext& context)
{
	context.levelToLoad = context.firstLevelPath;
	requestChange(GameStateId::Loading);
}

//===============================================================================
// Loading

void Game::LoadingState::onEnter(GameContext& context, const State* pFrom)
{
	m_shownOneFrame = false;
}

void Game::LoadingState::update(GameContext& context, const float& deltaTime)
{
	// the load blocks the whole frame, let one go by first or you'd never see the loading screen
	// proper async loading is its own whole job
	if (!m_shownOneFrame)
	{
		m_shownOneFrame = true;
		return;
	}

	if (context.pServices->loadScene(context.levelToLoad) != CRAIG_SUCCESS)
	{
		Craig::Logger::scene().error("Couldn't load level {}, back to the main menu", context.levelToLoad);
		requestChange(GameStateId::MainMenu);
		return;
	}

	requestChange(GameStateId::InLevel);
}

void Game::LoadingState::drawImGui(GameContext& context)
{
	if (beginMenuWindow("Loading"))
	{
		ImGui::Text("Loading %s...", context.levelToLoad.c_str());
	}
	ImGui::End();
}

//===============================================================================
// In level

void Game::InLevelState::onEnter(GameContext& context, const State* pFrom)
{
	Craig::Logger::engine().info("In level: Escape to pause");
}

void Game::InLevelState::update(GameContext& context, const float& deltaTime)
{
	if (Craig::Input::getInstance().wasKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		requestPush(GameStateId::PauseMenu);
	}
}

//===============================================================================
// Pause menu

void Game::PauseMenuState::onEnter(GameContext& context, const State* pFrom)
{
	Craig::Logger::engine().info("Paused: Escape to resume, Q to quit to the main menu");
}

void Game::PauseMenuState::update(GameContext& context, const float& deltaTime)
{
	Craig::Input& input = Craig::Input::getInstance();
	if (input.wasKeyPressed(SDL_SCANCODE_ESCAPE))
	{
		requestPop();
	}
	else if (input.wasKeyPressed(SDL_SCANCODE_Q))
	{
		quitToMenu();
	}
}

void Game::PauseMenuState::drawImGui(GameContext& context)
{
	if (beginMenuWindow("Paused"))
	{
		if (ImGui::Button("Resume (Escape)"))
		{
			requestPop();
		}
		if (ImGui::Button("Quit to menu (Q)"))
		{
			quitToMenu();
		}
	}
	ImGui::End();
}

void Game::PauseMenuState::quitToMenu()
{
	// both happen in order next frame, pop back to the level then swap it for the menu
	requestPop();
	requestChange(GameStateId::MainMenu);
}

//===============================================================================

void Game::buildGameStates(GameStateMachine& machine)
{
	machine.addState(GameStateId::Boot, std::make_unique<BootState>());
	machine.addState(GameStateId::MainMenu, std::make_unique<MainMenuState>());
	machine.addState(GameStateId::Loading, std::make_unique<LoadingState>());
	machine.addState(GameStateId::InLevel, std::make_unique<InLevelState>());
	machine.addState(GameStateId::PauseMenu, std::make_unique<PauseMenuState>());

	// anything not in here gets refused
	// pops don't need a row
	machine.allowTransition(GameStateId::Boot, GameStateId::MainMenu);
	machine.allowTransition(GameStateId::MainMenu, GameStateId::Loading);
	machine.allowTransition(GameStateId::Loading, GameStateId::InLevel);
	machine.allowTransition(GameStateId::Loading, GameStateId::MainMenu); // load failed
	machine.allowTransition(GameStateId::InLevel, GameStateId::PauseMenu); // push
	machine.allowTransition(GameStateId::InLevel, GameStateId::MainMenu);
	machine.allowTransition(GameStateId::InLevel, GameStateId::Loading); // next level, once there is one
}
