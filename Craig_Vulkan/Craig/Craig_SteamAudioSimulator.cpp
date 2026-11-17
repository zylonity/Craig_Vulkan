#include "Craig_SteamAudioSimulator.hpp"
#include "Craig_SteamAudio.hpp"
#include "Craig_AcousticGeometry.hpp"
#include "Craig_AcousticMaterial.hpp"
#include "Craig_ResourceManager.hpp"
#include "Craig_Scene.hpp"
#include "Craig_Logger.hpp"
#include "Components/Craig_Model.hpp"

#include <algorithm>
#include <stdexcept>

namespace {
	constexpr float kDuration = Craig::kReverbBins * Craig::kReverbBinSeconds;

	IPLVector3 toIPL(const std::array<float, 3>& v) { return {v[0], v[1], v[2]}; }

	//glm is columns first, Steam Audio's matrix is rows first
	IPLMatrix4x4 toIPL(const glm::mat4& transform) {
		IPLMatrix4x4 matrix{};
		for (int row = 0; row < 4; row++) {
			for (int column = 0; column < 4; column++) matrix.elements[row][column] = transform[column][row];
		}
		return matrix;
	}

	double millisecondsSince(Craig::SteamAudioSimulator::Clock::time_point start) {
		return std::chrono::duration<double, std::milli>(Craig::SteamAudioSimulator::Clock::now() - start).count();
	}
}

CraigError Craig::SteamAudioSimulator::createSimulator(const Settings& settings) {
	const IPLContext context = SteamAudio::getInstance().getContext();
	//The audio might still be reading the old source's response, so it gets parked instead of released
	if (m_source) {
		iplSourceRemove(m_source, m_simulator);
		mv_retiredSources.push_back(m_source);
		m_source = nullptr;
	}
	if (m_simulator) {
		mv_retiredSimulators.push_back(m_simulator);
		m_simulator = nullptr;
	}
	if (m_embree) iplEmbreeDeviceRelease(&m_embree);
	m_embree = nullptr;

	IPLSceneType type = IPL_SCENETYPE_DEFAULT;
	if (settings.rayTracer == RayTracer::Embree) {
		IPLEmbreeDeviceSettings embreeSettings{};
		//Embree's x86 only, so on Apple Silicon this fails and it falls back to Steam Audio's own tracer
		if (iplEmbreeDeviceCreate(context, &embreeSettings, &m_embree) == IPL_STATUS_SUCCESS) type = IPL_SCENETYPE_EMBREE;
		else {
			m_embree = nullptr;
			Logger::audio().warn("Embree isn't available here, the CPU simulator is using Steam Audio's default ray tracer");
		}
	}

	IPLSimulationSettings simulationSettings{};
	simulationSettings.flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT | IPL_SIMULATIONFLAGS_REFLECTIONS);
	simulationSettings.sceneType = type;
	simulationSettings.reflectionType = IPL_REFLECTIONEFFECTTYPE_CONVOLUTION;
	simulationSettings.maxNumOcclusionSamples = kMaxOcclusionSamples;
	simulationSettings.maxNumRays = kMaxRays;
	simulationSettings.numDiffuseSamples = 1024; //Same table size as the GPU's hemisphere samples
	simulationSettings.maxDuration = kDuration;
	simulationSettings.maxOrder = static_cast<IPLint32>(kMaxReflectionOrder);
	simulationSettings.maxNumSources = 1;
	simulationSettings.numThreads = std::max(1, settings.threads);
	simulationSettings.samplingRate = SteamAudio::kSamplingRate;
	simulationSettings.frameSize = SteamAudio::kFrameSize;
	if (iplSimulatorCreate(context, &simulationSettings, &m_simulator) != IPL_STATUS_SUCCESS) {
		m_simulator = nullptr;
		Logger::audio().error("Couldn't create Steam Audio's simulator");
		return CRAIG_FAIL;
	}

	IPLSourceSettings sourceSettings{simulationSettings.flags};
	if (iplSourceCreate(m_simulator, &sourceSettings, &m_source) != IPL_STATUS_SUCCESS) {
		m_source = nullptr;
		Logger::audio().error("Couldn't create the Steam Audio source");
		return CRAIG_FAIL;
	}
	iplSourceAdd(m_source, m_simulator);
	m_settings = settings;
	m_settings.rayTracer = type == IPL_SCENETYPE_EMBREE ? RayTracer::Embree : RayTracer::Default;
	Logger::audio().info("Steam Audio CPU simulator ready ({}, {} threads)", type == IPL_SCENETYPE_EMBREE ? "Embree" : "default ray tracer",
		simulationSettings.numThreads);
	return CRAIG_SUCCESS;
}

