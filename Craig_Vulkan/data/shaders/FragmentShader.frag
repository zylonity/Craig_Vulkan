// Set 1, binding 0 - per-object texture. Rebinds each draw.
[[vk::binding(0, 1)]] Texture2D texSampler;
[[vk::binding(0, 1)]] SamplerState texSamplerState;

// Push constants, has to match the vertex shader + PushConstantData in Craig_ResourceManager.hpp
struct PushConstants
{
    float4x4 nodeMatrix;
    float4 baseColorFactor; // Material colour
    uint objectIndex;
};
[[vk::push_constant]] PushConstants pc;

//set 0, binding 1 - light shit
[[vk::binding(2, 0)]]
cbuffer LightData{
    float3 lightDir;
    float3 lightColor;
    float3 ambientColor;
}

struct PSInput
{
    float4 pos : SV_Position; // Comes from vertex shader
    float3 color : COLOR0; // Interpolated
    float3 normals : NORMAL1;
    float2 texCoord : TEXCOORD2;
};

float4 main(PSInput input) : SV_Target
{
    // Sample the texture using interpolated UVs
    // Tinted by the material colour, white if the material doesn't set one
    float4 texColor = texSampler.Sample(texSamplerState, input.texCoord) * pc.baseColorFactor;

    float3 N = normalize(input.normals);
    float3 L = normalize(lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 lit = ambientColor.rgb + lightColor.rgb * NdotL;

    return float4(lit * texColor.rgb, texColor.a);
}