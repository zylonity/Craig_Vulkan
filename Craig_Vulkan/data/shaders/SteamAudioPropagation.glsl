//Adapted from Valve Corporation's Steam Audio v4.8.1, Apache-2.0
//https://github.com/ValveSoftware/steam-audio/tree/0da18255cca520771f363ee01f100572b39a308e/core/src/core
//Changed for Craig: coordinate_space.h, ray.cpp and reflection_simulator.cpp turned into GLSL
//sampling.cpp stays on the CPU (Craig_SteamAudioSampling.hpp) and gets uploaded as tables
const float kSteamPi = 3.14159265358979323846;
const float kSteamSoundSpeed = 340.0;
const float kSteamSurfaceOffset = 0.01;
const float kSteamReceiverRadius = 0.1;
const float kSteamY00 = 0.28209479177387814;
const uint kSteamDiffuseSamples = 1024u;

//Turns a hemisphere sample (pointing down -z) into one around the surface normal
vec3 steamTransformHemisphere(vec3 sampleDirection, vec3 normal) {
    vec3 right = abs(normal.x) > abs(normal.z) ? normalize(vec3(-normal.y, normal.x, 0.0)) :
        normalize(vec3(0.0, -normal.z, normal.y));
    vec3 up = cross(right, normal);
    return normalize(sampleDirection.x * right + sampleDirection.y * up - sampleDirection.z * normal);
}

//Distance along the ray to a 0.1 m sphere round the source/listener, huge if it misses
float steamSphereIntersection(vec3 origin, vec3 direction, vec3 centre) {
    vec3 v = origin - centre;
    float b = 2.0 * dot(v, direction);
    float c = dot(v, v) - kSteamReceiverRadius * kSteamReceiverRadius;
    float discriminant = b*b - 4.0*c;
    if (discriminant < 0.0) return 3.402823466e38;
    return -0.5 * (b + sqrt(discriminant));
}

//How much energy this bounce sends towards the source, diffuse + specular lobe, same maths as shade() upstream
vec3 steamReflectionEnergy(vec3 accumulatedEnergy, vec3 absorption, float scattering,
    vec3 incoming, vec3 normal, vec3 towardsSource, float sourceDistance, uint count) {
    float diffuse = (1.0 / kSteamPi) * scattering * max(dot(normal, towardsSource), 0.0);
    vec3 halfVector = (towardsSource - incoming) * 0.5;
    float halfLength = length(halfVector);
    float cosine = halfLength > 0.0 ? dot(halfVector / halfLength, normal) : 0.0;
    float specular = (102.0 / (8.0 * kSteamPi)) * (1.0 - scattering) * pow(max(cosine, 0.0), 100.0);
    float attenuation = 1.0 / max(sourceDistance, 1.0);
    float distanceTerm = (1.0 / (4.0 * kSteamPi)) * attenuation * attenuation;
    float scalar = (4.0 * kSteamPi) / float(count);
    //Order 0 SH only, omnidirectional source. Air absorption gets done later by the reconstruction, not here
    return kSteamY00 * scalar * distanceTerm * (diffuse + specular) * (vec3(1.0) - absorption) * accumulatedEnergy;
}
