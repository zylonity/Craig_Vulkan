#version 450

// Fullscreen triangle for the sky, no vertex buffer. 3 verts make one big triangle that covers the whole screen
// (cheaper than a quad, no seam down the middle)

layout(set = 0, binding = 0) uniform CameraData
{
    mat4 view;
    mat4 proj;
    vec4 camPos;
};

// world space direction from the camera through this pixel, the frag normalises it
layout(location = 0) out vec3 outViewDir;

void main()
{
    // 0 -> (0,0), 1 -> (2,0), 2 -> (0,2)
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 ndc = uv * 2.0 - 1.0;

    // z = 1 is the far plane, the pipeline uses LessOrEqual so it only shows where nothing else drew
    gl_Position = vec4(ndc, 1.0, 1.0);

    // un-project the far plane corner back into the world, then point at it from the camera
    // fine to do per vertex, it interpolates linearly across the screen
    vec4 farPoint = inverse(proj * view) * vec4(ndc, 1.0, 1.0);
    outViewDir = farPoint.xyz / farPoint.w - camPos.xyz;
}
