#define GLM_ENABLE_EXPERIMENTAL
#include "SoundDemo.hpp"
#include <glm/gtx/quaternion.hpp>
#include "Craig/Craig_Scene.hpp"

#include <array>
#include <cassert>
#include <cmath>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

namespace {
	//Clicks make separate echoes easy to hear, a steady tone mostly just changes colour
	constexpr std::array<const char*, 3> kSoundNames = {"Impact (button)", "Clicks (loop)", "Tone (loop)"};
	constexpr std::array<const char*, 3> kSoundPaths = {"data/audio/impact.wav", "data/audio/clicks_loop.wav", "data/audio/door_loop.wav"};
	constexpr std::array<bool, 3> kSoundIsOneShot = {true, false, false};
}

CraigError Game::SoundDemo::init(Craig::GameServices* pServices) {
	assert(pServices != nullptr && "SoundDemo needs the game services");
	mp_services = pServices;
	return mp_services->loadScene("data/scenes/SoundPropagationDemo.json");
}

CraigError Game::SoundDemo::terminate() {
	mp_services->disableSoundProbe();
	mp_services->stopSoundLoop();
	return CRAIG_SUCCESS;
}

void Game::SoundDemo::onPlayStarted(bool startedFromEditor) {
	(void)startedFromEditor;
	m_probeReady = mp_services->configureSoundProbe("Listener", "Source", 1) == CRAIG_SUCCESS;
	m_time = 0.0f;
	m_referenceCase = ReferenceCase::MovingDoor;
	m_referenceFailed = false;
	if (m_probeReady) {
		if (kSoundIsOneShot[m_sound]) mp_services->prepareSoundEffect(kSoundPaths[m_sound]);
		else mp_services->playSoundLoop(kSoundPaths[m_sound]);
	}
}

void Game::SoundDemo::onPlayStopped() {
	mp_services->disableSoundProbe();
	mp_services->stopSoundLoop();
	m_probeReady = false;
}

CraigError Game::SoundDemo::selectReferenceCase(ReferenceCase referenceCase) {
	const bool movingDoor = referenceCase == ReferenceCase::MovingDoor;
	const char* path = movingDoor ? "data/scenes/SoundPropagationDemo.json" : "data/scenes/SoundPropagationReference.json";
	if (mp_services->loadScene(path) != CRAIG_SUCCESS) return CRAIG_FAIL;
	if (!movingDoor) {
		Craig::GameObject* pWall = mp_services->getCurrentScene()->findObject("Reference wall");
		assert(pWall != nullptr && "The reference scene needs its wall");
		//BlankBox is [-1,1], wall scale.x is 2, so centred at -2 its right edge is exactly x = 0
		//That covers half of the symmetric emitter, x = 6 moves it completely out of the way
		const float x = referenceCase == ReferenceCase::Clear ? 6.0f :
			(referenceCase == ReferenceCase::Half ? -2.0f : 0.0f);
		pWall->setPosition({x, 2.0f, 0.0f});
	}
	m_referenceCase = referenceCase;
	m_time = 0.0f;
	m_probeReady = mp_services->configureSoundProbe("Listener", "Source", 1) == CRAIG_SUCCESS;
	return m_probeReady ? CRAIG_SUCCESS : CRAIG_FAIL;
}

CraigError Game::SoundDemo::update(const float& deltaTime) {
	//The editor still works on a GPU without ray queries
	if (!m_probeReady) return CRAIG_SUCCESS;
	Craig::Scene& scene = *mp_services->getCurrentScene();
	if (m_animate && m_referenceCase == ReferenceCase::MovingDoor) {
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
	if (m_referenceCase == ReferenceCase::MovingDoor) ImGui::Checkbox("Animate door", &m_animate);
	ImGui::SeparatorText("Geometric references");
	const char* cases[] = {"Moving door", "Clear (0%)", "Half (50%)", "Blocked (100%)"};
	for (int i = 0; i < 4; ++i) {
		if (i != 0) ImGui::SameLine();
		if (ImGui::Button(cases[i])) m_referenceFailed = selectReferenceCase(static_cast<ReferenceCase>(i)) != CRAIG_SUCCESS;
	}
	if (m_referenceFailed) ImGui::TextUnformatted("Scene/probe setup failed, see the log");
	if (m_referenceCase != ReferenceCase::MovingDoor) {
		const float expected = m_referenceCase == ReferenceCase::Clear ? 0.0f :
			(m_referenceCase == ReferenceCase::Half ? 0.5f : 1.0f);
		ImGui::Text("Expected obstruction: %.0f%% (if the reference scene hasn't been edited)", expected * 100.0f);
		ImGui::TextUnformatted("Half needs multiple rays and a non-zero emitter width");
		ImGui::TextUnformatted("Finite samples won't be exactly 50%, change the seed to try another set");
		const auto probe = mp_services->getSoundProbeReadout();
		if (probe.hasResult) ImGui::Text("GPU: %.4f, difference: %.4f", probe.obstruction, std::abs(probe.obstruction - expected));
	}
	ImGui::Separator();
	ImGui::TextUnformatted("Use the engine toolbar to play, pause, step and stop");
	ImGui::TextUnformatted("Sound propagation shows the last completed GPU result, not a synchronous check");
	const auto audio = mp_services->getAudioStatus();
	ImGui::Text("Audio: %s", Craig::Audio::stateName(audio.state));
	if (kSoundIsOneShot[m_sound]) {
		const auto probe = mp_services->getSoundProbeReadout();
		const bool ready = m_probeReady && audio.available && !audio.muted && probe.hasResult &&
			probe.state == Craig::SoundProbeReadout::State::Ready && probe.ageMs >= 0.0 &&
			probe.ageMs < Craig::Audio::kMaxResultAgeMs &&
			(!probe.reverbEnabled || !audio.reflectionsEnabled || audio.reverbReady);
		ImGui::BeginDisabled(!ready);
		if (ImGui::Button("Play sound")) mp_services->playSoundEffect();
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextUnformatted("Each press adds another, earlier ones keep playing");
		if (!ready) ImGui::TextUnformatted("Needs Play, unmuted audio and fresh paths, the tail might still be getting built");
	}
	if (audio.available) {
		bool muted = audio.muted;
		bool direct = audio.directEnabled;
		bool reflections = audio.reflectionsEnabled;
		if (ImGui::Checkbox("Mute", &muted)) mp_services->setSoundMuted(muted);
		ImGui::SameLine();
		if (ImGui::Checkbox("Direct", &direct)) mp_services->setSoundPathsEnabled(direct, reflections);
		ImGui::SameLine();
		if (ImGui::Checkbox("Reflections", &reflections)) mp_services->setSoundPathsEnabled(direct, reflections);
		if (ImGui::Combo("Sound", &m_sound, kSoundNames.data(), static_cast<int>(kSoundNames.size())) && m_probeReady) {
			if (kSoundIsOneShot[m_sound]) mp_services->prepareSoundEffect(kSoundPaths[m_sound]);
			else mp_services->playSoundLoop(kSoundPaths[m_sound]);
		}
		ImGui::Text("Playing %u arrivals, envelope %.3f, old results fade out after 250 ms", audio.arrivals, audio.gain);
		ImGui::Text("Steam Audio-method GPU reflections: %s", audio.reverbFailed ? "filter preparation failed" :
			(audio.reverbReady ? "ready" : "off / preparing"));
		ImGui::TextUnformatted("Direct: equal power pan. Reflections: order 0 centred mono, no HRTF");
	}
	ImGui::End();
#endif
}
