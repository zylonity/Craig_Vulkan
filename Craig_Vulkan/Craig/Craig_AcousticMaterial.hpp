#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace Craig {

	//Three bands like Steam Audio does it: below 800 Hz, 800 Hz to 8 kHz, above 8 kHz
	inline constexpr uint32_t kAcousticBandCount = 3;
	inline constexpr std::array<float, 2> kAcousticBandSplits = {800.0f, 8000.0f};

	//Absorption and scattering are the fraction of energy per bounce, transmission is the gain that gets through the surface
	struct AcousticMaterial {
		const char* name;
		std::array<float, kAcousticBandCount> absorption;
		float scattering;
		std::array<float, kAcousticBandCount> transmission;
	};

	//Presets and air absorption are Valve Corporation's, Steam Audio 4.8.1, Apache-2.0 - https://github.com/ValveSoftware/steam-audio
	//(unity/.../Resources/Materials/*.asset and core/src/core/air_absorption.cpp)
	inline constexpr std::array<AcousticMaterial, 9> kAcousticMaterials = {{
		{"Generic", {0.10f, 0.20f, 0.30f}, 0.05f, {0.100f, 0.050f, 0.030f}},
		{"Brick", {0.03f, 0.04f, 0.07f}, 0.05f, {0.015f, 0.015f, 0.015f}},
		{"Concrete", {0.05f, 0.07f, 0.08f}, 0.05f, {0.015f, 0.002f, 0.001f}},
		{"Carpet", {0.24f, 0.69f, 0.73f}, 0.05f, {0.020f, 0.005f, 0.003f}},
		{"Glass", {0.06f, 0.03f, 0.02f}, 0.05f, {0.060f, 0.044f, 0.011f}},
		{"Metal", {0.20f, 0.07f, 0.06f}, 0.05f, {0.200f, 0.025f, 0.010f}},
		{"Plaster", {0.12f, 0.06f, 0.04f}, 0.05f, {0.056f, 0.056f, 0.004f}},
		{"Wood", {0.11f, 0.07f, 0.06f}, 0.05f, {0.070f, 0.014f, 0.005f}},
		{"Rock", {0.13f, 0.20f, 0.24f}, 0.05f, {0.015f, 0.002f, 0.001f}},
	}};
	//gain = exp(-coefficient * metres)
	inline constexpr std::array<float, kAcousticBandCount> kAirAbsorption = {0.0002f, 0.0017f, 0.0182f};
	inline constexpr float kSpeedOfSound = 340.0f; //Steam Audio 4.8.1 uses this too (propagation_medium.h)

	//Returns kAcousticMaterials.size() if it can't find it
	inline uint32_t findAcousticMaterial(std::string_view name) {
		for (uint32_t i = 0; i < kAcousticMaterials.size(); ++i) {
			if (name == kAcousticMaterials[i].name) return i;
		}
		return kAcousticMaterials.size();
	}

}
