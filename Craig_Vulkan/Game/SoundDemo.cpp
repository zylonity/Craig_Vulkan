#define GLM_ENABLE_EXPERIMENTAL
#include "SoundDemo.hpp"
#include <glm/gtx/quaternion.hpp>
#include "Craig/Craig_Scene.hpp"
#include "Craig/Craig_AcousticMaterial.hpp"
#include "Craig/Components/Craig_RigidBody.hpp"
#include "Craig/Craig_Logger.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

namespace {
	//Clicks make separate echoes easy to hear, a steady tone mostly just changes colour
	constexpr std::array<const char*, 4> kSoundNames = {"Impact (button)", "Basketball (drop)", "Clicks (loop)", "Tone (loop)"};
	constexpr std::array<const char*, 4> kSoundPaths = {"data/audio/impact.wav", "data/audio/basketball_hit.wav", "data/audio/clicks_loop.wav", "data/audio/door_loop.wav"};
	constexpr std::array<bool, 4> kSoundIsOneShot = {true, true, false, false};
	//Where they are in SoundPropagationReference.json, for working out the expected reflections
	const glm::vec3 kReferenceListener(0.0f, 2.0f, -3.0f);
	const glm::vec3 kReferenceSource(0.0f, 2.0f, 3.0f);
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
	m_time = 0.0f;
	m_referenceCase = ReferenceCase::MovingDoor;
	m_referenceFailed = false;
	prepareSound();
}

void Game::SoundDemo::onPlayStopped() {
	mp_services->disableSoundProbe();
	mp_services->stopSoundLoop();
	m_probeReady = false;
	m_ballDropping = false;
}

void Game::SoundDemo::prepareSound() {
	m_ballDropping = false;
	m_previousBallVelocity = 0.0f;
	// the ball is a point source, the wide test emitter would poke through the floor at each bounce
	const bool basketball = m_sound == 1;
	m_probeReady = mp_services->configureSoundProbe("Listener", basketball ? "Basketball" : "Source", 1,
		basketball ? glm::vec2(0.0f) : glm::vec2(2.0f, 1.0f)) == CRAIG_SUCCESS;
	if (!m_probeReady) return;
	if (kSoundIsOneShot[m_sound]) mp_services->prepareSoundEffect(kSoundPaths[m_sound]);
	else mp_services->playSoundLoop(kSoundPaths[m_sound]);
}

void Game::SoundDemo::dropBasketball() {
	Craig::GameObject* pBall = mp_services->getCurrentScene()->findObject("Basketball");
	if (!pBall) return;
	auto* pBody = pBall->getComponent<Craig::Components::RigidBody>();
	if (!pBody) return;
	glm::vec3 position = pBall->getPosition();
	position.y = 1.52f; // 1.4 m drop, plus the ball's 12 cm radius
	pBall->setPosition(position);
	pBody->resetMotion();
	m_previousBallVelocity = 0.0f;
	m_ballDropping = true;
}

CraigError Game::SoundDemo::selectReferenceCase(ReferenceCase referenceCase) {
	const bool movingDoor = referenceCase == ReferenceCase::MovingDoor;
	const bool hall = referenceCase == ReferenceCase::Hall || referenceCase == ReferenceCase::MediumHall ||
		referenceCase == ReferenceCase::SmallHall || referenceCase == ReferenceCase::SportsHall;
	const char* path = "data/scenes/SoundPropagationReference.json";
	switch (referenceCase) {
		case ReferenceCase::MovingDoor: path = "data/scenes/SoundPropagationDemo.json"; break;
		case ReferenceCase::Hall: path = "data/scenes/SoundPropagationHall.json"; break;
		case ReferenceCase::MediumHall: path = "data/scenes/SoundPropagationMediumHall.json"; break;
		case ReferenceCase::SmallHall: path = "data/scenes/SoundPropagationSmallHall.json"; break;
		case ReferenceCase::SportsHall: path = "data/scenes/SoundPropagationSportsHall.json"; break;
		default: break;
	}
	if (mp_services->loadScene(path) != CRAIG_SUCCESS) return CRAIG_FAIL;
	//The halls are their own scenes, there's no reference wall to move
	if (!movingDoor && !hall) {
		Craig::GameObject* pWall = mp_services->getCurrentScene()->findObject("Reference wall");
		assert(pWall != nullptr && "The reference scene needs its wall");
		if (referenceCase == ReferenceCase::Reflection) {
			//Turned side on and moved off the line, so it can only reflect, never block
			pWall->setScale({0.12f, 3.0f, 6.0f});
			pWall->setPosition({m_reflectorDistance + 0.12f, 2.0f, 0.0f});
		}
		else {
			//BlankBox is [-1,1], wall scale.x is 2, so centred at -2 its right edge is exactly x = 0
			//That covers half of the symmetric emitter, x = 6 moves it completely out of the way
			const float x = referenceCase == ReferenceCase::Clear ? 6.0f :
				(referenceCase == ReferenceCase::Half ? -2.0f : 0.0f);
			pWall->setPosition({x, 2.0f, 0.0f});
		}
	}
	m_referenceCase = referenceCase;
	m_time = 0.0f;
	prepareSound();
	return m_probeReady ? CRAIG_SUCCESS : CRAIG_FAIL;
}

