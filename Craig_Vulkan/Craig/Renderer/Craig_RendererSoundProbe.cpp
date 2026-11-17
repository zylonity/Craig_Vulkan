#include "Craig/Craig_Renderer.hpp"
#include "Craig/Craig_SceneManager.hpp"
#include "Craig/Craig_Logger.hpp"
#include "Craig_SteamAudioSampling.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#if defined(IMGUI_ENABLED)
#include "imgui.h"
#endif

namespace {
    //The listener object's +X is its right ear and -Z is ahead, not the editor camera's
    //Steam Audio wants the same right handed, -Z forward convention, so no flips
    Craig::ListenerFrame makeListenerFrame(Craig::GameObject& listener) {
        const glm::mat4 model = listener.GetModelMatrix();
        const auto unit = [](glm::vec3 axis, glm::vec3 fallback) {
            const float length = glm::length(axis);
            return std::isfinite(length) && length > 0.0f ? axis / length : fallback;
        };
        const glm::vec3 right = unit(glm::vec3(model[0]), {1.0f, 0.0f, 0.0f});
        const glm::vec3 up = unit(glm::vec3(model[1]), {0.0f, 1.0f, 0.0f});
        const glm::vec3 ahead = -unit(glm::vec3(model[2]), {0.0f, 0.0f, 1.0f});
        Craig::ListenerFrame frame;
        frame.origin = {model[3].x, model[3].y, model[3].z};
        frame.right = {right.x, right.y, right.z};
        frame.up = {up.x, up.y, up.z};
        frame.ahead = {ahead.x, ahead.y, ahead.z};
        return frame;
    }
}

CraigError Craig::Renderer::configureSoundProbe(const std::string& listener, const std::string& source, uint64_t sourceId, glm::vec2 emitterSize) {
    resetSoundProbeContext();
    for (int i = 0; i < 2; ++i) {
        m_soundEmitterSize[i] = std::isfinite(emitterSize[i]) ? std::clamp(emitterSize[i], 0.0f, 4.0f) : 0.0f;
    }
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
    m_gpuSoundReady = false;
    try {
        m_gpuSoundReady = m_soundPropagation.init(&m_Devices, &m_commandManager, mp_SceneManager->getCurrentScene()) == CRAIG_SUCCESS;
    }
    catch (const std::exception& error) {
        Logger::renderer().error("GPU sound propagation preparation failed: {}", error.what());
    }
    if (!m_gpuSoundReady) m_soundPropagation.terminate();

    //Doesn't need ray queries, so a GPU without them still gets sound through this one
    m_cpuSoundReady = m_steamSimulator.prepare(*mp_SceneManager->getCurrentScene(), steamSimulatorSettings()) == CRAIG_SUCCESS;
    m_soundPrepared = m_gpuSoundReady || m_cpuSoundReady;
    if (!m_gpuSoundReady && m_cpuSoundReady && m_soundBackend == SoundBackend::GPU) {
        Logger::renderer().warn("No ray queries for sound, using Steam Audio's CPU simulator instead");
        m_soundBackend = SoundBackend::CPU;
    }
    return m_soundPrepared ? CRAIG_SUCCESS : CRAIG_FAIL;
}

Craig::SteamAudioSimulator::Settings Craig::Renderer::steamSimulatorSettings() const {
    SteamAudioSimulator::Settings settings;
    settings.rayTracer = m_steamEmbree ? SteamAudioSimulator::RayTracer::Embree : SteamAudioSimulator::RayTracer::Default;
    settings.threads = m_steamThreads;
    return settings;
}

void Craig::Renderer::resetSoundProbeContext(bool clearDebugRays) {
    if (clearDebugRays) {
        mv_soundDebugRays.clear();
        m_soundDebugHasPath = false;
        mv_soundDebugBounces.clear();
    }
    if (m_soundInvalidated) m_soundInvalidated();
    m_soundPropagation.stop();
    m_steamSimulator.stop();
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
    if (!m_soundResult) {
        SoundProbeReadout readout;
        readout.state = m_soundState;
        readout.sceneGeneration = m_soundSceneGeneration;
        readout.sourceId = m_soundSourceId;
        readout.backend = m_soundBackend;
        return readout;
    }
    SoundProbeReadout readout = *m_soundResult;
    readout.state = m_soundState;
    //Always work this out again, leaving a result on screen for a second doesn't make it a fresh one
    readout.ageMs = std::chrono::duration<double, std::milli>(SoundPropagation::Clock::now() - readout.inputTime).count();
    return readout;
}

