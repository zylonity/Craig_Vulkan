#version 450

// Sky dome. The sphere's built from gl_VertexIndex (no vertex buffer) and the atmosphere is worked out per VERTEX,
// the GPU blends the colours across each triangle. Way cheaper than ray marching every pixel
// (Unity's procedural skybox does the same trick)

// atmosphere() and rsi() are from glsl-atmosphere by Rye Terrell (wwwtyro), Unlicense (public domain)
// https://github.com/wwwtyro/glsl-atmosphere/blob/master/index.glsl
// the atmosphere() call follows the example in its README
// Anything changed from the original is marked with "CRAIG:"

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
    vec4 camPos;
};

// Set 0, binding 2 - the sun, same block as FragmentShader.frag
layout(set = 0, binding = 2) uniform LightData
{
    vec4 lightDir;     // xyz points towards the sun
    vec4 lightColour;  // w is intensity
    vec4 skyColour;
    vec4 groundColour;
};

layout(location = 0) out vec3 outSkyColour; // raw, exposure gets done per pixel
layout(location = 1) out vec3 outViewDir;   // for the sun disk

// Has to match kSkyDomeVertexCount in Craig_Renderer.cpp (segments * rings * 6)
#define SEGMENTS 64 // around
#define RINGS 32    // bottom to top

// CRAIG: step counts, cost is iSteps * jSteps per vertex now so it doesn't care about resolution
#define PI 3.141592
#define iSteps 16
#define jSteps 8

vec2 rsi(vec3 r0, vec3 rd, float sr) {
    // ray-sphere intersection that assumes
    // the sphere is centered at the origin.
    // No intersection when result.x > result.y
    float a = dot(rd, rd);
    float b = 2.0 * dot(rd, r0);
    float c = dot(r0, r0) - (sr * sr);
    float d = (b*b) - 4.0*a*c;
    if (d < 0.0) return vec2(1e5,-1e5);
    return vec2(
        (-b - sqrt(d))/(2.0*a),
        (-b + sqrt(d))/(2.0*a)
    );
}

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g) {
    // Normalize the sun and view directions.
    pSun = normalize(pSun);
    r = normalize(r);

    // Calculate the step size of the primary ray.
    vec2 p = rsi(r0, r, rAtmos);
    if (p.x > p.y) return vec3(0,0,0);
    p.y = min(p.y, rsi(r0, r, rPlanet).x);
    float iStepSize = (p.y - p.x) / float(iSteps);

    // Initialize the primary ray time.
    float iTime = 0.0;

    // Initialize accumulators for Rayleigh and Mie scattering.
    vec3 totalRlh = vec3(0,0,0);
    vec3 totalMie = vec3(0,0,0);

    // Initialize optical depth accumulators for the primary ray.
    float iOdRlh = 0.0;
    float iOdMie = 0.0;

    // Calculate the Rayleigh and Mie phases.
    float mu = dot(r, pSun);
    float mumu = mu * mu;
    float gg = g * g;
    float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
    float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

    // Sample the primary ray.
    for (int i = 0; i < iSteps; i++) {

        // Calculate the primary ray sample position.
        vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

        // Calculate the height of the sample.
        float iHeight = length(iPos) - rPlanet;

        // Calculate the optical depth of the Rayleigh and Mie scattering for this step.
        float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
        float odStepMie = exp(-iHeight / shMie) * iStepSize;

        // Accumulate optical depth.
        iOdRlh += odStepRlh;
        iOdMie += odStepMie;

        // Calculate the step size of the secondary ray.
        float jStepSize = rsi(iPos, pSun, rAtmos).y / float(jSteps);

        // Initialize the secondary ray time.
        float jTime = 0.0;

        // Initialize optical depth accumulators for the secondary ray.
        float jOdRlh = 0.0;
        float jOdMie = 0.0;

        // Sample the secondary ray.
        for (int j = 0; j < jSteps; j++) {

            // Calculate the secondary ray sample position.
            vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);

            // Calculate the height of the sample.
            float jHeight = length(jPos) - rPlanet;

            // Accumulate the optical depth.
            jOdRlh += exp(-jHeight / shRlh) * jStepSize;
            jOdMie += exp(-jHeight / shMie) * jStepSize;

            // Increment the secondary ray time.
            jTime += jStepSize;
        }

        // Calculate attenuation.
        vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

        // Accumulate scattering.
        totalRlh += odStepRlh * attn;
        totalMie += odStepMie * attn;

        // Increment the primary ray time.
        iTime += iStepSize;

    }

    // Calculate and return the final color.
    return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}

void main()
{
    // every 6 verts is one quad (2 triangles), work out which quad + which corner this is
    int quad = gl_VertexIndex / 6;
    int corner = gl_VertexIndex % 6;
    const ivec2 corners[6] = ivec2[](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));

    int segment = quad % SEGMENTS + corners[corner].x;
    int ring = quad / SEGMENTS + corners[corner].y;

    float azimuth = float(segment) / float(SEGMENTS) * 2.0 * PI;

    // -1 at the bottom, 1 at the top. Squaring it bunches the rings up near the horizon,
    // that's where sunset colours change fastest so it needs the most verts
    float t = float(ring) / float(RINGS) * 2.0 - 1.0;
    float elevation = sign(t) * t * t * (PI * 0.5);

    vec3 dir = vec3(cos(elevation) * sin(azimuth), sin(elevation), cos(elevation) * cos(azimuth));

    // view without its position so the dome always sits around the camera
    // z = w puts it exactly on the far plane (depth 1), the pipeline's LessOrEqual lets it through
    gl_Position = proj * mat4(mat3(view)) * vec4(dir, 1.0);
    gl_Position.z = gl_Position.w;

    // CRAIG: uSunPos is our sun direction, and the ray origin stays fixed so flying around doesn't move you through the air
    outSkyColour = atmosphere(
        dir,                            // normalized ray direction
        vec3(0,6372e3,0),               // ray origin
        normalize(lightDir.xyz),        // position of the sun
        22.0,                           // intensity of the sun
        6371e3,                         // radius of the planet in meters
        6471e3,                         // radius of the atmosphere in meters
        vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
        21e-6,                          // Mie scattering coefficient
        8e3,                            // Rayleigh scale height
        1.2e3,                          // Mie scale height
        0.758                           // Mie preferred scattering direction
    );
    outViewDir = dir;
}
