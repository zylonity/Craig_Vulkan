#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace Craig {

	//Reflections come back as energy in 10 ms bins, 2 s worth, from up to 64 bounces per ray
	inline constexpr uint32_t kReverbBins = 200;
	inline constexpr uint32_t kReverbBounces = 64;
	inline constexpr float kReverbBinSeconds = 0.01f;
	inline constexpr uint32_t kReverbBands = 3;
	//Third order is as far as the decoder goes, 16 channels
	inline constexpr uint32_t kMaxReflectionOrder = 3;

	//Steam Audio's energy field layout, channels x bands x bins, so it copies straight into an IPLEnergyField
	//The bins are how long after the direct sound it arrives
	struct ReflectionField {
		uint32_t order = 0;
		std::vector<float> energy;
		static constexpr uint32_t channelsFor(uint32_t order) { return (order + 1) * (order + 1); }
		void reset(uint32_t newOrder) { order = newOrder; energy.assign(channelsFor(order) * kReverbBands * kReverbBins, 0.0f); }
		float& at(uint32_t channel, uint32_t band, uint32_t bin) { return energy[(channel * kReverbBands + band) * kReverbBins + bin]; }
		float at(uint32_t channel, uint32_t band, uint32_t bin) const { return energy[(channel * kReverbBands + band) * kReverbBins + bin]; }
	};

	//The same numbers Steam Audio's direct simulation hands back, so either backend can drive one IPLDirectEffect
	struct DirectPath {
		float distanceAttenuation = 1.0f;
		std::array<float, 3> airAbsorption{1.0f, 1.0f, 1.0f};
		float occlusion = 1.0f; //Steam Audio's way round, 1 is nothing in the way
		std::array<float, 3> transmission{1.0f, 1.0f, 1.0f};
		float distance = 0.0f;
	};

	//Where the listener is and which way it's facing, world space
	struct ListenerFrame {
		std::array<float, 3> origin{};
		std::array<float, 3> right{1.0f, 0.0f, 0.0f};
		std::array<float, 3> up{0.0f, 1.0f, 0.0f};
		std::array<float, 3> ahead{0.0f, 0.0f, -1.0f};
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
		std::array<float, 3> source{};
		ListenerFrame listener;
		DirectPath direct;
		//Shared so a 16 channel field doesn't get copied every time the readout does
		std::shared_ptr<const ReflectionField> reflections;
		bool reverbEnabled = false;
	};

}
