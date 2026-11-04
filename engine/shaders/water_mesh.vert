#version 330 core
// The surface of a water mesh (a river): the mesh itself, lifted by the same waves as a Water
// object. The waves depend only on where a point is in the world, so neighbouring pieces meet
// without a seam.
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float time;
uniform float waveHeight;
uniform float waveLength;
uniform float waveSpeed;
uniform vec2 flow;

out vec3 WorldPos;
out vec3 WorldNormal;
out vec2 Local;
out float Crest;

const int WAVES = 4;
const vec2 DIRECTIONS[WAVES] = vec2[WAVES](vec2(1.0, 0.2), vec2(-0.4, 1.0), vec2(0.7, -0.8), vec2(-1.0, -0.5));
const float SCALES[WAVES] = float[WAVES](1.0, 0.63, 0.41, 0.27);
const float AMPLITUDES[WAVES] = float[WAVES](1.0, 0.55, 0.3, 0.18);

void main()
{
    vec3 world = (model * vec4(aPos, 1.0)).xyz;
    float k0 = 6.2831853 / max(waveLength, 0.1);

    float height = 0.0;
    vec2 slope = vec2(0.0);
    for (int i = 0; i < WAVES; i++) {
        vec2 dir = normalize(DIRECTIONS[i]);
        float k = k0 / SCALES[i];
        float phase = dot(dir, world.xz) * k + time * waveSpeed * (1.0 + 0.3 * float(i)) * sqrt(k * 9.8) * 0.5 - dot(flow, dir) * k * time;
        height += sin(phase) * AMPLITUDES[i];
        slope += dir * cos(phase) * AMPLITUDES[i] * k;
    }
    height *= waveHeight * 0.4;
    slope *= waveHeight * 0.4;

    WorldPos = vec3(world.x, world.y + height, world.z);
    WorldNormal = normalize(vec3(-slope.x, 1.0, -slope.y));
    Local = vec2(0.0);
    Crest = height / max(waveHeight * 0.4, 1e-4);
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
