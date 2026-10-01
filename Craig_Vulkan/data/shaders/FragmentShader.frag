#version 450

// The PBR functions in here are from Sascha Willems' Vulkan examples (MIT), see External/SaschaWillems_LICENSE.md
// pbrtexture.frag: https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/pbrtexture/pbrtexture.frag
// pbrbasic.frag (same BRDF, no textures): https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/pbrbasic/pbr.frag
// grabbed at commit 41a4410243fca7640a2dd0115a5ff5cb9a29494b
// Anything changed from the original is marked with "CRAIG:"

// Set 1 - per-material textures. Rebinds each draw.
layout(set = 1, binding = 0) uniform sampler2D texSampler;
// UNORM, not sRGB. White if the material doesn't have one so the factors pass straight through
layout(set = 1, binding = 1) uniform sampler2D metallicRoughnessMap;

// Push constants, has to match the vertex shader + PushConstantData in Craig_ResourceManager.hpp
layout(push_constant) uniform PushConstants
{
    mat4 nodeMatrix;
    vec4 baseColorFactor; // Material colour
    uint objectIndex;
    float metallicFactor;
    float roughnessFactor;
} pc;

// Set 0, binding 0 - same camera block as the vertex shader, we just want camPos
layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
    vec4 camPos;
};

// Set 0, binding 2 - light data. All vec4s so std140 lines up with LightData in Craig_Renderer.hpp without any alignas
layout(set = 0, binding = 2) uniform LightData
{
    vec4 lightDir;     // xyz points towards the sun
    vec4 lightColour;  // w is intensity
    vec4 skyColour;
    vec4 groundColour;
};

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inWorldPos;

layout(location = 0) out vec4 outColor;

#define PI 3.1415926535897932384626433832795
// CRAIG: original is pow(texture(albedoMap, inUV).rgb, vec3(2.2)), our textures are already an sRGB format so the sampler linearises for us
#define ALBEDO (texture(texSampler, inTexCoord).rgb * pc.baseColorFactor.rgb)

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom);
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
vec3 F_SchlickR(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 specularContribution(vec3 L, vec3 V, vec3 N, vec3 F0, float metallic, float roughness)
{
	// Precalculate vectors and dot products
	vec3 H = normalize (V + L);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);

	// CRAIG: was fixed at vec3(1.0), now it's the sun
	vec3 lightColor = lightColour.rgb * lightColour.w;

	vec3 color = vec3(0.0);

	if (dotNL > 0.0) {
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness);
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, F0);
		vec3 spec = D * F * G / (4.0 * dotNL * dotNV + 0.001);
		vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
		// CRAIG: added * lightColor, the original never actually used it
		color += (kD * ALBEDO / PI + spec) * dotNL * lightColor;
	}

	return color;
}

// CRAIG: stands in for the irradiance + prefiltered cubemaps, we don't have IBL so it's just the sky/ground hemisphere
vec3 hemisphere(vec3 dir)
{
	return mix(groundColour.rgb, skyColour.rgb, dir.y * 0.5 + 0.5);
}

void main()
{
	// CRAIG: original reads a normal map here (calculateNormal), we've got no tangents yet so it's just the vertex normal
	vec3 N = normalize(inNormal);

	vec3 V = normalize(camPos.xyz - inWorldPos);
	vec3 R = reflect(-V, N);

	// CRAIG: original samples separate metallicMap/roughnessMap (.r each), glTF packs both into one texture
	// (roughness in G, metallic in B) and multiplies them by the material's factors
	vec4 metallicRoughness = texture(metallicRoughnessMap, inTexCoord);
	float metallic = pc.metallicFactor * metallicRoughness.b;
	float roughness = pc.roughnessFactor * metallicRoughness.g;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, ALBEDO, metallic);

	// CRAIG: original loops over 4 point lights, we've just got the sun
	vec3 Lo = vec3(0.0);
	vec3 L = normalize(lightDir.xyz);
	Lo += specularContribution(L, V, N, F0, metallic, roughness);

	// CRAIG: no BRDF LUT or prefiltered map, so reflection is the hemisphere along R and the LUT scale/bias is left out
	vec3 reflection = hemisphere(R);
	vec3 irradiance = hemisphere(N);

	// Diffuse based on irradiance
	vec3 diffuse = irradiance * ALBEDO;

	vec3 F = F_SchlickR(max(dot(N, V), 0.0), F0, roughness);

	// Specular reflectance
	vec3 specular = reflection * F;

	// Ambient part
	vec3 kD = 1.0 - F;
	kD *= 1.0 - metallic;
	// CRAIG: no ao map
	vec3 ambient = (kD * diffuse + specular);

	vec3 color = ambient + Lo;

	// CRAIG: tone mapping (Uncharted2Tonemap) and gamma correction are left out
	// no HDR, and the swapchain is sRGB so it does the gamma for us

	// CRAIG: original writes alpha 1, we keep the texture's alpha
	outColor = vec4(color, texture(texSampler, inTexCoord).a * pc.baseColorFactor.a);
}
