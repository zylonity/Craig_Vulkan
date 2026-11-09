#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

//Valve Corporation, Apache-2.0 - https://github.com/ValveSoftware/steam-audio/tree/v4.8.1/core/src/core
//Changed for this check: minimal vector/scene types round the original CPU shade body
//A formula reference, not a replacement for comparing against the stock CPU library
namespace SteamReference {
struct Vector3f {
    glm::vec3 value;
    Vector3f(float x, float y, float z) : value(x,y,z) {}
    explicit Vector3f(glm::vec3 v = {}) : value(v) {}
    float length() const { return glm::length(value); }
    static float dot(Vector3f a, Vector3f b) { return glm::dot(a.value,b.value); }
    static Vector3f unitVector(Vector3f v) { return Vector3f(glm::normalize(v.value)); }
};
inline Vector3f operator-(Vector3f a, Vector3f b) { return Vector3f(a.value-b.value); }
inline Vector3f operator*(Vector3f a, float b) { return Vector3f(a.value*b); }
inline Vector3f operator/(Vector3f a, float b) { return Vector3f(a.value/b); }
struct CoordinateSpace3f { Vector3f origin; };
struct Material { float absorption[3]; float scattering; };
struct Hit { float distance; Vector3f normal; const Material* material; };
struct Ray { Vector3f origin; Vector3f direction; };
struct IScene { bool anyHit(const Ray&, float, float) const { return false; } };
struct Directivity { float evaluateAt(Vector3f, const CoordinateSpace3f&) const { return 1.0f; } };
namespace Math { inline constexpr float kPi = 3.14159265358979323846f; }
namespace Bands { inline constexpr int kNumBands = 3; }
namespace PropagationMedium { inline constexpr float kSpeedOfSound = 340.0f; }
struct DirectSimulator {
    static float directPathDelay(Vector3f a, Vector3f b) { return (a-b).length()/PropagationMedium::kSpeedOfSound; }
};
struct ReflectionSimulator {
    static constexpr float kSpecularExponent = 1e2f;
    CoordinateSpace3f* mListener;
    CoordinateSpace3f* mSources;
    Directivity* mDirectivities;
    float mIrradianceMinDistance = 1.0f;
    bool shade(const IScene&, const Ray&, int, int, const Hit&, const Vector3f&, const float*, float, float, float*, float&);
};

// reflection_simulator.cpp::shade, unchanged arithmetic/body from 0da18255cca520771f363ee01f100572b39a308e
inline bool ReflectionSimulator::shade(const IScene& scene,
                                const Ray& ray,
                                int bounce,
                                int sourceIndex,
                                const Hit& hit,
                                const Vector3f& hitPoint,
                                const float* accumEnergy,
                                float accumDistance,
                                float scalar,
                                float* energy,
                                float& delay)
{
    const auto& listener = *mListener;

    auto hitToSource = mSources[sourceIndex].origin - hitPoint;
    if (Vector3f::dot(hit.normal, hitToSource) < 0.0f)
        return false;

    auto hitToSourceDistance = hitToSource.length();
    if (hitToSourceDistance <= mIrradianceMinDistance)
        return false;

    Ray shadowRay{ hitPoint, hitToSource / hitToSourceDistance };
    if (scene.anyHit(shadowRay, 0.0f, hitToSourceDistance))
        return false;

    auto diffuseTerm = (1.0f / Math::kPi) * hit.material->scattering * std::max(Vector3f::dot(hit.normal, shadowRay.direction), 0.0f);
    auto halfVector = Vector3f::unitVector((shadowRay.direction - ray.direction) * 0.5f);
    auto specularTerm = ((kSpecularExponent + 2.0f) / (8.0f * Math::kPi)) * (1.0f - hit.material->scattering) * powf(Vector3f::dot(halfVector, hit.normal), kSpecularExponent);
    auto attenuation = 1.0f / std::max(hitToSourceDistance, mIrradianceMinDistance);
    auto distanceTerm = (1.0f / (4.0f * Math::kPi)) * (attenuation * attenuation);
    auto directivityTerm = mDirectivities[sourceIndex].evaluateAt(hitPoint, mSources[sourceIndex]);
    auto frequencyIndependentTerm = scalar * distanceTerm * directivityTerm * (diffuseTerm + specularTerm);

    for (auto j = 0; j < Bands::kNumBands; ++j)
    {
        energy[j] = frequencyIndependentTerm * (1.0f - hit.material->absorption[j]) * accumEnergy[j];
    }

    auto distance = accumDistance + hit.distance + hitToSourceDistance;
    delay = (distance / PropagationMedium::kSpeedOfSound) - DirectSimulator::directPathDelay(listener.origin, mSources[sourceIndex].origin);

    return true;
}

//sampling.cpp::hammersley and generateSphereSamples, unchanged bodies
inline float hammersley(int i)
{
    auto value = 0.0f;
    auto p = 0.5f;
    for (auto k = i; k > 0; p *= 0.5f, k >>= 1)
    {
        if (k & 1) value += p;
    }
    return value;
}
inline void generateSphereSamples(int numSamples, Vector3f* samples)
{
    for (auto i = 0; i < numSamples; ++i)
    {
        auto u = (i + 0.5f) / numSamples;
        auto v = hammersley(i);
        auto azimuth = 2.0f * Math::kPi * u;
        auto height = 2.0f * v - 1.0f;
        auto horizontal = sqrtf(1.0f - height*height);
        samples[i] = Vector3f(horizontal * cosf(azimuth), height, horizontal * -sinf(azimuth));
    }
}
}
