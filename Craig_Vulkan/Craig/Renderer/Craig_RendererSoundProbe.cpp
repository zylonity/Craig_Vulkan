#include "Craig/Craig_Renderer.hpp"
#include "Craig/Craig_SceneManager.hpp"
#include "Craig/Craig_Logger.hpp"

#include <cmath>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

CraigError Craig::Renderer::configureSoundProbe(const std::string& listener, const std::string& source, uint64_t sourceId) {
    resetSoundProbeContext();
    m_soundListener = listener;
    m_soundSource = source;
    m_soundSourceId = sourceId;
    m_soundConfigured = true;
    if (!m_soundPrepared && prepareSoundProbe() != CRAIG_SUCCESS) {
        disableSoundProbe();
        return CRAIG_FAIL;
    }
    m_soundState = SoundProbeReadout::State::Waiting;
    return CRAIG_SUCCESS;
}

CraigError Craig::Renderer::prepareSoundProbe() {
    //Only called when configuring or changing scenes, builds can wait for the GPU here but not every frame
    m_soundPropagation.terminate();
    m_soundPrepared = false;
    try {
        if (m_soundPropagation.init(&m_Devices, &m_commandManager, mp_SceneManager->getCurrentScene()) == CRAIG_SUCCESS) {
            m_soundPrepared = true;
            return CRAIG_SUCCESS;
        }
    }
    catch (const std::exception& error) {
        Logger::renderer().error("Sound probe preparation failed: {}", error.what());
    }
    m_soundPropagation.terminate();
    return CRAIG_FAIL;
}

void Craig::Renderer::resetSoundProbeContext() {
    m_soundPropagation.stop();
    m_soundContextActive = false;
    m_soundResult.reset();
    m_soundSequence = 0;
    m_soundListenerInstance = 0;
    m_soundSourceInstance = 0;
    m_soundState = m_soundConfigured ? SoundProbeReadout::State::Waiting : SoundProbeReadout::State::Disabled;
}

void Craig::Renderer::disableSoundProbe() {
    resetSoundProbeContext();
    m_soundConfigured = false;
    m_soundState = SoundProbeReadout::State::Disabled;
}

void Craig::Renderer::setSoundProbeMode(uint32_t mode, bool running) {
    //Even if we're resuming the same scene, results from before the pause aren't current anymore
    if (mode != m_soundMode || running != m_soundRunning) resetSoundProbeContext();
    m_soundMode = mode;
    m_soundRunning = running;
    if (m_soundConfigured && !running) m_soundState = SoundProbeReadout::State::Paused;
}

Craig::SoundProbeReadout Craig::Renderer::getSoundProbeReadout() const {
    SoundProbeReadout readout;
    readout.state = m_soundState;
    readout.sceneGeneration = m_soundSceneGeneration;
    readout.sourceId = m_soundSourceId;
    if (m_soundResult) {
        readout.hasResult = true;
        readout.blockedRays = m_soundResult->blocked;
        readout.rayCount = m_soundResult->rayCount;
        readout.obstruction = static_cast<float>(readout.blockedRays) / readout.rayCount;
        readout.sceneGeneration = m_soundResult->sceneGeneration;
        readout.sourceId = m_soundResult->sourceId;
        readout.sequenceId = m_soundResult->sequenceId;
        //Always work this out again, leaving a result on screen for a second doesn't make it a fresh one
        readout.ageMs = std::chrono::duration<double, std::milli>(SoundPropagation::Clock::now() - m_soundResult->inputTime).count();
    }
    return readout;
}

