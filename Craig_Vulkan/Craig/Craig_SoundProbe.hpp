#pragma once

#include <array>
#include <cstdint>

namespace Craig {

	inline constexpr uint32_t kMaxSoundArrivals = 64;
	//Longest delay the audio can play back, about 2.7 s at 48 kHz, power of two so the ring buffer can wrap with a mask
	inline constexpr uint32_t kSoundDelayCapacity = 1u << 17;

	//Reflections come back as energy in 10 ms bins, 2 s worth, from up to 64 bounces per ray
	inline constexpr uint32_t kReverbBins = 200;
	inline constexpr uint32_t kReverbBounces = 64;
	inline constexpr float kReverbBinSeconds = 0.01f;
	//Steam Audio's order 0 SH energy per band, the bins are how long after the direct sound it arrives
	using ReverbEnergy = std::array<std::array<float, 3>, kReverbBins>;

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
		ReverbEnergy reverbEnergy{};
		bool reverbEnabled = false;
	};

}
