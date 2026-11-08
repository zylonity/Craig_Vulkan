#define GLM_ENABLE_EXPERIMENTAL
#include "SoundDemo.hpp"
#include <glm/gtx/quaternion.hpp>
#include "Craig/Craig_Scene.hpp"
#include "Craig/Craig_AcousticMaterial.hpp"

#include <array>
#include <cassert>
#include <cmath>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

namespace {
	//Clicks make separate echoes easy to hear, a steady tone mostly just changes colour
	constexpr std::array<const char*, 4> kSoundNames = {"Impact (button)", "Basketball (button)", "Clicks (loop)", "Tone (loop)"};
	constexpr std::array<const char*, 4> kSoundPaths = {"data/audio/impact.wav", "data/audio/basketball.wav", "data/audio/clicks_loop.wav", "data/audio/door_loop.wav"};
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
	const bool hall = referenceCase == ReferenceCase::Hall || referenceCase == ReferenceCase::SportsHall;
	const char* path = movingDoor ? "data/scenes/SoundPropagationDemo.json" :
		(referenceCase == ReferenceCase::Hall ? "data/scenes/SoundPropagationHall.json" :
		(referenceCase == ReferenceCase::SportsHall ? "data/scenes/SoundPropagationSportsHall.json" : "data/scenes/SoundPropagationReference.json"));
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

	//So the slider moves it live
	if (m_referenceCase == ReferenceCase::Reflection) {
		if (Craig::GameObject* pWall = scene.findObject("Reference wall")) pWall->setPosition({m_reflectorDistance + 0.12f, 2.0f, 0.0f});
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
	//Under the sound propagation window so they don't start on top of each other
	ImGui::SetNextWindowPos(ImVec2(330, 450), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(640, 340), ImGuiCond_FirstUseEver);
	ImGui::Begin("Moving-door demo");
	ImGui::TextUnformatted("Small cubes: listener/source, thin line: the segment between them");
	if (m_referenceCase == ReferenceCase::MovingDoor) ImGui::Checkbox("Animate door", &m_animate);
	ImGui::SeparatorText("Geometric references");
	const char* cases[] = {"Moving door", "Clear (0%)", "Half (50%)", "Blocked (100%)", "Reflection", "Big hall", "Sports hall"};
	for (int i = 0; i < 7; ++i) {
		if (i != 0) ImGui::SameLine();
		if (ImGui::Button(cases[i])) m_referenceFailed = selectReferenceCase(static_cast<ReferenceCase>(i)) != CRAIG_SUCCESS;
	}
	if (m_referenceFailed) ImGui::TextUnformatted("Scene/probe setup failed, see the log");
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
		ImGui::TextUnformatted("Expected from image sources (if the listener/source haven't been moved)");
		for (const auto& [name, length] : expected) {
			const float excess = (length - expected[0].second) / speed;
			ImGui::Text("%s: %.3f m, %.2f ms, %.2f ms after the direct sound", name, length,
				length / speed * 1000.0f, excess * 1000.0f);
		}
		ImGui::TextUnformatted("The GPU gives 10 ms energy bins, so look for energy in the bin that delay lands in");
	}
	else if (m_referenceCase == ReferenceCase::Hall) {
		ImGui::TextUnformatted("30 x 20 x 8 m concrete hall, source at the far end, listener at this end");
		ImGui::TextUnformatted("Sabine says RT60 is about 5.5 s mid band, so it should still be going at the end of the 2 s window");
		ImGui::TextUnformatted("'Show bouncing rays' shows where the listener rays go, Reflection rays sets how many");
	}
	else if (m_referenceCase == ReferenceCase::SportsHall) {
		ImGui::TextUnformatted("34 x 18 x 9 m: wood floor/bleachers, brick low walls, plaster high end walls, glass backboards");
		//Steam Audio doesn't have an acoustic panel preset, carpet's the closest
		ImGui::TextUnformatted("Carpet stands in for the absorbent roof, long wall panels and end wall mats");
		//Fitzroy instead of Sabine since nearly all the absorption is on the roof, the bounces between the walls last longer
		ImGui::TextUnformatted("Fitzroy estimate without mats ~2.5 / 1.8 / 1.8 s low/mid/high (no air), not a measured hall");
		ImGui::TextUnformatted("Try it with the basketball");
	}
	else if (m_referenceCase != ReferenceCase::MovingDoor) {
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
		ImGui::Text("Callback: %.3f / %.3f ms (%u frames @ %d Hz)", audio.callbackMs, audio.callbackBudgetMs,
			audio.callbackBufferFrames, audio.callbackRate);
		ImGui::Text("Peak load %.1f%%; over budget %llu; long gaps %llu (max %.2f ms)", 100.0f * audio.callbackPeakLoad,
			static_cast<unsigned long long>(audio.callbackOverBudget), static_cast<unsigned long long>(audio.callbackLongGaps), audio.callbackMaxGapMs);
		ImGui::Text("Tail: %u banks active, %llu filter starts, %llu resets requested", audio.reverbBanks,
			static_cast<unsigned long long>(audio.reverbChanges), static_cast<unsigned long long>(audio.reverbInvalidations));
		ImGui::TextDisabled("Counted since the sound was opened, these aren't real underrun counts");
		ImGui::TextUnformatted("Reflections switches off the whole GPU reflected IR, early part included");
		ImGui::TextUnformatted("Direct: equal power pan. Reflections: order 0 centred mono, no HRTF");
	}
	ImGui::End();
#endif
}
