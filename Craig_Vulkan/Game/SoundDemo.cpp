#define GLM_ENABLE_EXPERIMENTAL
#include "SoundDemo.hpp"
#include <glm/gtx/quaternion.hpp>
#include "Craig/Craig_Scene.hpp"

#include <cassert>
#include <cmath>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

CraigError Game::SoundDemo::init(Craig::GameServices* pServices) {
	assert(pServices != nullptr && "SoundDemo needs the game services");
	mp_services = pServices;
	return mp_services->loadScene("data/scenes/SoundPropagationDemo.json");
}

CraigError Game::SoundDemo::terminate() {
	mp_services->disableSoundProbe();
	return CRAIG_SUCCESS;
}

void Game::SoundDemo::onPlayStarted(bool startedFromEditor) {
	(void)startedFromEditor;
	m_probeReady = mp_services->configureSoundProbe("Listener", "Source", 1) == CRAIG_SUCCESS;
	m_time = 0.0f;
}

void Game::SoundDemo::onPlayStopped() {
	mp_services->disableSoundProbe();
	m_probeReady = false;
}

CraigError Game::SoundDemo::update(const float& deltaTime) {
	//The editor still works on a GPU without ray queries
	if (!m_probeReady) return CRAIG_SUCCESS;
	Craig::Scene& scene = *mp_services->getCurrentScene();
	if (m_animate) {
		m_time += deltaTime;
		if (Craig::GameObject* pDoor = scene.findObject("Door")) {
			pDoor->setPosition({1.3f * (1.0f - std::cos(m_time)), 1.5f, 0.0f});
		}
	}

	//Thin box joining the two markers, it doesn't block sound, just shows where the ray should be
	const auto* pListener = scene.findObject("Listener");
	const auto* pSource = scene.findObject("Source");
	if (pListener && pSource) {
		if (auto* pLine = scene.findObject("Query segment")) {
			const glm::vec3 segment = pSource->getPosition() - pListener->getPosition();
			const float distance = glm::length(segment);
			if (std::isfinite(distance) && distance > 0.002f) {
				pLine->setPosition((pListener->getPosition() + pSource->getPosition()) * 0.5f);
				pLine->setScale({0.015f, 0.015f, distance * 0.5f});
				pLine->setRotationQuat(glm::rotation(glm::vec3(0, 0, 1), segment / distance));
			}
		}
	}
	return CRAIG_SUCCESS;
}

void Game::SoundDemo::drawImGui() {
#if defined(IMGUI_ENABLED)
	ImGui::Begin("Moving-door demo");
	ImGui::TextUnformatted("Small cubes: listener/source, thin line: the segment between them");
	ImGui::Checkbox("Animate door", &m_animate);
	ImGui::TextUnformatted("Use the engine toolbar to play, pause, step and stop");
	ImGui::TextUnformatted("Sound propagation shows the last completed GPU result, not a synchronous check");
	ImGui::End();
#endif
}
