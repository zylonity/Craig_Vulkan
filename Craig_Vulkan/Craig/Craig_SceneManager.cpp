#include "Craig_SceneManager.hpp"
#include <cassert>

CraigError Craig::SceneManager::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Initialize our scene
	mp_CurrentScene = new Craig::Scene;
	assert(mp_CurrentScene != nullptr && "mp_CurrentScene failed to allocate memory");
	ret = mp_CurrentScene->init(kDefaultScenePath);
	assert(ret == CRAIG_SUCCESS && "Default scene failed to load, check the .json");

	return ret;
}

CraigError Craig::SceneManager::loadScene(const std::string& scenePath) {

	CraigError ret = CRAIG_SUCCESS;

	// load the new scene first, so if it fails we still have the old one
	Craig::Scene* pNewScene = new Craig::Scene;
	ret = pNewScene->init(scenePath);
	if (ret != CRAIG_SUCCESS)
	{
		pNewScene->terminate();
		delete pNewScene;
		return ret;
	}

	// swap it in and clean up the old one
	mp_CurrentScene->terminate();
	delete mp_CurrentScene;
	mp_CurrentScene = pNewScene;

	return ret;
}

CraigError Craig::SceneManager::update(const float& deltaTime) {

	CraigError ret = CRAIG_SUCCESS;

	mp_CurrentScene->update(deltaTime);

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


