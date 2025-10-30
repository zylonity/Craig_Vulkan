Texture2D texSampler : register(t1);
SamplerState texSamplerState : register(s1);

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