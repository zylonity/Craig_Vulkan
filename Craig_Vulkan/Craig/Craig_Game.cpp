#include "Craig_Game.hpp"
#include "Craig_Renderer.hpp"
#include "Craig_SceneManager.hpp"

#include <cassert>

void Craig::GameServices::init(Craig::Renderer* pRenderer, Craig::SceneManager* pSceneManager, Craig::Audio* pAudio)
{
	assert(pRenderer != nullptr && pSceneManager != nullptr && pAudio != nullptr && "GameServices needs the engine's services");
	mp_renderer = pRenderer;
	mp_sceneManager = pSceneManager;
	mp_audio = pAudio;
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

CraigError Craig::GameServices::playSoundLoop(const std::string& path) { return mp_audio->playLoop(path); }
CraigError Craig::GameServices::prepareSoundEffect(const std::string& path) { return mp_audio->prepareOneShot(path); }
void Craig::GameServices::playSoundEffect() { mp_audio->playOneShot(); }
void Craig::GameServices::stopSoundLoop() { mp_audio->stop(); }
void Craig::GameServices::setSoundMuted(bool muted) { mp_audio->setMuted(muted); }
void Craig::GameServices::setSoundPathsEnabled(bool direct, bool reflections) { mp_audio->setPathsEnabled(direct, reflections); }
Craig::AudioStatus Craig::GameServices::getAudioStatus() const { return mp_audio->getStatus(); }

Craig::Scene* Craig::GameServices::getCurrentScene() const
{
	return mp_sceneManager->getCurrentScene();
}
