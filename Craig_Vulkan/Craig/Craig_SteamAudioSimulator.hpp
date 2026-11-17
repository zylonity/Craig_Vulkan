#pragma once

#include "Craig_Constants.hpp"
#include "Craig_SoundProbe.hpp"
#include <phonon.h>
#include <array>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace Craig {

	class Scene;
	class GameObject;

	//Valve's stock CPU simulator, untouched, the baseline the GPU port gets compared against
	//Valve Corporation, Apache-2.0 - Steam Audio 4.8.1, https://github.com/ValveSoftware/steam-audio
	//Runs on its own thread so a slow simulation never holds up a frame, same submit/poll shape as SoundPropagation
	class SteamAudioSimulator {
	public:
		using Clock = std::chrono::steady_clock;
		enum class RayTracer { Default, Embree };
		static constexpr uint32_t kMaxOcclusionSamples = 1024;
		static constexpr uint32_t kMaxRays = 4096;

		//Settings the library only takes when the simulator gets made
		struct Settings {
			RayTracer rayTracer = RayTracer::Default;
			int threads = 4;
			bool operator==(const Settings&) const = default;
		};

		//What the renderer hands over each frame, the instance transforms get filled in by trySubmit
		struct Request {
			std::array<float, 3> source{};
			ListenerFrame listener;
			uint32_t occlusionSamples = 128;
			float occlusionRadius = 1.0f;
			uint32_t transmissionRays = 3;
			bool reflections = true;
			uint32_t reflectionRays = 1024;
			uint32_t bounces = 64;
			uint32_t order = 1;
			uint64_t sceneGeneration = 0;
			uint64_t sourceId = 0;
			uint64_t sequenceId = 0;
			Clock::time_point inputTime;
		};

		struct Result {
			DirectPath direct;
			bool reflections = false;
			IPLReflectionEffectParams reflectionParams{};
			uint32_t order = 0;
			SoundTimings timings;
			std::array<float, 3> source{};
			ListenerFrame listener;
			uint64_t sceneGeneration = 0;
			uint64_t sourceId = 0;
			uint64_t sequenceId = 0;
			Clock::time_point inputTime;
		};

		struct Statistics {
			uint64_t requested = 0;
			uint64_t submitted = 0;
			uint64_t completed = 0;
			uint64_t skipped = 0;
			uint64_t discarded = 0;
		};
		enum class SubmitStatus { Submitted, Replaced, Inactive, GeometryNotReady };

		~SteamAudioSimulator() { terminate(); }
		//Builds the Steam Audio scene from the same triangles the BLASes get, waits for the worker first
		CraigError prepare(Scene& scene, const Settings& settings);
		bool isReady() const { return m_ready; }
		const Settings& getSettings() const { return m_settings; }
		//Anything submitted before this gets thrown away when it comes back
		void setContext(uint64_t sceneGeneration, uint64_t sourceId);
		void stop();
		//If the worker's still busy the queued request gets swapped for this newer one, it never waits
		SubmitStatus trySubmit(Scene& scene, Request request);
		bool poll(Result& result);
		const Statistics& getStatistics() const { return m_statistics; }
		CraigError terminate();

	private:
		struct Job {
			Request request;
			std::vector<IPLMatrix4x4> transforms;
			uint64_t contextVersion = 0;
		};
		//One sub-scene per model and material, the library can't override a static mesh's material per instance
		struct SubScene {
			IPLScene scene = nullptr;
			IPLStaticMesh mesh = nullptr;
		};
		struct Instance {
			GameObject* pObject = nullptr;
			uint64_t instanceId = 0;
			IPLInstancedMesh mesh = nullptr;
		};

		CraigError createSimulator(const Settings& settings);
		void releaseScene();
		bool collectTransforms(Scene& scene, std::vector<IPLMatrix4x4>& transforms) const;
		void work();
		Result simulate(const Job& job);

		bool m_ready = false;
		Settings m_settings;
		IPLEmbreeDevice m_embree = nullptr;
		IPLSimulator m_simulator = nullptr;
		IPLSource m_source = nullptr;
		//Old sources stay alive until shutdown, the audio thread might still be reading a response out of one
		std::vector<IPLSource> mv_retiredSources;
		std::vector<IPLSimulator> mv_retiredSimulators;
		IPLScene m_scene = nullptr;
		std::map<std::pair<std::string, uint32_t>, SubScene> m_subScenes;
		std::vector<Instance> mv_instances;

		std::thread m_worker;
		std::mutex m_mutex; //Guards the job/result handover
		std::mutex m_simulationMutex; //Held while the library's busy, so prepare can't swap the scene under it
		std::condition_variable m_wake;
		bool m_stopping = false;
		std::optional<Job> m_pending;
		std::optional<Result> m_completed;
		uint64_t m_contextVersion = 0;
		uint64_t m_completedVersion = 0;
		uint64_t m_sceneGeneration = 0;
		uint64_t m_sourceId = 0;
		bool m_acceptRequests = false;
		Statistics m_statistics;
	};

}
