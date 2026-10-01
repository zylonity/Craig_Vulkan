#include "Craig_SceneManager.hpp"
#include "Craig_Logger.hpp"
#include <cassert>

CraigError Craig::SceneManager::init(Craig::PhysicsEngine* pPhysicsEngine) {

	CraigError ret = CRAIG_SUCCESS;

	assert(pPhysicsEngine != nullptr && "SceneManager needs the physics engine");
	mp_physicsEngine = pPhysicsEngine;

	// Initialize our scene
	mp_CurrentScene = new Craig::Scene;
	assert(mp_CurrentScene != nullptr && "mp_CurrentScene failed to allocate memory");
	ret = mp_CurrentScene->init(kDefaultScenePath, mp_physicsEngine);
	if (ret != CRAIG_SUCCESS) {
		Craig::Logger::scene().critical("The default scene ({}) didn't load, and there's nothing to fall back on", kDefaultScenePath);
	}
	assert(ret == CRAIG_SUCCESS && "Default scene failed to load, check the .json");

	return ret;
}

CraigError Craig::SceneManager::loadScene(const std::string& scenePath) {

	CraigError ret = CRAIG_SUCCESS;

	Craig::Logger::scene().info("Switching scene: {} -> {}", mp_CurrentScene->getScenePath(), scenePath);

	// load the new scene first, so if it fails we still have the old one
	Craig::Scene* pNewScene = new Craig::Scene;
	ret = pNewScene->init(scenePath, mp_physicsEngine);

	return swapInScene(pNewScene, ret, scenePath);
}

CraigError Craig::SceneManager::loadSceneFromJson(const nlohmann::json& sceneJson, const std::string& scenePath) {

	CraigError ret = CRAIG_SUCCESS;

	Craig::Logger::scene().info("Rebuilding {} from memory", scenePath);

	// same deal as loadScene, build the new one before binning the old one
	Craig::Scene* pNewScene = new Craig::Scene;
	ret = pNewScene->initFromJson(sceneJson, scenePath, mp_physicsEngine);

	return swapInScene(pNewScene, ret, scenePath);
}

CraigError Craig::SceneManager::swapInScene(Craig::Scene* pNewScene, CraigError initResult, const std::string& scenePath) {

	if (initResult != CRAIG_SUCCESS)
	{
		Craig::Logger::scene().error("Couldn't load {}, sticking with {}", scenePath, mp_CurrentScene->getScenePath());
		pNewScene->terminate();
		delete pNewScene;
		return initResult;
	}

	// swap it in and clean up the old one
	mp_CurrentScene->terminate();
	delete mp_CurrentScene;
	mp_CurrentScene = pNewScene;

	return CRAIG_SUCCESS;
}

CraigError Craig::SceneManager::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	mp_CurrentScene->update(deltaTime);

	return ret;
}

CraigError Craig::SceneManager::gameplayUpdate(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	mp_CurrentScene->gameplayUpdate(deltaTime);

	return ret;
}


CraigError Craig::SceneManager::terminate() {

	CraigError ret = CRAIG_SUCCESS;

	ret = mp_CurrentScene->terminate();
	assert(ret == CRAIG_SUCCESS && "mp_CurrentScene didn't terminate properly");
	delete mp_CurrentScene;
	mp_CurrentScene = nullptr;

	return ret;
}


