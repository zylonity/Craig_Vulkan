#version 450

// Sky dome, the atmosphere's already been worked out per vertex in SkyVertexShader.vert
// this just does the exposure and draws the sun disk, both need to be per pixel to look right
// exposure line is from the glsl-atmosphere README example (Rye Terrell, Unlicense)

// Set 0, binding 2 - the sun, same block as FragmentShader.frag
layout(set = 0, binding = 2) uniform LightData
{
    vec4 lightDir;     // xyz points towards the sun
    vec4 lightColour;  // w is intensity
    vec4 skyColour;
    vec4 groundColour;
};

layout(location = 0) in vec3 inSkyColour;
layout(location = 1) in vec3 inViewDir;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 dir = normalize(inViewDir);
    vec3 sunDir = normalize(lightDir.xyz);

    // Apply exposure.
    vec3 color = 1.0 - exp(-1.0 * inSkyColour);

    // CRAIG: the original has no sun disk, just the glow, so draw one. ~0.5 degree radius, bit bigger than the real one
    // hidden below the horizon so it doesn't show through the ground
    float sunRadius = cos(radians(0.5));
    float disk = smoothstep(sunRadius - 0.00002, sunRadius + 0.00002, dot(dir, sunDir)) * step(0.0, dir.y);
    color = mix(color, clamp(lightColour.rgb, 0.0, 1.0), disk);

    outColor = vec4(color, 1.0);
}
