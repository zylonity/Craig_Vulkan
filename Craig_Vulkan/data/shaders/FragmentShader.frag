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

// Set 0, binding 0 - same camera block as the vertex shader, we just want camPos
layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
    vec4 camPos;
};

// Set 0, binding 2 - light data. All vec4s so std140 lines up with LightData in Craig_Renderer.hpp without any alignas
layout(set = 0, binding = 2) uniform LightData
{
    vec4 lightDir;     // xyz points towards the sun
    vec4 lightColour;  // w is intensity
    vec4 skyColour;
    vec4 groundColour;
};

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inWorldPos; // not used yet, specular will want it

layout(location = 0) out vec4 outColor;

void main()
{
    // Sample the texture using interpolated UVs
    // Tinted by the material colour, white if the material doesn't set one
    vec4 texColor = texture(texSampler, inTexCoord) * pc.baseColorFactor;

    vec3 N = normalize(inNormal);
    vec3 L = normalize(lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);

    // hemisphere ambient, facing up gets sky, facing down gets ground
    vec3 ambient = mix(groundColour.rgb, skyColour.rgb, N.y * 0.5 + 0.5);

    vec3 lit = ambient + lightColour.rgb * lightColour.w * NdotL;

    outColor = vec4(lit * texColor.rgb, texColor.a);
}
