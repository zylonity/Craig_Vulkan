#include "Craig/Craig_Renderer.hpp"
#include "Craig/Craig_SceneManager.hpp"
#include "Craig/Craig_Logger.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
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

void Craig::Renderer::resetSoundProbeContext(bool clearDebugRays) {
    if (clearDebugRays) {
        mv_soundDebugRays.clear();
        mv_soundDebugPaths.clear();
    }
    if (m_soundInvalidated) m_soundInvalidated();
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
    if (mode != m_soundMode || running != m_soundRunning) resetSoundProbeContext(false);
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
        readout.seed = m_soundResult->seed;
        readout.obstruction = static_cast<float>(readout.blockedRays) / readout.rayCount;
        readout.sceneGeneration = m_soundResult->sceneGeneration;
        readout.sourceId = m_soundResult->sourceId;
        readout.sequenceId = m_soundResult->sequenceId;
        readout.arrivals = m_soundResult->arrivals;
        readout.arrivalCount = m_soundResult->arrivalCount;
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
        //Only log when it becomes fully clear/blocked, not every small sample change
        if (!m_soundResult || (result.blocked == 0) != (m_soundResult->blocked == 0) ||
            (result.blocked == result.rayCount) != (m_soundResult->blocked == m_soundResult->rayCount)) {
            Logger::renderer().info("Live sound probe: {}/{} rays blocked, {} arrivals (scene {}, source {}, sequence {}, input age {:.2f} ms)",
                result.blocked, result.rayCount, result.arrivalCount, result.sceneGeneration, result.sourceId, result.sequenceId, result.ageMs);
        }
        m_soundResult = result;
        //Copied so pausing keeps them on screen
        mv_soundDebugPaths.assign(result.arrivals.begin(), result.arrivals.begin() + result.arrivalCount);
        m_soundDebugListener = origin;
        m_soundDebugSource = endpoint;
    }

    SoundPropagation::Request request{};
    request.rayCount = static_cast<uint32_t>(m_soundRayCount);
    request.seed = static_cast<uint32_t>(m_soundSeed);
    //Same seed each time so a stationary wall doesn't get a different sample pattern every frame
    std::mt19937 generator(request.seed);
    for (uint32_t i = 0; i < request.rayCount; ++i) {
        glm::vec3 sample = endpoint;
        if (request.rayCount > 1) {
            //Uniform rectangle across world X/Y, one ray still aims at the centre
            const float u = static_cast<float>(generator() >> 8) / 16777216.0f;
            const float v = static_cast<float>(generator() >> 8) / 16777216.0f;
            sample += glm::vec3((u - 0.5f) * m_soundEmitterSize[0], (v - 0.5f) * m_soundEmitterSize[1], 0.0f);
        }
        //From the source to the listener, the way the sound goes
        const glm::vec3 raySegment = origin - sample;
        const float rayDistance = glm::length(raySegment);
        if (!std::isfinite(rayDistance) || rayDistance <= 0.002f) {
            resetSoundProbeContext();
            m_soundState = SoundProbeReadout::State::InvalidSegment;
            return;
        }
        const glm::vec3 direction = raySegment / rayDistance;
        request.rays[i] = {{sample.x, sample.y, sample.z, 0.001f},
            {direction.x, direction.y, direction.z, rayDistance - 0.001f}};
    }
    request.sceneGeneration = m_soundSceneGeneration;
    request.sourceId = m_soundSourceId;
    request.sequenceId = m_soundSequence++;
    request.inputTime = inputTime;
    request.listener = {origin.x, origin.y, origin.z};
    request.source = {endpoint.x, endpoint.y, endpoint.z};
    //The listener object's +X is its right ear, not the editor camera's
    const glm::vec3 right = glm::vec3(pListener->GetModelMatrix()[0]);
    const float rightLength = glm::length(right);
    const glm::vec3 ear = std::isfinite(rightLength) && rightLength > 0.0f ? right / rightLength : glm::vec3(1.0f, 0.0f, 0.0f);
    request.listenerRight = {ear.x, ear.y, ear.z};

    const auto status = m_soundPropagation.trySubmit(scene, request);
#if defined(IMGUI_ENABLED)
    //Only show rays we actually submitted, not what we would have submitted if a slot was free
    if (m_showSoundRays && status == SoundPropagation::SubmitStatus::Submitted) {
        mv_soundDebugRays.assign(request.rays.begin(), request.rays.begin() + request.rayCount);
        m_soundDebugSequence = request.sequenceId;
    }
