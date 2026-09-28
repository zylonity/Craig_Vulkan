#version 450

// Set 1, binding 0 - per-object texture. Rebinds each draw.
layout(set = 1, binding = 0) uniform sampler2D texSampler;

// Push constants, has to match the vertex shader + PushConstantData in Craig_ResourceManager.hpp
layout(push_constant) uniform PushConstants
{
    mat4 nodeMatrix;
    vec4 baseColorFactor; // Material colour
    uint objectIndex;
} pc;

// Set 0, binding 2 - light data. std140 pads each vec3 to 16 bytes, matches the alignas(16) in Craig_Renderer.hpp
layout(set = 0, binding = 2) uniform LightData
{
    vec3 lightDir;
    vec3 lightColor;
    vec3 ambientColor;
};

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    // Sample the texture using interpolated UVs
    // Tinted by the material colour, white if the material doesn't set one
    vec4 texColor = texture(texSampler, inTexCoord) * pc.baseColorFactor;

    vec3 N = normalize(inNormal);
    vec3 L = normalize(lightDir);
    float NdotL = max(dot(N, L), 0.0);
    vec3 lit = ambientColor + lightColor * NdotL;

    outColor = vec4(lit * texColor.rgb, texColor.a);
}