void Craig::SteamAudioSimulator::releaseScene() {
	for (auto& instance : mv_instances) {
		if (instance.mesh) {
			iplInstancedMeshRemove(instance.mesh, m_scene);
			iplInstancedMeshRelease(&instance.mesh);
		}
	}
	mv_instances.clear();
	for (auto& [key, subScene] : m_subScenes) {
		if (subScene.mesh) iplStaticMeshRelease(&subScene.mesh);
		if (subScene.scene) iplSceneRelease(&subScene.scene);
	}
	m_subScenes.clear();
	if (m_scene) iplSceneRelease(&m_scene);
	m_scene = nullptr;
}

CraigError Craig::SteamAudioSimulator::prepare(Scene& scene, const Settings& settings) {
	if (!SteamAudio::getInstance().isReady()) return CRAIG_FAIL;
	if (!m_worker.joinable()) {
		m_stopping = false;
		m_worker = std::thread(&SteamAudioSimulator::work, this);
	}
	stop();
	//Wait for anything the worker's halfway through, then nothing touches the library until we're done
	std::lock_guard simulationLock(m_simulationMutex);
	m_ready = false;

	try {
		if (!m_simulator || !(settings == m_settings)) {
			if (createSimulator(settings) != CRAIG_SUCCESS) return CRAIG_FAIL;
		}
		releaseScene();

		const IPLContext context = SteamAudio::getInstance().getContext();
		IPLSceneSettings sceneSettings{};
		sceneSettings.type = m_embree ? IPL_SCENETYPE_EMBREE : IPL_SCENETYPE_DEFAULT;
		sceneSettings.embreeDevice = m_embree;
		if (iplSceneCreate(context, &sceneSettings, &m_scene) != IPL_STATUS_SUCCESS) throw std::runtime_error("scene creation failed");

		//Same objects in the same order as SoundPropagation::collectSceneInstances
		ResourceManager& resources = ResourceManager::getInstance();
		size_t triangles = 0;
		for (GameObject* pObject : scene.getGameObjects()) {
			if (!isAcousticObject(*pObject)) continue;
			const glm::mat4 transform = pObject->GetModelMatrix();
			if (!validAcousticTransform(transform)) continue;
			const auto* pModel = pObject->getComponent<Components::Model>();
			const uint32_t material = std::min<uint32_t>(pModel->getAcousticMaterial(), kAcousticMaterials.size() - 1);

			auto key = std::make_pair(pModel->getModelPath(), material);
			auto entry = m_subScenes.find(key);
			if (entry == m_subScenes.end()) {
				entry = m_subScenes.try_emplace(key).first;
				const AcousticMesh mesh = collectAcousticMesh(resources.getModel(key.first));
				if (mesh.indices.empty()) continue;
				SubScene& subScene = entry->second;
				if (iplSceneCreate(context, &sceneSettings, &subScene.scene) != IPL_STATUS_SUCCESS) throw std::runtime_error("sub-scene creation failed");

				std::vector<IPLVector3> vertices;
				vertices.reserve(mesh.positions.size());
				for (const auto& position : mesh.positions) vertices.push_back(toIPL(position));
				std::vector<IPLTriangle> faces(mesh.triangleCount());
				for (size_t i = 0; i < faces.size(); ++i) {
					faces[i] = {{static_cast<IPLint32>(mesh.indices[3 * i]), static_cast<IPLint32>(mesh.indices[3 * i + 1]),
						static_cast<IPLint32>(mesh.indices[3 * i + 2])}};
				}
				std::vector<IPLint32> materialIndices(faces.size(), 0);
				const AcousticMaterial& preset = kAcousticMaterials[material];
				IPLMaterial iplMaterial{{preset.absorption[0], preset.absorption[1], preset.absorption[2]}, preset.scattering,
					{preset.transmission[0], preset.transmission[1], preset.transmission[2]}};

				IPLStaticMeshSettings meshSettings{};
				meshSettings.numVertices = static_cast<IPLint32>(vertices.size());
				meshSettings.numTriangles = static_cast<IPLint32>(faces.size());
				meshSettings.numMaterials = 1;
				meshSettings.vertices = vertices.data();
				meshSettings.triangles = faces.data();
				meshSettings.materialIndices = materialIndices.data();
				meshSettings.materials = &iplMaterial;
				if (iplStaticMeshCreate(subScene.scene, &meshSettings, &subScene.mesh) != IPL_STATUS_SUCCESS) throw std::runtime_error("static mesh creation failed");
				iplStaticMeshAdd(subScene.mesh, subScene.scene);
				iplSceneCommit(subScene.scene);
				triangles += faces.size();
			}
			if (!entry->second.scene) continue; //No triangles

			//Every object's an instance, so the door moving is just a new transform, not a rebuild
			Instance instance{pObject, pObject->getInstanceId(), nullptr};
			IPLInstancedMeshSettings instanceSettings{entry->second.scene, toIPL(transform)};
			if (iplInstancedMeshCreate(m_scene, &instanceSettings, &instance.mesh) != IPL_STATUS_SUCCESS) throw std::runtime_error("instanced mesh creation failed");
			iplInstancedMeshAdd(instance.mesh, m_scene);
			mv_instances.push_back(instance);
		}
		iplSceneCommit(m_scene);
		iplSimulatorSetScene(m_simulator, m_scene);
		iplSimulatorCommit(m_simulator);
		Logger::audio().info("Steam Audio scene prepared: {} instances, {} sub-scenes, {} triangles", mv_instances.size(), m_subScenes.size(), triangles);
	}
	catch (const std::exception& error) {
		Logger::audio().error("Steam Audio scene preparation failed: {}", error.what());
		releaseScene();
		return CRAIG_FAIL;
	}
	m_ready = true;
	return CRAIG_SUCCESS;
}