#endif
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
    ImGui::Text("%s -> %s", m_soundSource.c_str(), m_soundListener.c_str());
    bool changed = ImGui::SliderInt("Direct rays", &m_soundRayCount, 1, SoundPropagation::kMaxRayCount);
    changed |= ImGui::InputInt("Seed", &m_soundSeed);
    changed |= ImGui::SliderFloat2("Emitter width / height (m)", m_soundEmitterSize, 0.0f, 4.0f);
    m_soundRayCount = std::clamp(m_soundRayCount, 1, static_cast<int>(SoundPropagation::kMaxRayCount));
    for (float& size : m_soundEmitterSize) size = std::isfinite(size) ? std::clamp(size, 0.0f, 4.0f) : 0.0f;
    if (changed) resetSoundProbeContext();
    ImGui::Checkbox("Show sound paths", &m_showSoundPaths);
    ImGui::SameLine();
    if (ImGui::Checkbox("Show direct samples", &m_showSoundRays) && !m_showSoundRays) mv_soundDebugRays.clear();
    if (m_showSoundRays) {
        ImGui::TextUnformatted("Neutral lines through walls, not hit/miss colours");
        ImGui::Text("Last submitted: %zu rays, sequence %llu (frozen on pause)",
            mv_soundDebugRays.size(), static_cast<unsigned long long>(m_soundDebugSequence));
    }
    ImGui::TextUnformatted("Uniform world-XY rectangle, 1 ray uses its centre");
    if (readout.hasResult && !changed) {
        ImGui::Text("Obstruction: %.3f (%u / %u blocked), seed %u", readout.obstruction, readout.blockedRays, readout.rayCount, readout.seed);
        ImGui::Text("Input age: %.2f ms, sequence %llu", readout.ageMs, static_cast<unsigned long long>(readout.sequenceId));
        if (ImGui::BeginTable("Arrivals", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY, ImVec2(0, 160))) {
            ImGui::TableSetupColumn("Order");
            ImGui::TableSetupColumn("Length (m)");
            ImGui::TableSetupColumn("Delay (ms)");
            ImGui::TableSetupColumn("Low / mid / high gain");
            ImGui::TableSetupColumn("Pan");
            ImGui::TableSetupColumn("Level (dB)");
            ImGui::TableHeadersRow();
            for (uint32_t i = 0; i < readout.arrivalCount; ++i) {
                const SoundArrival& arrival = readout.arrivals[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%u", arrival.order);
                ImGui::TableNextColumn(); ImGui::Text("%.3f", arrival.length);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", arrival.delaySeconds * 1000.0f);
                ImGui::TableNextColumn(); ImGui::Text("%.3f / %.3f / %.3f", arrival.gains[0], arrival.gains[1], arrival.gains[2]);
                ImGui::TableNextColumn(); ImGui::Text("%+.2f", arrival.pan);
                //Average of the three bands, just to have one number to look at
                const float level = (arrival.gains[0] + arrival.gains[1] + arrival.gains[2]) / 3.0f;
                ImGui::TableNextColumn(); ImGui::Text("%.1f", 20.0f * std::log10(std::max(level, 1e-6f)));
            }
            ImGui::EndTable();
        }
    }
    const auto& stats = m_soundPropagation.getStatistics();
    ImGui::Text("Submitted %llu, completed %llu, skipped %llu, discarded %llu",
        static_cast<unsigned long long>(stats.submitted), static_cast<unsigned long long>(stats.completed),
        static_cast<unsigned long long>(stats.skipped), static_cast<unsigned long long>(stats.discarded));
    ImGui::End();
    drawSoundProbeRays();
#endif
}

void Craig::Renderer::drawSoundProbeRays() {
#if defined(IMGUI_ENABLED)
    if ((!m_showSoundRays || mv_soundDebugRays.empty()) && (!m_showSoundPaths || mv_soundDebugPaths.empty())) return;
    const Camera& camera = mp_SceneManager->getCurrentScene()->getCamera();
    const glm::mat4 viewProj = camera.getProj() * camera.getView();
    const ImGuiViewport* pViewport = ImGui::GetMainViewport();
    ImDrawList* pDrawList = ImGui::GetBackgroundDrawList();

    //getProj already flips Y for Vulkan, no second flip here
    const auto screenPoint = [pViewport](const glm::vec4& clip) {
        const glm::vec2 ndc = glm::vec2(clip) / clip.w;
        return ImVec2(pViewport->Pos.x + (ndc.x * 0.5f + 0.5f) * pViewport->Size.x,
            pViewport->Pos.y + (ndc.y * 0.5f + 0.5f) * pViewport->Size.y);
    };

    //Clip against the six planes before dividing by w, otherwise a ray behind the camera flips across the screen
    const std::array<glm::vec4, 6> planes = {{
        {1, 0, 0, 1}, {-1, 0, 0, 1}, {0, 1, 0, 1}, {0, -1, 0, 1}, {0, 0, 1, 0}, {0, 0, -1, 1}
    }};
    const auto drawSegment = [&](const glm::vec3& from, const glm::vec3& to, ImU32 colour) {
        glm::vec4 start = viewProj * glm::vec4(from, 1.0f);
        glm::vec4 end = viewProj * glm::vec4(to, 1.0f);
        for (const auto& plane : planes) {
            const float a = glm::dot(plane, start);
            const float b = glm::dot(plane, end);
            if (a < 0.0f && b < 0.0f) return;
            if (a < 0.0f) start = glm::mix(start, end, a / (a - b));
            else if (b < 0.0f) end = glm::mix(start, end, a / (a - b));
        }
        if (start.w > 0.0f && end.w > 0.0f) pDrawList->AddLine(screenPoint(start), screenPoint(end), colour, 1.5f);
    };

    if (m_showSoundRays) {
        for (const auto& ray : mv_soundDebugRays) {
            const glm::vec3 origin(ray.origin[0], ray.origin[1], ray.origin[2]);
            const glm::vec3 direction(ray.direction[0], ray.direction[1], ray.direction[2]);
            drawSegment(origin + direction * ray.origin[3], origin + direction * ray.direction[3], IM_COL32(255, 220, 100, 90));
        }
    }
    if (m_showSoundPaths) {
        //White line for the direct path from the last result
        for (const SoundArrival& arrival : mv_soundDebugPaths) {
            if (arrival.order == 0) drawSegment(m_soundDebugListener, m_soundDebugSource, IM_COL32(255, 255, 255, 200));
        }
    }
#endif
}
