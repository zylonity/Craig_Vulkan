cbuffer UniformBufferObject : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

struct VSInput
{
    float2 pos : POSITION0; // so a float2 is 32bits, which means it only uses 1 location slot, which is why it's COLOR1 after
    float3 color : COLOR1; // If the position was a double2, it would use 2 location slots, so COLOR1 would become COLOR2
};


struct VSOutput
{
    float4 pos : SV_Position; // Output to rasterizer
    float3 color : COLOR0; // Passed to fragment shader
};


VSOutput main(VSInput input)
{
    VSOutput output;
    
    float4 worldPos = float4(input.pos, 0.0, 1.0);
    
    //Apply MVP
    worldPos = mul(model, worldPos); //Apply model matrix
    worldPos = mul(view, worldPos); //Apply view matrix
    worldPos = mul(proj, worldPos); //Apply projection matrix
    
    
    output.pos = float4(input.pos, 0.0f, 1.0f);
    output.color = input.color;
    return output;
}