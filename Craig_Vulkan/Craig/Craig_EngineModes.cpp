#include "Craig_EngineModes.hpp"
#include "Craig_Game.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_SceneManager.hpp"
#include "Craig_Utilities.hpp"
#include "Craig_Logger.hpp"

#include <memory>

static void startGameSession(Craig::EngineContext& context, bool startedFromEditor)
{
	if (context.pGame == nullptr || context.gameSessionActive)
	{
		return;
	}

	Craig::Logger::state().info("Game session starting ({})", startedFromEditor ? "from the editor" : "standalone");
	context.gameSessionActive = true;
	context.pGame->onPlayStarted(startedFromEditor);
}

static void endGameSession(Craig::EngineContext& context)
{
	if (context.pGame == nullptr || !context.gameSessionActive)
	{
		return;
	}

	Craig::Logger::state().info("Game session over");
	context.pGame->onPlayStopped();
	context.gameSessionActive = false;
}

void Craig::EngineMode::onEnter(EngineContext& context, const State* pFrom)
{
	// nothing before us = started straight into this mode (no editor)
	if (m_flags.runGameplay)
	{
		startGameSession(context, pFrom != nullptr);
	}
}

void Craig::EngineMode::onExit(EngineContext& context, const State* pTo)
{
	// app's closing mid play, let the game clean up
	if (pTo == nullptr)
	{
		endGameSession(context);
	}
}

//===============================================================================
// Edit

Craig::EditMode::EditMode() : EngineMode("Edit", EngineModeFlags{ .stepPhysics = false, .runGameplay = false }) {}

void Craig::EditMode::onExit(EngineContext& context, const State* pTo)
{
	// shutting down, no point snapshotting
	if (pTo == nullptr)
	{
		EngineMode::onExit(context, pTo);
		return;
	}

	// what Stop goes back to
	const Craig::Scene* pScene = context.pSceneManager->getCurrentScene();
	m_sceneSnapshot = pScene->toJson();
	m_snapshotScenePath = pScene->getScenePath();
	m_hasSnapshot = true;

	Craig::Logger::state().debug("Snapshotted '{}' ({} game objects)", pScene->getName(), m_sceneSnapshot["gameObjects"].size());
}

void Craig::EditMode::onEnter(EngineContext& context, const State* pFrom)
{
	// game first so it's done with the scene before it gets swapped out
	endGameSession(context);

	if (!m_hasSnapshot)
	{
		return; // first time in, nothing to put back
	}

	// keep the camera where it is, jumping back to where it was before play is just annoying
	Craig::Camera& camera = context.pSceneManager->getCurrentScene()->getCamera();
	nlohmann::json cameraJson = nlohmann::json::object();
	Utilities::writeJsonVec3(cameraJson, "position", camera.getPosition());
	Utilities::writeJsonVec2(cameraJson, "rotation", camera.getRotation());
	m_sceneSnapshot["camera"] = cameraJson;

	if (context.pRenderer->restoreScene(m_sceneSnapshot, m_snapshotScenePath) != CRAIG_SUCCESS)
	{
		// shouldn't happen, it's the same json we just made
		Craig::Logger::state().error("Couldn't put the scene back after playing, you're looking at the played version. Don't save it!");
	}

	m_sceneSnapshot.clear();
	m_snapshotScenePath.clear();
	m_hasSnapshot = false;
}

//===============================================================================
// Step

Craig::StepMode::StepMode() : EngineMode("Step", EngineModeFlags{ .stepPhysics = true, .runGameplay = true, .singleTick = true }) {}

void Craig::StepMode::update(EngineContext& context, const float& deltaTime)
{
	// got our one frame, back to paused next frame
	requestChange(EngineModeId::Pause);
}

//===============================================================================

void Craig::buildEngineModes(EngineModeMachine& machine)
{
	// modes with nothing special are just a name and some flags
	machine.addState(EngineModeId::Edit, std::make_unique<EditMode>());
	machine.addState(EngineModeId::Play, std::make_unique<EngineMode>("Play", EngineModeFlags{ .stepPhysics = true, .runGameplay = true }));
	machine.addState(EngineModeId::Pause, std::make_unique<EngineMode>("Pause", EngineModeFlags{ .stepPhysics = false, .runGameplay = false }));
	machine.addState(EngineModeId::Step, std::make_unique<StepMode>());
	machine.addState(EngineModeId::Simulate, std::make_unique<EngineMode>("Simulate", EngineModeFlags{ .stepPhysics = true, .runGameplay = false }));

	// anything not in here gets refused
	machine.allowTransition(EngineModeId::Edit, EngineModeId::Play);
	machine.allowTransition(EngineModeId::Edit, EngineModeId::Simulate);

	machine.allowTransition(EngineModeId::Play, EngineModeId::Pause);
	machine.allowTransition(EngineModeId::Play, EngineModeId::Edit);

	machine.allowTransition(EngineModeId::Pause, EngineModeId::Play);
	machine.allowTransition(EngineModeId::Pause, EngineModeId::Step);
	machine.allowTransition(EngineModeId::Pause, EngineModeId::Edit);

	machine.allowTransition(EngineModeId::Step, EngineModeId::Pause);
	machine.allowTransition(EngineModeId::Step, EngineModeId::Edit);

	// no pausing a simulate, Pause -> Play would kick gameplay off
	machine.allowTransition(EngineModeId::Simulate, EngineModeId::Edit);
}

const Craig::EngineModeFlags& Craig::getCurrentModeFlags(const EngineModeMachine& machine)
{
	// everything in the engine machine is an EngineMode, buildEngineModes only adds those
	const EngineMode* pMode = static_cast<const EngineMode*>(machine.getCurrent());
	assert(pMode != nullptr && "Engine modes haven't started yet");
	return pMode->getFlags();
}
