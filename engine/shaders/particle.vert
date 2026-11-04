#version 330 core
// One instance per particle; the four corners of its quad come from the vertex number.
layout (location = 0) in vec4 aPositionSize;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aRotationSeed;

uniform mat4 view;
uniform mat4 projection;

out vec2 Corner;
out vec4 Color;
out float Seed;
out float ViewDistance;

void main()
{
    vec2 corner = vec2(float(gl_VertexID & 1), float((gl_VertexID >> 1) & 1)) * 2.0 - 1.0;
    float s = sin(aRotationSeed.x), c = cos(aRotationSeed.x);
    vec2 rotated = vec2(c * corner.x - s * corner.y, s * corner.x + c * corner.y);

    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 world = aPositionSize.xyz + (right * rotated.x + up * rotated.y) * aPositionSize.w;

    vec4 viewPosition = view * vec4(world, 1.0);
    ViewDistance = length(viewPosition.xyz);
    Corner = corner;
    Color = aColor;
    Seed = aRotationSeed.y;
    gl_Position = projection * viewPosition;
}
