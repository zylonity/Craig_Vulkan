// Set 1, binding 0 - per-object texture. Rebinds each draw.
[[vk::binding(0, 1)]] Texture2D texSampler;
[[vk::binding(0, 1)]] SamplerState texSamplerState;

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
    float4 texColor = texSampler.Sample(texSamplerState, input.texCoord);

    // Hardcoded directional light (sunlight from upper-right-front)
    //float3 lightDir = normalize(float3(0.5, 1.0, 0.25));
    //float3 lightColor = float3(1.0, 0.98, 0.95);
    //float3 ambient = float3(0.05, 0.05, 0.08);

    float3 N = normalize(input.normals);
    float3 L = normalize(lightDir.xyz);
    float NdotL = max(dot(N, L), 0.0);
    float3 lit = ambientColor.rgb + lightColor.rgb * NdotL;

    return float4(lit * texColor.rgb, texColor.a);
}