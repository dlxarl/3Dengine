#version 330 core
in vec2 Corner;
in vec4 Color;
in float Seed;
in float ViewDistance;
out vec4 FragColor;

uniform float softness;   // 0 hard edge, 1 fades from the middle
uniform float flicker;    // noise eating into the edge
uniform float time;
uniform bool additive;
uniform vec2 fogRange;
uniform vec3 fogColor;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

void main()
{
    float r = length(Corner);
    if (r >= 1.0) discard;

    float edge = 1.0 - r;
    float shape = mix(smoothstep(0.0, 0.08, edge), edge * edge * (3.0 - 2.0 * edge), softness);
    if (flicker > 0.0) {
        float n = noise(Corner * 3.0 + vec2(Seed * 50.0, time * 3.0 + Seed * 20.0));
        shape *= mix(1.0, smoothstep(0.15, 0.85, n + edge * 0.5), flicker);
    }

    float alpha = Color.a * shape;
    if (fogRange.y > 0.0) alpha *= 1.0 - smoothstep(fogRange.x, fogRange.y, ViewDistance);
    if (alpha < 0.003) discard;

    // Additive light adds its color; smoke covers what is behind it.
    FragColor = additive ? vec4(Color.rgb * alpha, 0.0) : vec4(Color.rgb, alpha);
}
