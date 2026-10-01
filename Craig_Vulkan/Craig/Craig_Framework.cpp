
//System includes
#include <cassert>

//Craig includes
#include "Craig_Framework.hpp"
#include "Craig_Window.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_Editor.hpp"
#include "Craig_SceneManager.hpp"
#include "Craig_PhysicsEngine.hpp"
#include "Craig_Profiler.hpp"
#include "Craig_Logger.hpp"

#include <chrono>

// asserts vanish in release, this way a failure still ends up in the log
static void logIfFailed(CraigError ret, const char* what) {
	if (ret != CRAIG_SUCCESS) {
		Craig::Logger::engine().critical("{} failed (CraigError {})", what, static_cast<int>(ret));
	}
}

CraigError Craig::Framework::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Logger goes first so everything after it (validation layers included) ends up in the log
	// not asserted, if the file won't open it still logs to the console and editor
	Craig::Logger::getInstance().init();

	const spdlog::stopwatch startupTimer;
#if defined(_DEBUG)
	constexpr const char* kBuildType = "debug";
#else
	constexpr const char* kBuildType = "release";
#endif
#if defined(IMGUI_ENABLED)
	constexpr const char* kImguiState = "on";
#else
	constexpr const char* kImguiState = "off";
#endif
	Craig::Logger::engine().info("Starting Craig ({} build, ImGui {})", kBuildType, kImguiState);

	//Create our objects and get the pointers we need to initialise later
	mp_Window = new Craig::Window;
	mp_SceneManager = new Craig::SceneManager;
	mp_Renderer = new Craig::Renderer;
	mp_PhysicsEngine = new Craig::PhysicsEngine;

	//Check they were succesfully allocated just in case
	assert(mp_Window != nullptr && "mp_Window failed to allocate memory");
	assert(mp_SceneManager != nullptr && "mp_SceneManager failed to allocate memory");
	assert(mp_Renderer != nullptr && "mp_Renderer failed to allocate memory");
	assert(mp_PhysicsEngine != nullptr && "mp_PhysicsEngine failed to allocate memory");

	//Initialise the objects
	ret = mp_Window->init();
	logIfFailed(ret, "Window init");
	assert(ret == CRAIG_SUCCESS);

	Craig::ImguiEditor::getInstance().setRenderer(mp_Renderer);
	Craig::ImguiEditor::getInstance().setSceneManager(mp_SceneManager);
	Craig::ImguiEditor::getInstance().setPhysicsEngine(mp_PhysicsEngine);
	Craig::ResourceManager::getInstance().init(mp_Renderer); // Initialize the Resource Manager Singleton, this needs to be done before the renderer

	ret = mp_Renderer->init(mp_Window, mp_SceneManager);
	logIfFailed(ret, "Renderer init");
	assert(ret == CRAIG_SUCCESS);

	// before the scene manager so the scenes get a ready physics engine
	ret = mp_PhysicsEngine->init();
	logIfFailed(ret, "Physics init");
	assert(ret == CRAIG_SUCCESS);

	// Has to be between the renderer's two inits, loading the scene's models needs the device
	// and the renderer's buffers need the scene's models
	ret = mp_SceneManager->init(mp_PhysicsEngine);
	logIfFailed(ret, "Scene manager init");
	assert(ret == CRAIG_SUCCESS);

	ret = mp_Renderer->initSceneResources();
	logIfFailed(ret, "Renderer scene resources");
	assert(ret == CRAIG_SUCCESS);

	// after everything the game might want is up
	m_gameServices.init(mp_Renderer, mp_SceneManager);
	if (mp_Game != nullptr) {
		ret = mp_Game->init(&m_gameServices);
		logIfFailed(ret, "Game init");
		assert(ret == CRAIG_SUCCESS);
	}
	else {
		Craig::Logger::engine().info("No game set, it's just the editor");
	}

	// last, entering the first mode can already start the game
	Craig::EngineContext& modeContext = m_engineModes.getContext();
	modeContext.pRenderer = mp_Renderer;
	modeContext.pSceneManager = mp_SceneManager;
	modeContext.pGame = mp_Game;
	Craig::buildEngineModes(m_engineModes);
#if defined(IMGUI_ENABLED)
	Craig::ImguiEditor::getInstance().setEngineModes(&m_engineModes);
	m_engineModes.start(Craig::EngineModeId::Edit);
#else
	// no editor means no Play button, so just play
	m_engineModes.start(Craig::EngineModeId::Play);
#endif

	Craig::Logger::engine().info("Everything's up, startup took {:.0f} ms", startupTimer.elapsed().count() * 1000.0);

	m_LastFrameTime = std::chrono::steady_clock::now();

	return ret;
}


