#pragma once

#include "Craig_SoundProbe.hpp"
#include <array>
#include <cstdint>
#include <vector>
#include <glm/glm.hpp>

namespace Craig {

	//Steam Audio's names, the same numbers go to iplPathBakerBake and the GPU bake
	struct PathingSettings {
		float probeSpacing = 2.0f;
		float probeHeight = 1.5f;
		uint32_t visSamples = 4;
		float visRadius = 0.5f;
		float visThreshold = 0.1f;
		float visRange = 50.0f;
		float pathRange = 100.0f;
		bool operator==(const PathingSettings&) const = default;
	};

	struct ProbeSphere {
		glm::vec3 centre{0.0f};
		float radius = 0.0f;
	};

	//Valve Corporation's Steam Audio 4.8.1 path finding, Apache-2.0 - https://github.com/ValveSoftware/steam-audio
	//path_visibility.cpp, path_finder.cpp, path_data.cpp, path_simulator.cpp, probe_manager.cpp, deviation.cpp and eq_effect.cpp
	//Changed for Craig: the probe visibility comes from the GPU bake, no runtime validation or alternate paths,
	//and every influencing probe gets checked instead of walking upstream's probe tree
	class SoundPathing {
	public:
		static constexpr uint32_t kMaxNeighbours = 8; //ProbeNeighborhood::kMaxProbesPerBatch
		using Pair = std::array<uint32_t, 2>;

		//Probe pairs worth testing, i > j and close enough sideways, upstream's asymmetric range with down as -y
		static std::vector<Pair> candidatePairs(const std::vector<ProbeSphere>& probes, float visRange);
		//The visibility graph from the bake, then every probe's shortest paths the same as the baker
		void build(const std::vector<ProbeSphere>& probes, const std::vector<Pair>& pairs, const std::vector<uint8_t>& visible, float pathRange);
		void clear();
		bool isReady() const { return !mv_probes.empty(); }
		const std::vector<ProbeSphere>& getProbes() const { return mv_probes; }
		uint32_t getEdgeCount() const { return m_edges; }

		//Probes whose influence sphere holds the point, nearest first, up to kMaxNeighbours
		uint32_t influencingProbes(const glm::vec3& point, std::array<uint32_t, kMaxNeighbours>& probes) const;

		//What the GPU traced for one frame, the occlusion flags line up with the influencing probe lists
		struct Query {
			glm::vec3 source{0.0f};
			glm::vec3 listener{0.0f};
			std::array<uint32_t, kMaxNeighbours> sourceProbes{};
			std::array<uint32_t, kMaxNeighbours> listenerProbes{};
			std::array<bool, kMaxNeighbours> sourceOccluded{};
			std::array<bool, kMaxNeighbours> listenerOccluded{};
			uint32_t sourceCount = 0;
			uint32_t listenerCount = 0;
			bool directOccluded = false;
			uint32_t order = 1;
		};
		//PathSimulator::findPaths with validation off
		PathingResult findPaths(const Query& query) const;

	private:
		//The upstream SoundPath, only the ends of the route and its totals
		struct SoundPath {
			int firstProbe = -1;
			int lastProbe = -1;
			int probeAfterFirst = -1;
			int probeBeforeLast = -1;
			bool direct = false;
			float distanceInternal = 0.0f;
			float deviationInternal = 0.0f;
			bool isValid() const { return direct || (firstProbe >= 0 && lastProbe >= 0); }
		};
		std::vector<int> nodes(uint32_t start, uint32_t end) const; //Probes between start and end from start's search, both ends left out
		bool hasPath(uint32_t start, uint32_t end) const;
		SoundPath makePath(uint32_t start, uint32_t end) const;
		SoundPath lookupShortestPath(uint32_t start, uint32_t end) const;
		float distance(const SoundPath& path, uint32_t start, uint32_t end) const;
		float deviation(const SoundPath& path, uint32_t start, uint32_t end) const;
		glm::vec3 toVirtualSource(const SoundPath& path, uint32_t start, uint32_t end) const;

		std::vector<ProbeSphere> mv_probes;
		std::vector<std::vector<std::pair<uint32_t, float>>> mv_adjacent;
		std::vector<int32_t> mv_parents; //N x N, row is the search's start probe, -1 means no parent
		uint32_t m_edges = 0;
	};

	//The default deviation model, Steam Audio's Uniform Theory of Diffraction term for one band
	float utdDeviation(float angle, uint32_t band);

}