CraigError Game::SoundDemo::update(const float& deltaTime) {
	//The editor still works on a GPU without ray queries
	if (!m_probeReady) return CRAIG_SUCCESS;
	Craig::Scene& scene = *mp_services->getCurrentScene();
	if (m_ballDropping && m_sound == 1) {
		if (const auto* pBall = scene.findObject("Basketball")) {
			if (const auto* pBody = pBall->getComponent<Craig::Components::RigidBody>()) {
				const float velocity = pBody->getLinearVelocity().y;
				// this demo only drops onto a flat floor, a down-to-up change is its impact, not a timer
				if (m_previousBallVelocity < -0.35f && velocity > 0.0f) {
					// softer rebounds get quieter without cutting off the earlier tail
					mp_services->playSoundEffect(std::min(1.0f, -m_previousBallVelocity / std::sqrt(2.0f * 9.81f * 1.4f)));
					Craig::Logger::audio().debug("Basketball hit: {:.2f} m/s down, {:.2f} m/s up", -m_previousBallVelocity, velocity);
				}
				m_previousBallVelocity = velocity;
			}
		}
	}
	if (m_animate && m_referenceCase == ReferenceCase::MovingDoor) {
		m_time += deltaTime;
		if (Craig::GameObject* pDoor = scene.findObject("Door")) {
			pDoor->setPosition({1.3f * (1.0f - std::cos(m_time)), 1.5f, 0.0f});
		}
	}

	//So the slider moves it live
	if (m_referenceCase == ReferenceCase::Reflection) {
		if (Craig::GameObject* pWall = scene.findObject("Reference wall")) pWall->setPosition({m_reflectorDistance + 0.12f, 2.0f, 0.0f});
	}

	//Thin box joining the two markers, it doesn't block sound, just shows where the ray should be
	const auto* pListener = scene.findObject("Listener");
	const auto* pSource = scene.findObject(m_sound == 1 ? "Basketball" : "Source");
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
	//Under the sound propagation window so they don't start on top of each other
	ImGui::SetNextWindowPos(ImVec2(330, 450), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(640, 340), ImGuiCond_FirstUseEver);
	ImGui::Begin("Audio tests");
	const char* cases[] = {"Moving door", "Clear (0%)", "Half (50%)", "Blocked (100%)", "Reflection", "Big hall", "Medium hall", "Small hall", "Sports hall"};
	int selected = static_cast<int>(m_referenceCase);
	if (ImGui::Combo("Scene", &selected, cases, static_cast<int>(std::size(cases)))) {
		m_referenceFailed = selectReferenceCase(static_cast<ReferenceCase>(selected)) != CRAIG_SUCCESS;
	}
	if (m_referenceCase == ReferenceCase::MovingDoor) ImGui::Checkbox("Animate door", &m_animate);
	if (m_referenceFailed) ImGui::TextUnformatted("Setup failed (see log)");
	if (m_referenceCase == ReferenceCase::Reflection) {
		ImGui::SliderFloat("Reflector distance (m)", &m_reflectorDistance, 3.0f, 60.0f);
		//Image sources by hand: mirror the source in the plane, the path length is just the straight line to the mirrored one
		const float speed = Craig::kSpeedOfSound;
		const glm::vec3 wallImage(2.0f * m_reflectorDistance - kReferenceSource.x, kReferenceSource.y, kReferenceSource.z);
		const glm::vec3 floorImage(kReferenceSource.x, -kReferenceSource.y, kReferenceSource.z);
		const std::array<std::pair<const char*, float>, 3> expected = {{
			{"Direct", glm::length(kReferenceSource - kReferenceListener)},
			{"Floor", glm::length(floorImage - kReferenceListener)},
			{"Reflector", glm::length(wallImage - kReferenceListener)}}};
		ImGui::TextUnformatted("Expected delays (original positions)");
		for (const auto& [name, length] : expected) {
			const float excess = (length - expected[0].second) / speed;
			ImGui::Text("%s: %.2f m, %.2f ms (+%.2f ms)", name, length,
				length / speed * 1000.0f, excess * 1000.0f);
		}
		ImGui::TextDisabled("Reflections use 10 ms energy bins");
	}
	else if (m_referenceCase == ReferenceCase::Hall || m_referenceCase == ReferenceCase::MediumHall ||
		m_referenceCase == ReferenceCase::SmallHall) {
		const bool big = m_referenceCase == ReferenceCase::Hall;
		const bool medium = m_referenceCase == ReferenceCase::MediumHall;
		ImGui::Text("%s m, sports-hall materials", big ? "30 x 20 x 8" : (medium ? "15 x 10 x 4" : "8 x 6 x 3"));
		ImGui::SetItemTooltip("Wood floor, brick lower walls, plaster upper side walls.\nCarpet approximates the absorbent roof and upper end panels.");
	}
	else if (m_referenceCase == ReferenceCase::SportsHall) {
		ImGui::TextUnformatted("34 x 18 x 9 m, mixed materials");
		ImGui::TextUnformatted("Fitzroy RT60: ~2.5 / 1.8 / 1.8 s");
		ImGui::SetItemTooltip("Low/mid/high, no air or end mats; estimate, not measured.\nCarpet approximates the absorbent roof, panels and mats.");
	}
	else if (m_referenceCase != ReferenceCase::MovingDoor) {
		const float expected = m_referenceCase == ReferenceCase::Clear ? 0.0f :
			(m_referenceCase == ReferenceCase::Half ? 0.5f : 1.0f);
		ImGui::Text("Expected obstruction: %.0f%%", expected * 100.0f);
		ImGui::SetItemTooltip("Original geometry. Half needs multiple rays and non-zero emitter width.\nFinite samples vary with the seed.");
		const auto probe = mp_services->getSoundProbeReadout();
		if (probe.hasResult) ImGui::Text("GPU: %.4f, difference: %.4f", probe.obstruction, std::abs(probe.obstruction - expected));
	}
	ImGui::Separator();
	ImGui::TextDisabled("Start with Play in the engine toolbar");
	const auto audio = mp_services->getAudioStatus();
	ImGui::Text("Audio: %s", Craig::Audio::stateName(audio.state));
	if (kSoundIsOneShot[m_sound]) {
		const auto probe = mp_services->getSoundProbeReadout();
		const bool ready = m_probeReady && audio.available && !audio.muted && probe.hasResult &&
			probe.state == Craig::SoundProbeReadout::State::Ready && probe.ageMs >= 0.0 &&
			probe.ageMs < Craig::Audio::kMaxResultAgeMs &&
			(!probe.reverbEnabled || !audio.reflectionsEnabled || audio.reverbReady);
		ImGui::BeginDisabled(!ready);
		if (ImGui::Button(m_sound == 1 ? "Drop basketball" : "Play sound")) {
			if (m_sound == 1) dropBasketball();
			else mp_services->playSoundEffect();
		}
		ImGui::EndDisabled();
		ImGui::SetItemTooltip(m_sound == 1 ? "Drop from 1.4 m; each rebound plays one impact" : "Adds a sound without cutting off earlier ones");
		if (!ready) ImGui::TextDisabled("Waiting for Play, unmuted audio and fresh paths");
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
		if (ImGui::Combo("Sound", &m_sound, kSoundNames.data(), static_cast<int>(kSoundNames.size()))) prepareSound();
		ImGui::Text("Reflections: %s", audio.reverbFailed ? "filter failed" :
			(audio.reverbReady ? "ready" : "off / preparing"));
		if (ImGui::CollapsingHeader("Audio diagnostics")) {
			ImGui::Text("Arrivals %u | Gain %.3f", audio.arrivals, audio.gain);
			ImGui::Text("Callback: %.2f / %.2f ms (%u frames @ %d Hz)", audio.callbackMs, audio.callbackBudgetMs,
				audio.callbackBufferFrames, audio.callbackRate);
			ImGui::Text("Peak %.1f%% | Over budget %llu | Gaps %llu (max %.2f ms)", 100.0f * audio.callbackPeakLoad,
				static_cast<unsigned long long>(audio.callbackOverBudget), static_cast<unsigned long long>(audio.callbackLongGaps), audio.callbackMaxGapMs);
			ImGui::SetItemTooltip("Since sound opened; not actual underrun counts");
			ImGui::Text("Tail banks %u | Filter starts %llu | Resets %llu", audio.reverbBanks,
				static_cast<unsigned long long>(audio.reverbChanges), static_cast<unsigned long long>(audio.reverbInvalidations));
			ImGui::TextDisabled("Direct: equal-power pan. Reflections: mono, no HRTF.");
		}
	}
	ImGui::End();
#endif
}
