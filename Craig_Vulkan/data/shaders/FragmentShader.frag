struct PSInput
{
    float4 pos : SV_Position; // Comes from vertex shader
    float3 color : COLOR0; // Interpolated
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.color, 1.0f);
}