void Craig::Renderer::acceptSoundResult(const SoundProbeReadout& result) {
    //Only log when it becomes fully clear/blocked, not every small sample change
    if (!m_soundResult || (result.obstruction == 0.0f) != (m_soundResult->obstruction == 0.0f) ||
        (result.obstruction == 1.0f) != (m_soundResult->obstruction == 1.0f)) {
        const SoundTimings& t = result.timings;
        Logger::renderer().info("Live sound probe ({}): {:.1f}% obstructed (scene {}, source {}, sequence {}, input age {:.2f} ms), {}",
            result.backend == SoundBackend::GPU ? "GPU" : "Steam Audio CPU", 100.0f * result.obstruction, result.sceneGeneration,
            result.sourceId, result.sequenceId, std::chrono::duration<double, std::milli>(SoundPropagation::Clock::now() - result.inputTime).count(),
            result.backend == SoundBackend::GPU ?
                fmt::format("TLAS {:.3f} ms, rays {:.3f} ms, readback {:.3f} ms", t.gpuAccelerationStructureMs, t.gpuTraceMs, t.readbackMs) :
                fmt::format("scene {:.3f} ms, direct {:.3f} ms, reflections {:.3f} ms", t.steamSceneMs, t.steamDirectMs, t.steamReflectionsMs));
    }
    m_soundResult = result;
    m_soundResult->hasResult = true;
    //Kept so pausing leaves the line on screen
    m_soundDebugHasPath = true;
    m_soundDebugListener = glm::vec3(result.listener.origin[0], result.listener.origin[1], result.listener.origin[2]);
    m_soundDebugSource = glm::vec3(result.source[0], result.source[1], result.source[2]);
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

    const bool gpu = m_soundBackend == SoundBackend::GPU;
    if (gpu ? !m_gpuSoundReady : !m_cpuSoundReady) {
        if (previousState != SoundProbeReadout::State::BackendUnavailable) {
            Logger::renderer().warn("{} sound propagation isn't available, pick the other one", gpu ? "GPU" : "CPU");
        }
        resetSoundProbeContext();
        m_soundState = SoundProbeReadout::State::BackendUnavailable;
        return;
    }

    if (!m_soundContextActive || pListener->getInstanceId() != m_soundListenerInstance || pSource->getInstanceId() != m_soundSourceInstance) {
        resetSoundProbeContext();
        if (gpu) m_soundPropagation.setContext(m_soundSceneGeneration, m_soundSourceId);
        else m_steamSimulator.setContext(m_soundSceneGeneration, m_soundSourceId);
        m_soundListenerInstance = pListener->getInstanceId();
        m_soundSourceInstance = pSource->getInstanceId();
        m_soundContextActive = true;
    }

    if (!gpu) {
        updateSteamSimulator(scene, *pListener, origin, endpoint, inputTime, previousState);
        return;
    }

    SoundPropagation::Result result;
    if (m_soundPropagation.poll(result)) {
        SoundProbeReadout readout;
        readout.backend = SoundBackend::GPU;
        readout.blockedRays = result.blocked;
        readout.rayCount = result.rayCount;
        readout.seed = result.seed;
        //Works for both direct models, the volumetric one only counts samples the source can actually see
        readout.obstruction = 1.0f - result.direct.occlusion;
        readout.sceneGeneration = result.sceneGeneration;
        readout.sourceId = result.sourceId;
        readout.sequenceId = result.sequenceId;
        readout.source = result.source;
        readout.listener = result.listener;
        readout.direct = result.direct;
        readout.reflections = result.reflections;
        readout.reflectionOrder = result.reflections ? result.reflections->order : 0;
        readout.reverbEnabled = result.reverbEnabled;
        readout.timings = result.timings;
        readout.inputTime = result.inputTime;
        acceptSoundResult(readout);
        //Only the overlay keeps the bounce points, the readout doesn't need another copy
        mv_soundDebugBounces = std::move(result.bounces);
    }

    SoundPropagation::Request request{};
    request.seed = static_cast<uint32_t>(m_soundSeed);
    request.volumetric = m_soundVolumetric;
    request.transmissionRays = static_cast<uint32_t>(m_soundTransmissionRays);
    //A segment from a to b, written the way the shader reads it
    const auto makeRay = [](const glm::vec3& from, const glm::vec3& to) {
        const glm::vec3 offset = to - from;
        const float length = glm::length(offset);
        //Sample 0 of the sphere is the centre itself, there's nothing to trace, so an empty ray that can't hit anything
        if (!std::isfinite(length) || length <= 0.002f) return SoundPropagation::Ray{{from.x, from.y, from.z, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}};
        const glm::vec3 direction = offset / length;
        return SoundPropagation::Ray{{from.x, from.y, from.z, 0.001f}, {direction.x, direction.y, direction.z, length - 0.001f}};
    };
    if (request.volumetric) {
        //Steam Audio's volumetric occlusion: points inside a sphere round the source, each one checked from the source then the listener
        const uint32_t samples = std::min(static_cast<uint32_t>(m_soundRayCount), SoundPropagation::kMaxRayCount / 2);
        if (mv_soundVolumeSamples.size() != samples) {
            mv_soundVolumeSamples.resize(samples);
            fillSteamAudioSphereVolumeSamples(mv_soundVolumeSamples);
        }
        for (uint32_t i = 0; i < samples; ++i) {
            const auto& unit = mv_soundVolumeSamples[i];
            const glm::vec3 sample = endpoint + m_soundOcclusionRadius * glm::vec3(unit[0], unit[1], unit[2]);
            request.rays[2 * i] = makeRay(endpoint, sample);
            request.rays[2 * i + 1] = makeRay(origin, sample);
        }
        request.rayCount = 2 * samples;
    }
    else {
        request.rayCount = static_cast<uint32_t>(m_soundRayCount);
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
            if (glm::length(origin - sample) <= 0.002f) {
                resetSoundProbeContext();
                m_soundState = SoundProbeReadout::State::InvalidSegment;
                return;
            }
            request.rays[i] = makeRay(sample, origin);
        }
    }
    request.sceneGeneration = m_soundSceneGeneration;
    request.sourceId = m_soundSourceId;
    request.sequenceId = m_soundSequence++;
    request.inputTime = inputTime;
    request.reflectionRayCount = static_cast<uint32_t>(m_soundReflectionRayCount);
    request.maxBounces = static_cast<uint32_t>(m_soundMaxBounces);
    request.reverbEnabled = m_soundReverbEnabled;
    request.reflectionOrder = static_cast<uint32_t>(m_soundReflectionOrder);
    request.source = {endpoint.x, endpoint.y, endpoint.z};
    request.listener = makeListenerFrame(*pListener);

