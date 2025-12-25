#include "Craig_SceneManager.hpp"
#include <cassert>

CraigError Craig::SceneManager::init() {

	CraigError ret = CRAIG_SUCCESS;

	// Initialize our scene
	mp_CurrentScene = new Craig::Scene;
	assert(mp_CurrentScene != nullptr && "mp_CurrentScene failed to allocate memory");
	ret = mp_CurrentScene->init();
	assert(ret == CRAIG_SUCCESS);

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