void Craig::SteamAudioSimulator::setContext(uint64_t sceneGeneration, uint64_t sourceId) {
	stop();
	std::lock_guard lock(m_mutex);
	m_sceneGeneration = sceneGeneration;
	m_sourceId = sourceId;
	m_acceptRequests = true;
}

void Craig::SteamAudioSimulator::stop() {
	std::lock_guard lock(m_mutex);
	m_acceptRequests = false;
	//Bumped even if the scene/source IDs stay the same, otherwise a pause and resume could let old results back in
	m_contextVersion++;
	if (m_pending) m_statistics.discarded++;
	if (m_completed) m_statistics.discarded++;
	m_pending.reset();
	m_completed.reset();
}

bool Craig::SteamAudioSimulator::collectTransforms(Scene& scene, std::vector<IPLMatrix4x4>& transforms) const {
	//Has to be exactly what got prepared, anything added, removed or swapped needs the scene reloading like the GPU side
	transforms.clear();
	size_t next = 0;
	for (GameObject* pObject : scene.getGameObjects()) {
		if (!isAcousticObject(*pObject)) continue;
		const glm::mat4 transform = pObject->GetModelMatrix();
		if (!validAcousticTransform(transform)) continue;
		if (next == mv_instances.size()) return false;
		const Instance& instance = mv_instances[next];
		//Models with no triangles never got an instance, so skip past those
		if (instance.pObject != pObject) {
			const auto* pModel = pObject->getComponent<Components::Model>();
			const auto key = std::make_pair(pModel->getModelPath(), std::min<uint32_t>(pModel->getAcousticMaterial(), kAcousticMaterials.size() - 1));
			const auto entry = m_subScenes.find(key);
			if (entry != m_subScenes.end() && !entry->second.scene) continue;
			return false;
		}
		if (instance.instanceId != pObject->getInstanceId()) return false;
		transforms.push_back(toIPL(transform));
		next++;
	}
	return next == mv_instances.size();
}

