#pragma once

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

//Valve Corporation, Apache-2.0 - Steam Audio v4.8.1 sampling.cpp
//https://github.com/ValveSoftware/steam-audio/tree/0da18255cca520771f363ee01f100572b39a308e/core/src/core
//Changed for Craig: writes vec4 tables for the GPU instead of Vector3f arrays, the sampling maths is the same
namespace Craig {
    inline constexpr uint32_t kSteamDiffuseSamples = 1024;

    //sphere: the directions the listener rays start in, hemisphere: diffuse bounce directions
    inline void fillSteamAudioSamples(std::span<std::array<float, 4>> sphere, std::span<std::array<float, 4>> hemisphere) {
        constexpr float pi = 3.14159265358979323846f;
        //Van der Corput radical inverse, gives the second Hammersley coordinate
        const auto hammersley = [](uint32_t i) {
            float value = 0.0f, p = 0.5f;
            for (uint32_t k = i; k > 0; p *= 0.5f, k >>= 1) if (k & 1) value += p;
            return value;
        };
        for (uint32_t i = 0; i < sphere.size(); ++i) {
            const float u = (i + 0.5f) / sphere.size(), v = hammersley(i);
            const float azimuth = 2.0f * pi * u, height = 2.0f * v - 1.0f;
            const float horizontal = std::sqrt(1.0f - height * height);
            sphere[i] = {horizontal * std::cos(azimuth), height, horizontal * -std::sin(azimuth), 0.0f};
        }
        for (uint32_t i = 0; i < hemisphere.size(); ++i) {
            const float u = (i + 0.5f) / hemisphere.size(), v = hammersley(i);
            const float r = std::sqrt(u), theta = 2.0f * pi * v;
            const float x = r * std::cos(theta), y = r * std::sin(theta);
            hemisphere[i] = {x, y, -std::sqrt(std::max(0.0f, 1.0f - x*x - y*y)), 0.0f};
        }
    }
}
