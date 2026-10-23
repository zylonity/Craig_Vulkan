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

CraigError Craig::GameServices::configureSoundProbe(const std::string& listener, const std::string& source, uint64_t sourceId)
{
	return mp_renderer->configureSoundProbe(listener, source, sourceId);
}

void Craig::GameServices::disableSoundProbe()
{
	mp_renderer->disableSoundProbe();
}

Craig::SoundProbeReadout Craig::GameServices::getSoundProbeReadout() const
{
	return mp_renderer->getSoundProbeReadout();
}

Craig::Scene* Craig::GameServices::getCurrentScene() const
{
	return mp_sceneManager->getCurrentScene();
}
