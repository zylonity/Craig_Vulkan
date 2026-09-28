#version 450

// Set 0, binding 0 - per-frame camera data (view + proj). Same for every object this frame.
layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
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

void main()
{
    // Grab this object's model matrix from the SSBO using the push-constant index, then put the node inside it
    mat4 model = transforms[pc.objectIndex].model * pc.nodeMatrix;

    // Apply MVP
    gl_Position = proj * view * model * vec4(inPos, 1.0);

    outColor = inColor;
    outNormal = normalize(mat3(model) * inNormal);
    outTexCoord = inTexCoord;
}
