// Set 0, binding 0 - per-frame camera data (view + proj). Same for every object this frame.
[[vk::binding(0, 0)]]
cbuffer CameraData
{
    float4x4 view;
    float4x4 proj;
};

// Set 0, binding 1 - big array of per-object transforms. We index into it using the push constant.
struct PerObjectData
{
    float4x4 model;
};

[[vk::binding(1, 0)]]
StructuredBuffer<PerObjectData> transforms;

// Push constant, tells the shader which slot of the transforms array to read for this draw.
struct PushConstants
{
    uint objectIndex;
};
[[vk::push_constant]] PushConstants pc;

struct VSInput
{
    float3 pos : POSITION0; // so a float2 is 32bits, which means it only uses 1 location slot, which is why it's COLOR1 after
    float3 color : COLOR1; // If the position was a double2, it would use 2 location slots, so COLOR1 would become COLOR2
    float2 texCoord : TEXCOORD2;
};


struct VSOutput
{
    float4 pos : SV_Position; // Output to rasterizer
    float3 color : COLOR0; // Passed to fragment shader
    float2 texCoord : TEXCOORD1; // UVs to fragment
};


VSOutput main(VSInput input)
{
    VSOutput output;

    float4 worldPos = float4(input.pos, 1.0);

    // Grab this object's model matrix from the SSBO using the push-constant index.
    float4x4 model = transforms[pc.objectIndex].model;

    //Apply MVP
    worldPos = mul(model, worldPos); //Apply model matrix
    worldPos = mul(view, worldPos); //Apply view matrix
    worldPos = mul(proj, worldPos); //Apply projection matrix

    output.pos = worldPos;
    output.color = input.color;
    output.texCoord = input.texCoord;

    return output;
}