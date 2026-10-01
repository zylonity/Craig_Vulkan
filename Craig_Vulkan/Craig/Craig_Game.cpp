#include "Craig_Game.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_SceneManager.hpp"

#include <cassert>

void Craig::GameServices::init(Craig::Renderer* pRenderer, Craig::SceneManager* pSceneManager)
{
	assert(pRenderer != nullptr && pSceneManager != nullptr && "GameServices needs the renderer and scene manager");
	mp_renderer = pRenderer;
	mp_sceneManager = pSceneManager;
}

CraigError Craig::GameServices::loadScene(const std::string& scenePath)
{
	// through the renderer, it knows to wait for the GPU and rebuild the buffers
	return mp_renderer->loadScene(scenePath);
}

Craig::Scene* Craig::GameServices::getCurrentScene() const
{
	return mp_sceneManager->getCurrentScene();
}