CraigError Craig::Framework::update() {

	CraigError ret = CRAIG_SUCCESS;

	const float elapsed = getElapsedTime();

	{
		CRAIG_PROFILE_SCOPE("Window");
		ret = mp_Window->update(elapsed);
	}
	if (ret != CRAIG_CLOSED) {
		logIfFailed(ret, "Window update");
	}
	assert((ret == CRAIG_SUCCESS || ret == CRAIG_CLOSED) && "mp_Window failed to update");
	if(ret == CRAIG_CLOSED) {
		return CRAIG_CLOSED; // If the window is closed, we return that code
	}

	// game wants out, back to the editor if there is one, otherwise close
	if (m_gameServices.consumeExitRequest()) {
#if defined(IMGUI_ENABLED)
		m_engineModes.requestChange(Craig::EngineModeId::Edit);
#else
		Craig::Logger::engine().info("Game asked to quit");
		return CRAIG_CLOSED;
#endif
	}

	// the only place modes actually change
	m_engineModes.applyRequests();
	const Craig::EngineModeFlags& mode = Craig::getCurrentModeFlags(m_engineModes);

	// stepping = one physics step, not however long the frame took
	const float deltaTime = (mode.singleTick ? mp_PhysicsEngine->getFixedTimeStep() : elapsed) * mode.timeScale;

	m_engineModes.update(deltaTime);

	// game first, it might load a different scene
	const bool gameRunning = mode.runGameplay && mp_Game != nullptr && m_engineModes.getContext().gameSessionActive;
	if (gameRunning) {
		CRAIG_PROFILE_SCOPE("Game");
		ret = mp_Game->update(deltaTime);
		logIfFailed(ret, "Game update");
		assert(ret == CRAIG_SUCCESS && "mp_Game failed to update");
	}
	// menus freeze the world but the game keeps going so it can read input
	const bool worldFrozenByGame = gameRunning && !mp_Game->isWorldRunning();

	// physics steps, then the scene copies the results onto the objects, then it gets drawn
	// otherwise every frame shows the step before
	{
		CRAIG_PROFILE_SCOPE("Physics");
		mp_PhysicsEngine->setSimulating(mode.stepPhysics && !worldFrozenByGame);
		ret = mp_PhysicsEngine->update(deltaTime);
	}
	logIfFailed(ret, "Physics update");
	assert(ret == CRAIG_SUCCESS && "mp_PhysicsEngine failed to update");

	// gameplay before the engine side so model matrices pick up what it moved
	if (mode.runGameplay && !worldFrozenByGame) {
		CRAIG_PROFILE_SCOPE("Gameplay");
		ret = mp_SceneManager->gameplayUpdate(deltaTime);
		logIfFailed(ret, "Scene manager gameplay update");
		assert(ret == CRAIG_SUCCESS && "mp_SceneManager failed to gameplay update");
	}

	{
		CRAIG_PROFILE_SCOPE("SceneManager");
		ret = mp_SceneManager->update(deltaTime);
	}
	logIfFailed(ret, "Scene manager update");
	assert(ret == CRAIG_SUCCESS && "mp_SceneManager failed to update");

	{
		CRAIG_PROFILE_SCOPE("Renderer (total)");
		ret = mp_Renderer->update(elapsed);
	}
	logIfFailed(ret, "Renderer update");
	assert(ret == CRAIG_SUCCESS && "mp_Renderer failed to update");

	CRAIG_PROFILE_END_FRAME();

	return ret;
}

CraigError Craig::Framework::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	Craig::Logger::engine().info("Shutting everything down");

	// modes first, ends the game's session while the scene's still there
	m_engineModes.stop();

	if (mp_Game != nullptr) {
		ret = mp_Game->terminate();
		logIfFailed(ret, "Game terminate");
		assert(ret == CRAIG_SUCCESS && "mp_Game didn't terminate properly");
	}

	// Scenes go before the physics engine, rigid bodies remove themselves from it when they terminate
	ret = mp_SceneManager->terminate();
	logIfFailed(ret, "Scene manager terminate");
	assert(ret == CRAIG_SUCCESS && "mp_SceneManager didn't terminate properly");
	delete mp_SceneManager;
	mp_SceneManager = nullptr;

	ret = mp_PhysicsEngine->terminate();
	logIfFailed(ret, "Physics terminate");
	assert(ret == CRAIG_SUCCESS && "mp_PhysicsEngine failed to terminate");
	delete mp_PhysicsEngine;
	mp_PhysicsEngine = nullptr;

	ret = mp_Renderer->terminate(); //Delete left over items in memory
	logIfFailed(ret, "Renderer terminate");
	assert(ret == CRAIG_SUCCESS && "mp_Renderer didn't terminate properly"); //Check it closed properly
	delete mp_Renderer; //Delete the scene manager
	mp_Renderer = nullptr; //Set the pointer to null (Might not be done by default, just in case)

	ret = mp_Window->terminate(); //Delete left over items in memory
	logIfFailed(ret, "Window terminate");
	assert(ret == CRAIG_SUCCESS && "mp_Window didn't terminate properly"); //Check it closed properly
	delete mp_Window; //Delete the scene manager
	mp_Window = nullptr; //Set the pointer to null (Might not be done by default, just in case)

	Craig::ResourceManager::getInstance().terminate();

	// last so everything else can still log while it shuts down
	ret = Craig::Logger::getInstance().terminate();
	assert(ret == CRAIG_SUCCESS && "Logger didn't terminate properly");

	return ret;
}

float Craig::Framework::getElapsedTime()
{
	// Calculate frame time as current time - time when we last called the function.
	const std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
	const float elapsedTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - m_LastFrameTime).count() / 1000000.f;

	// Update our reference to the recorded time.
	m_LastFrameTime = currentTime;

	return elapsedTime;
}