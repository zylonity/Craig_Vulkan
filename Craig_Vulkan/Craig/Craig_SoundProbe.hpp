#pragma once

#include <array>
#include <cstdint>

namespace Craig {

	inline constexpr uint32_t kMaxSoundArrivals = 64;

	//One way the sound gets to the listener, order 0 is the direct path
	struct SoundArrival {
		float delaySeconds = 0.0f;
		std::array<float, 3> gains{}; //Per band, distance, air and material losses all together
		float pan = 0.0f; //-1 left, 1 right of the listener
		uint32_t order = 0;
		float length = 0.0f;
	};

	//Just the numbers, no Vulkan handles cross over into the game or audio
	struct SoundProbeReadout {
		enum class State { Disabled, Paused, Waiting, Ready, MissingEndpoint, InvalidSegment, GeometryNotReady };
		State state = State::Disabled;
		bool hasResult = false;
		float obstruction = 0.0f; //Blocked rays / all rays
		uint32_t blockedRays = 0;
		uint32_t rayCount = 0;
		uint32_t seed = 0;
		uint64_t sceneGeneration = 0;
		uint64_t sourceId = 0;
		uint64_t sequenceId = 0;
		double ageMs = 0.0;
		std::array<SoundArrival, kMaxSoundArrivals> arrivals{};
		uint32_t arrivalCount = 0;
	};

}
