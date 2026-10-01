#version 450

// Set 0, binding 0 - per-frame camera data (view + proj). Same for every object this frame.
layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
    vec4 camPos; // only the frag uses this
};

// Set 0, binding 1 - big array of per-object transforms. We index into it using the push constant.
struct PerObjectData
{
    mat4 model;
};

layout(std430, set = 0, binding = 1) readonly buffer TransformBuffer
{
    PerObjectData transforms[];
};

// Push constants, sent per draw. Has to match PushConstantData in Craig_ResourceManager.hpp
layout(push_constant) uniform PushConstants
{
    mat4 nodeMatrix;      // The glTF node's transform inside the model
    vec4 baseColorFactor; // Material colour, only the fragment shader uses it
    uint objectIndex;     // Which slot of the transforms array to read for this draw
    float metallicFactor; // frag only
    float roughnessFactor;
} pc;

// Locations have to match the attribute descriptions in Craig_ResourceManager.cpp
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

// Passed to the fragment shader, locations have to match its inputs
layout(location = 0) out vec3 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outWorldPos;

void main()
{
    // Grab this object's model matrix from the SSBO using the push-constant index, then put the node inside it
    mat4 model = transforms[pc.objectIndex].model * pc.nodeMatrix;

    vec4 worldPos = model * vec4(inPos, 1.0);

    // Apply MVP
    gl_Position = proj * view * worldPos;

    outColor = inColor;
    // inverse transpose keeps normals right when something's scaled unevenly, plain mat3(model) bends them the wrong way
    // a 3x3 inverse per vertex is fine for now, could move it to the CPU later
    outNormal = normalize(transpose(inverse(mat3(model))) * inNormal);
    outTexCoord = inTexCoord;
    outWorldPos = worldPos.xyz;
}