Craig::SteamAudioSimulator::SubmitStatus Craig::SteamAudioSimulator::trySubmit(Scene& scene, Request request) {
	m_statistics.requested++;
	Job job;
	if (!m_ready || !collectTransforms(scene, job.transforms)) {
		m_statistics.skipped++;
		return m_ready ? SubmitStatus::GeometryNotReady : SubmitStatus::Inactive;
	}
	job.request = std::move(request);

	std::lock_guard lock(m_mutex);
	if (!m_acceptRequests || job.request.sceneGeneration != m_sceneGeneration || job.request.sourceId != m_sourceId) {
		m_statistics.skipped++;
		return SubmitStatus::Inactive;
	}
	job.contextVersion = m_contextVersion;
	//Only ever one waiting, a newer frame replaces it, same idea as the GPU skipping when its slots are full
	const bool replaced = m_pending.has_value();
	if (replaced) m_statistics.skipped++;
	m_pending = std::move(job);
	m_statistics.submitted++;
	m_wake.notify_one();
	return replaced ? SubmitStatus::Replaced : SubmitStatus::Submitted;
}

bool Craig::SteamAudioSimulator::poll(Result& result) {
	std::lock_guard lock(m_mutex);
	if (!m_completed) return false;
	if (m_completedVersion != m_contextVersion) {
		m_statistics.discarded++;
		m_completed.reset();
		return false;
	}
	result = *m_completed;
	m_completed.reset();
	return true;
}

void Craig::SteamAudioSimulator::work() {
	for (;;) {
		Job job;
		{
			std::unique_lock lock(m_mutex);
			m_wake.wait(lock, [this] { return m_stopping || m_pending.has_value(); });
			if (m_stopping) return;
			job = std::move(*m_pending);
			m_pending.reset();
		}

		Result result;
		{
			std::lock_guard simulationLock(m_simulationMutex);
			if (!m_ready) continue;
			result = simulate(job);
		}

		std::lock_guard lock(m_mutex);
		m_statistics.completed++;
		if (job.contextVersion != m_contextVersion) {
			m_statistics.discarded++;
			continue;
		}
		if (m_completed) m_statistics.discarded++;
		m_completed = result;
		m_completedVersion = job.contextVersion;
	}
}