void Craig::Renderer::updateSoundProbe() {
    if (!m_soundConfigured || !m_soundPrepared) return;
    if (!m_soundRunning) {
        m_soundState = SoundProbeReadout::State::Paused;
        return;
    }

    const auto previousState = m_soundState;
    Scene& scene = *mp_SceneManager->getCurrentScene();
    GameObject* pListener = scene.findObject(m_soundListener);
    GameObject* pSource = scene.findObject(m_soundSource);
    if (!pListener || !pSource) {
        resetSoundProbeContext();
        m_soundState = SoundProbeReadout::State::MissingEndpoint;
        return;
    }

    const auto inputTime = SoundPropagation::Clock::now();
    //ImGui can move things after Scene::update, refresh the matrices before tracing AND drawing them
    for (GameObject* pObject : scene.getGameObjects()) pObject->updateModelMatrix();
    const glm::vec3 origin(pListener->GetModelMatrix()[3]);
    const glm::vec3 endpoint(pSource->GetModelMatrix()[3]);
    const glm::vec3 segment = endpoint - origin;
    const float distance = glm::length(segment);
    if (!std::isfinite(distance) || distance <= 0.002f) {
        resetSoundProbeContext();
        m_soundState = SoundProbeReadout::State::InvalidSegment;
        return;
    }

    if (!m_soundContextActive || pListener->getInstanceId() != m_soundListenerInstance || pSource->getInstanceId() != m_soundSourceInstance) {
        resetSoundProbeContext();
        m_soundPropagation.setContext(m_soundSceneGeneration, m_soundSourceId);
        m_soundListenerInstance = pListener->getInstanceId();
        m_soundSourceInstance = pSource->getInstanceId();
        m_soundContextActive = true;
    }

    SoundPropagation::Result result;
    if (m_soundPropagation.poll(result)) {
        if (!m_soundResult || result.blocked != m_soundResult->blocked) {
            Logger::renderer().info("Live sound probe: {}/{} rays blocked (scene {}, source {}, sequence {}, input age {:.2f} ms)",
                result.blocked, result.rayCount, result.sceneGeneration, result.sourceId, result.sequenceId, result.ageMs);
        }
        m_soundResult = result;
    }

    SoundPropagation::Request request{};
    request.rayCount = 1;
    const glm::vec3 direction = segment / distance;
    request.rays[0] = {{origin.x, origin.y, origin.z, 0.001f},
        {direction.x, direction.y, direction.z, distance - 0.001f}};
    request.sceneGeneration = m_soundSceneGeneration;
    request.sourceId = m_soundSourceId;
    request.sequenceId = m_soundSequence++;
    request.inputTime = inputTime;

    const auto status = m_soundPropagation.trySubmit(scene, request);
    if (status == SoundPropagation::SubmitStatus::GeometryNotReady) {
        if (previousState != SoundProbeReadout::State::GeometryNotReady) {
            Logger::renderer().warn("Sound probe geometry is outside its prepared model set/capacity; reload the scene to prepare it");
        }
        resetSoundProbeContext();
        m_soundState = SoundProbeReadout::State::GeometryNotReady;
        return;
    }
    m_soundState = m_soundResult ? SoundProbeReadout::State::Ready : SoundProbeReadout::State::Waiting;
}

void Craig::Renderer::drawSoundProbeReadout() {
#if defined(IMGUI_ENABLED)
    if (!m_soundConfigured) return;
    const auto readout = getSoundProbeReadout();
    const char* states[] = {"disabled", "paused", "waiting", "ready", "missing endpoint", "invalid segment", "geometry not prepared"};
    ImGui::Begin("Sound propagation");
    ImGui::Text("State: %s", states[static_cast<unsigned>(readout.state)]);
    ImGui::Text("%s -> %s", m_soundListener.c_str(), m_soundSource.c_str());
    if (readout.hasResult) {
        ImGui::Text("Blocked: %u / %u", readout.blockedRays, readout.rayCount);
        ImGui::Text("Input age: %.2f ms, sequence %llu", readout.ageMs, static_cast<unsigned long long>(readout.sequenceId));
    }
    const auto& stats = m_soundPropagation.getStatistics();
    ImGui::Text("Submitted %llu, completed %llu, skipped %llu, discarded %llu",
        static_cast<unsigned long long>(stats.submitted), static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.skipped), static_cast<unsigned long long>(stats.discarded));
    ImGui::End();
#endif
}
