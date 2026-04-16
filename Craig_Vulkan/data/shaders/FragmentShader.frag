// Set 1, binding 0 - per-object texture. Rebinds each draw.
[[vk::binding(0, 1)]] Texture2D texSampler;
[[vk::binding(0, 1)]] SamplerState texSamplerState;

struct PSInput
{
    float4 pos : SV_Position; // Comes from vertex shader
    float3 color : COLOR0; // Interpolated
    float2 texCoord : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target
{
    // Sample the texture using interpolated UVs
    float4 texColor = texSampler.Sample(texSamplerState, input.texCoord);

    return texColor;// * float4(input.color, 1.0);
}