Craig::SteamAudioSimulator::Result Craig::SteamAudioSimulator::simulate(const Job& job) {
	const Request& request = job.request;
	Result result;
	result.source = request.source;
	result.listener = request.listener;
	result.sceneGeneration = request.sceneGeneration;
	result.sourceId = request.sourceId;
	result.sequenceId = request.sequenceId;
	result.inputTime = request.inputTime;

	//The door moving is just its instance getting a new transform, then the scene gets committed
	auto start = Clock::now();
	for (size_t i = 0; i < mv_instances.size(); ++i) iplInstancedMeshUpdateTransform(mv_instances[i].mesh, m_scene, job.transforms[i]);
	iplSceneCommit(m_scene);
	result.timings.steamSceneMs = millisecondsSince(start);

	const ListenerFrame& listener = request.listener;
	const IPLSimulationFlags flags = static_cast<IPLSimulationFlags>(IPL_SIMULATIONFLAGS_DIRECT |
		(request.reflections ? IPL_SIMULATIONFLAGS_REFLECTIONS : 0));
	IPLSimulationSharedInputs shared{};
	shared.listener = {toIPL(listener.right), toIPL(listener.up), toIPL(listener.ahead), toIPL(listener.origin)};
	shared.numRays = static_cast<IPLint32>(std::min(request.reflectionRays, kMaxRays));
	shared.numBounces = static_cast<IPLint32>(request.bounces);
	shared.duration = kDuration;
	shared.order = static_cast<IPLint32>(std::min(request.order, kMaxReflectionOrder));
	shared.irradianceMinDistance = 1.0f; //What the GPU port has baked in
	iplSimulatorSetSharedInputs(m_simulator, flags, &shared);

	IPLSimulationInputs inputs{};
	inputs.flags = flags;
	inputs.directFlags = static_cast<IPLDirectSimulationFlags>(IPL_DIRECTSIMULATIONFLAGS_DISTANCEATTENUATION |
		IPL_DIRECTSIMULATIONFLAGS_AIRABSORPTION | IPL_DIRECTSIMULATIONFLAGS_OCCLUSION | IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION);
	inputs.source = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, toIPL(request.source)};
	inputs.distanceAttenuationModel.type = IPL_DISTANCEATTENUATIONTYPE_DEFAULT;
	inputs.airAbsorptionModel.type = IPL_AIRABSORPTIONTYPE_DEFAULT;
	inputs.directivity = {0.0f, 1.0f, nullptr, nullptr}; //Omnidirectional, like the GPU's source
	inputs.occlusionType = IPL_OCCLUSIONTYPE_VOLUMETRIC;
	inputs.occlusionRadius = request.occlusionRadius;
	inputs.numOcclusionSamples = static_cast<IPLint32>(std::clamp(request.occlusionSamples, 1u, kMaxOcclusionSamples));
	inputs.numTransmissionRays = static_cast<IPLint32>(std::max(request.transmissionRays, 1u));
	inputs.reverbScale[0] = inputs.reverbScale[1] = inputs.reverbScale[2] = 1.0f;
	iplSourceSetInputs(m_source, flags, &inputs);

	start = Clock::now();
	iplSimulatorRunDirect(m_simulator);
	result.timings.steamDirectMs = millisecondsSince(start);
	if (request.reflections) {
		start = Clock::now();
		iplSimulatorRunReflections(m_simulator);
		result.timings.steamReflectionsMs = millisecondsSince(start);
	}

	IPLSimulationOutputs outputs{};
	iplSourceGetOutputs(m_source, flags, &outputs);
	DirectPath& direct = result.direct;
	direct.distanceAttenuation = outputs.direct.distanceAttenuation;
	direct.occlusion = outputs.direct.occlusion;
	for (int band = 0; band < 3; ++band) {
		direct.airAbsorption[band] = outputs.direct.airAbsorption[band];
		//Upstream needs at least one transmission ray, so 0 on the slider means nothing gets through like the GPU
		direct.transmission[band] = request.transmissionRays ? outputs.direct.transmission[band] : 0.0f;
	}
	const glm::vec3 offset = glm::vec3(request.source[0], request.source[1], request.source[2]) -
		glm::vec3(listener.origin[0], listener.origin[1], listener.origin[2]);
	direct.distance = glm::length(offset);
	if (request.reflections) {
		result.reflections = true;
		result.reflectionParams = outputs.reflections;
		result.order = static_cast<uint32_t>(shared.order);
	}
	return result;
}

CraigError Craig::SteamAudioSimulator::terminate() {
	{
		std::lock_guard lock(m_mutex);
		m_stopping = true;
		m_pending.reset();
		m_completed.reset();
	}
	m_wake.notify_one();
	if (m_worker.joinable()) m_worker.join();

	//The audio has to be stopped before this, the source's response is read straight off the audio thread
	releaseScene();
	if (m_source) {
		iplSourceRemove(m_source, m_simulator);
		iplSourceRelease(&m_source);
	}
	for (IPLSource& source : mv_retiredSources) iplSourceRelease(&source);
	mv_retiredSources.clear();
	if (m_simulator) iplSimulatorRelease(&m_simulator);
	for (IPLSimulator& simulator : mv_retiredSimulators) iplSimulatorRelease(&simulator);
	mv_retiredSimulators.clear();
	if (m_embree) iplEmbreeDeviceRelease(&m_embree);
	m_source = nullptr;
	m_simulator = nullptr;
	m_embree = nullptr;
	m_ready = false;
	m_stopping = false;
	return CRAIG_SUCCESS;
}