#if defined(IMGUI_ENABLED)
    request.captureBounces = m_showSoundBounces;
#endif
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

void Craig::Renderer::updateSteamSimulator(Scene& scene, GameObject& listener, const glm::vec3& origin, const glm::vec3& endpoint,
    SoundPropagation::Clock::time_point inputTime, SoundProbeReadout::State previousState) {
    SteamAudioSimulator::Result result;
    if (m_steamSimulator.poll(result)) {
        SoundProbeReadout readout;
        readout.backend = SoundBackend::CPU;
        readout.obstruction = 1.0f - result.direct.occlusion;
        readout.seed = static_cast<uint32_t>(m_soundSeed);
        readout.sceneGeneration = result.sceneGeneration;
        readout.sourceId = result.sourceId;
        readout.sequenceId = result.sequenceId;
        readout.source = result.source;
        readout.listener = result.listener;
        readout.direct = result.direct;
        readout.reverbEnabled = result.reflections;
        readout.steamReflections = result.reflections;
        readout.steamReflectionParams = result.reflectionParams;
        readout.reflectionOrder = result.order;
        readout.timings = result.timings;
        readout.inputTime = result.inputTime;
        acceptSoundResult(readout);
        mv_soundDebugBounces.clear();
    }

    //Same settings as the GPU gets, the CPU can only do the volumetric direct model though
    SteamAudioSimulator::Request request;
    request.source = {endpoint.x, endpoint.y, endpoint.z};
    request.listener = makeListenerFrame(listener);
    request.occlusionSamples = static_cast<uint32_t>(std::min(m_soundRayCount, static_cast<int>(SoundPropagation::kMaxRayCount / 2)));
    request.occlusionRadius = m_soundOcclusionRadius;
    request.transmissionRays = static_cast<uint32_t>(m_soundTransmissionRays);
    request.reflections = m_soundReverbEnabled;
    request.reflectionRays = static_cast<uint32_t>(m_soundReflectionRayCount);
    request.bounces = static_cast<uint32_t>(m_soundMaxBounces);
    request.order = static_cast<uint32_t>(m_soundReflectionOrder);
    request.sceneGeneration = m_soundSceneGeneration;
    request.sourceId = m_soundSourceId;
    request.sequenceId = m_soundSequence++;
    request.inputTime = inputTime;
    (void)origin;
    if (m_steamSimulator.trySubmit(scene, request) == SteamAudioSimulator::SubmitStatus::GeometryNotReady) {
        if (previousState != SoundProbeReadout::State::GeometryNotReady) {
            Logger::renderer().warn("Steam Audio scene doesn't match the prepared objects; reload the scene to prepare it");
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
    const char* states[] = {"disabled", "paused", "waiting", "ready", "missing endpoint", "invalid segment", "geometry not prepared",
        "backend unavailable"};
    ImGui::SetNextWindowPos(ImVec2(330, 25), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(640, 340), ImGuiCond_FirstUseEver);
    ImGui::Begin("Sound propagation");
    ImGui::Text("State: %s", states[static_cast<unsigned>(readout.state)]);
    ImGui::Text("%s -> %s", m_soundSource.c_str(), m_soundListener.c_str());
    const char* backends[] = {"GPU (Vulkan ray queries)", "CPU (Steam Audio simulator)"};
    int backend = static_cast<int>(m_soundBackend);
    if (ImGui::Combo("Ray tracing", &backend, backends, 2)) {
        m_soundBackend = static_cast<SoundBackend>(backend);
        resetSoundProbeContext();
    }
    ImGui::SetItemTooltip("Same settings either way. GPU is the port, CPU is Valve's stock simulator.");
    if (m_soundBackend == SoundBackend::CPU) {
        //Both get baked into the simulator when it's made, so changing them rebuilds it
        bool rebuild = ImGui::Checkbox("Embree", &m_steamEmbree);
        ImGui::SetItemTooltip("Intel's CPU ray tracer, x86 only. Steam Audio's own one gets used where it isn't there.");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderInt("Threads", &m_steamThreads, 1, 32);
        m_steamThreads = std::clamp(m_steamThreads, 1, 32);
        //Only once the slider's let go, rebuilding on every step of a drag would stall the frame each time
        rebuild |= ImGui::IsItemDeactivatedAfterEdit();
        if (rebuild) {
            resetSoundProbeContext();
            m_cpuSoundReady = m_steamSimulator.prepare(*mp_SceneManager->getCurrentScene(), steamSimulatorSettings()) == CRAIG_SUCCESS;
        }
        if (m_soundBackend == SoundBackend::CPU && !m_soundVolumetric) ImGui::TextDisabled("The CPU only does the volumetric direct model");
    }
    const char* directModels[] = {"Area emitter (obstruction metric)", "Steam Audio volumetric"};
    int directModel = m_soundVolumetric ? 1 : 0;
    bool changed = ImGui::Combo("Direct model", &directModel, directModels, 2);
    m_soundVolumetric = directModel == 1;
    ImGui::SetItemTooltip("Volumetric is Steam Audio's sphere occlusion, the one its CPU simulator does too.\nArea is the world-XY rectangle from the obstruction tests.");
    changed |= ImGui::SliderInt(m_soundVolumetric ? "Occlusion samples" : "Direct rays", &m_soundRayCount, 1,
        m_soundVolumetric ? SoundPropagation::kMaxRayCount / 2 : SoundPropagation::kMaxRayCount);
    if (m_soundVolumetric) changed |= ImGui::SliderFloat("Occlusion radius (m)", &m_soundOcclusionRadius, 0.0f, 4.0f);
    changed |= ImGui::SliderInt("Transmission rays", &m_soundTransmissionRays, 0, 8);
    ImGui::SetItemTooltip("Surfaces counted along the centre line, 0 lets nothing through a wall");
    m_soundOcclusionRadius = std::isfinite(m_soundOcclusionRadius) ? std::clamp(m_soundOcclusionRadius, 0.0f, 4.0f) : 1.0f;
    m_soundTransmissionRays = std::clamp(m_soundTransmissionRays, 0, 8);
    changed |= ImGui::SliderInt("Reflection rays", &m_soundReflectionRayCount, 1, SoundPropagation::kMaxReflectionRayCount);
    changed |= ImGui::SliderInt("Max bounces", &m_soundMaxBounces, 0, kReverbBounces);
    m_soundReflectionRayCount = std::clamp(m_soundReflectionRayCount, 1, static_cast<int>(SoundPropagation::kMaxReflectionRayCount));
    m_soundMaxBounces = std::clamp(m_soundMaxBounces, 0, static_cast<int>(kReverbBounces));
    changed |= ImGui::InputInt("Seed", &m_soundSeed);
    changed |= ImGui::SliderFloat2("Emitter size (m)", m_soundEmitterSize, 0.0f, 4.0f);
    m_soundRayCount = std::clamp(m_soundRayCount, 1, static_cast<int>(m_soundVolumetric ? SoundPropagation::kMaxRayCount / 2 : SoundPropagation::kMaxRayCount));
    for (float& size : m_soundEmitterSize) size = std::isfinite(size) ? std::clamp(size, 0.0f, 4.0f) : 0.0f;
    changed |= ImGui::SliderInt("Ambisonic order", &m_soundReflectionOrder, 0, kMaxReflectionOrder);
    m_soundReflectionOrder = std::clamp(m_soundReflectionOrder, 0, static_cast<int>(kMaxReflectionOrder));
    ImGui::SetItemTooltip("How directional the reflections are, (order + 1)^2 channels. 0 is just the energy, no direction.");
    changed |= ImGui::Checkbox("Trace reflections", &m_soundReverbEnabled);
    ImGui::SetItemTooltip("Steam Audio method, traced on the GPU. Separate from playback's Reflections toggle.");
    if (changed) resetSoundProbeContext();
    ImGui::Checkbox("Show sound paths", &m_showSoundPaths);
    ImGui::SameLine();
    if (ImGui::Checkbox("Show bouncing rays", &m_showSoundBounces) && !m_showSoundBounces) mv_soundDebugBounces.clear();
    if (ImGui::Checkbox("Show direct samples", &m_showSoundRays) && !m_showSoundRays) mv_soundDebugRays.clear();
    if (m_showSoundRays) {
        if (mv_soundDebugRays.empty()) ImGui::TextDisabled("Waiting for ray snapshot");
        else ImGui::Text("Samples: %zu | Sequence %llu",
            mv_soundDebugRays.size(), static_cast<unsigned long long>(m_soundDebugSequence));
        ImGui::SetItemTooltip("Neutral lines, not hit/miss colours. Frozen on pause.");
    }
    if (ImGui::CollapsingHeader("Tracing notes")) {
        ImGui::TextWrapped("Direct: Steam Audio's volumetric sphere, or the world-XY emitter with one ray at its centre. Reflections: listener to source, Steam Audio 4.8.1 method.");
        ImGui::TextWrapped("Reflections: 10 ms energy bins, 2 s after direct sound, SH projected by each listener ray's start direction.");
        ImGui::TextWrapped("Purple: first 128 listener rays, not confirmed source paths. Readout: last completed GPU result.");
    }
    if (readout.hasResult && !changed) {
        if (readout.backend == SoundBackend::CPU) ImGui::Text("Obstructed: %.1f%%", readout.obstruction * 100.0f);
        else ImGui::Text("Blocked: %u / %u (%.1f%%)", readout.blockedRays, readout.rayCount, readout.obstruction * 100.0f);
        if (m_soundVolumetric) ImGui::SetItemTooltip("Only samples the source can see count, the rest are inside a wall");
        ImGui::Text("Age: %.1f ms | Sequence %llu | Seed %u", readout.ageMs, static_cast<unsigned long long>(readout.sequenceId), readout.seed);
        const DirectPath& direct = readout.direct;
        if (ImGui::TreeNode("Direct path")) {
            ImGui::Text("Distance %.2f m | Attenuation %.3f", direct.distance, direct.distanceAttenuation);
            ImGui::Text("Occlusion %.3f (1 is clear)", direct.occlusion);
            ImGui::Text("Air: %.3f / %.3f / %.3f", direct.airAbsorption[0], direct.airAbsorption[1], direct.airAbsorption[2]);
            ImGui::Text("Transmission: %.3f / %.3f / %.3f", direct.transmission[0], direct.transmission[1], direct.transmission[2]);
            ImGui::TextDisabled("Low / mid / high bands, what Steam Audio's direct effect gets");
            ImGui::TreePop();
        }
        const SoundTimings& timings = readout.timings;
        if (readout.backend == SoundBackend::GPU) {
            ImGui::Text("GPU: TLAS %.3f ms | Rays %.3f ms | Readback %.3f ms", timings.gpuAccelerationStructureMs, timings.gpuTraceMs, timings.readbackMs);
            ImGui::SetItemTooltip("TLAS and rays from GPU timestamps, readback is CPU time binning the records. -1 means it didn't run.");
        }
        else {
            ImGui::Text("CPU: Scene %.3f ms | Direct %.3f ms | Reflections %.3f ms", timings.steamSceneMs, timings.steamDirectMs, timings.steamReflectionsMs);
            ImGui::SetItemTooltip("Wall clock on the simulation thread. -1 means it didn't run.");
        }
        if (readout.steamReflections) ImGui::Text("Reflections: order %u, Steam Audio's own response", readout.reflectionOrder);
        if (readout.reflections) {
            //Just the omni channel, the rest are signed direction weights
            float total = 0.0f;
            for (uint32_t bin = 0; bin < kReverbBins; ++bin) total += readout.reflections->at(0, 1, bin);
            ImGui::Text("Reflections: order %u, %u channels, mid omni energy %.3g", readout.reflections->order,
                ReflectionField::channelsFor(readout.reflections->order), total);
        }
    }
    if (ImGui::CollapsingHeader("Submission diagnostics")) {
        const auto& stats = m_soundPropagation.getStatistics();
        ImGui::Text("GPU: submitted %llu | completed %llu | skipped %llu | discarded %llu",
            static_cast<unsigned long long>(stats.submitted), static_cast<unsigned long long>(stats.completed),
            static_cast<unsigned long long>(stats.skipped), static_cast<unsigned long long>(stats.discarded));
        const auto& steamStats = m_steamSimulator.getStatistics();
        ImGui::Text("CPU: submitted %llu | completed %llu | replaced %llu | discarded %llu",
            static_cast<unsigned long long>(steamStats.submitted), static_cast<unsigned long long>(steamStats.completed),
            static_cast<unsigned long long>(steamStats.skipped), static_cast<unsigned long long>(steamStats.discarded));
        ImGui::SetItemTooltip("The CPU only queues one request, a newer frame replaces it instead of waiting");
    }
    ImGui::End();
    drawSoundProbeRays();
#endif
}

void Craig::Renderer::drawSoundProbeRays() {
#if defined(IMGUI_ENABLED)
    if ((!m_showSoundRays || mv_soundDebugRays.empty()) && (!m_showSoundPaths || !m_soundDebugHasPath) &&
        (!m_showSoundBounces || mv_soundDebugBounces.empty())) return;
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
    if (m_showSoundBounces) {
        //Where the listener rays went, not paths that actually reached the source
        //Capped at 128 rays, drawing all 4096 * 64 segments with ImGui tanked the frame rate
        const size_t shown = std::min(mv_soundDebugBounces.size(), static_cast<size_t>(128 * kReverbBounces));
        for (size_t ray = 0; ray + kReverbBounces <= shown; ray += kReverbBounces) {
            glm::vec3 from = m_soundDebugListener;
            for (uint32_t k = 0; k < kReverbBounces; ++k) {
                const auto& hit = mv_soundDebugBounces[ray + k];
                const glm::vec3 value(hit[0], hit[1], hit[2]);
                //w 1 is a hit point, w 2 is a direction it flew off in
                if (hit[3] == 1.0f) {
                    drawSegment(from, value, IM_COL32(180, 140, 255, 40));
                    from = value;
                }
                else {
                    if (hit[3] == 2.0f) drawSegment(from, from + value * 2.0f, IM_COL32(180, 140, 255, 20));
                    break;
                }
            }
        }
    }
    if (m_showSoundPaths) {
        //White line for the direct path from the last result
        if (m_soundDebugHasPath) drawSegment(m_soundDebugListener, m_soundDebugSource, IM_COL32(255, 255, 255, 200));
    }
#endif
}
