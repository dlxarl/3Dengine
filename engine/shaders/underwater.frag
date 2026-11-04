#version 330 core
// What the world looks like from under the surface: red light is absorbed first, distance
// fades into the color of the water, the picture wobbles, and light shimmers from above.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform float nearPlane;
uniform float farPlane;
uniform float time;
uniform vec3 tint;          // the color of the water at a distance
uniform float density;      // 1 is the usual murk
uniform float depth;        // how far below the surface the camera is

float linearDepth(float d)
{
    return 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (d * 2.0 - 1.0) * (farPlane - nearPlane));
}

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
    vec2 uv = TexCoords;
    uv += vec2(sin(uv.y * 28.0 + time * 1.9), cos(uv.x * 24.0 + time * 1.6)) * 0.0035;

    vec3 color = texture(screenTexture, uv).rgb;
    float d = texture(depthTexture, uv).r;
    float distanceToScene = d >= 1.0 ? farPlane : linearDepth(d);

    // Absorption: red goes first, then green.
    vec3 absorb = vec3(0.34, 0.11, 0.06) * density;
    color *= exp(-absorb * distanceToScene);

    // The murk in between.
    float murk = 1.0 - exp(-distanceToScene * 0.045 * density);
    vec3 water = tint * exp(-depth * 0.045 * density);
    color = mix(color, water, clamp(murk, 0.0, 1.0));

    // Rays of light from the surface, strongest near the top of the picture and near the surface.
    float rays = noise(vec2(uv.x * 9.0 + sin(time * 0.4) * 0.6, time * 0.25)) * noise(vec2(uv.x * 4.0 - time * 0.15, uv.y * 2.0));
    float fade = smoothstep(0.0, 1.0, uv.y) * exp(-depth * 0.12);
    color += tint * 1.6 * rays * fade * 0.5;

    // Deeper is darker, and the edges close in a little.
    color *= exp(-depth * 0.03 * density);
    vec2 edge = uv - 0.5;
    color *= 1.0 - dot(edge, edge) * 0.55;

    FragColor = vec4(color, 1.0);
}
