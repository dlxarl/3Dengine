#version 330 core
in vec3 WorldPos;
in vec3 WorldNormal;
in vec2 Local;
in float Crest;
out vec4 FragColor;

uniform vec3 eye;
uniform vec3 shallowColor;
uniform vec3 deepColor;
uniform float opacity;
uniform bool isRound;
uniform bool fromAbove;          // a mesh has no reliable winding: the eye decides which side is seen
uniform float time;
uniform vec2 flow;
uniform vec3 lightDirection;     // the way the light travels
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 horizonColor;       // the sky at the horizon
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
    if (isRound && dot(Local, Local) > 1.0) discard;

    vec3 toEye = eye - WorldPos;
    float distanceToEye = length(toEye);
    vec3 V = toEye / distanceToEye;

    // Small ripples drifting with the current, on top of the big waves.
    vec2 p = WorldPos.xz * 1.7 - flow * time * 0.5;
    float a = noise(p + time * 0.35), b = noise(p * 2.3 - time * 0.5 + 7.0);
    vec3 N = normalize(WorldNormal + vec3(a - 0.5, 0.0, b - 0.5) * 0.28);

    vec3 sunDir = normalize(-lightDirection);
    float daylight = clamp(length(lightColor), 0.0, 1.5);
    vec3 lit = ambientColor + lightColor * (0.4 + 0.6 * max(sunDir.y, 0.0));

    vec4 result;
    if (fromAbove ? eye.y >= WorldPos.y : gl_FrontFacing) {
        // From above: the sky on the surface, more at a glancing angle, over the color of the
        // water, which is deeper where one looks straight down.
        vec3 R = reflect(-V, N);
        float fresnel = 0.04 + 0.96 * pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 5.0);
        vec3 sky = mix(horizonColor, horizonColor * vec3(0.55, 0.7, 0.95) + vec3(0.02, 0.04, 0.08), smoothstep(0.0, 0.6, R.y));
        float lookDown = clamp(dot(N, V), 0.0, 1.0);
        vec3 body = mix(shallowColor, deepColor, pow(lookDown, 0.6)) * lit;

        vec3 color = mix(body, sky, fresnel);
        float glint = pow(max(dot(R, sunDir), 0.0), 220.0) * (1.0 - step(sunDir.y, 0.0));
        color += lightColor * glint * 2.5;
        color += vec3(0.05, 0.07, 0.07) * smoothstep(0.55, 1.0, Crest) * daylight;   // pale tops of the waves

        result = vec4(color, clamp(opacity + fresnel * (1.0 - opacity), 0.0, 1.0));
    } else {
        // From below: a dim ceiling of rippling light, with a bright window straight overhead.
        float overhead = clamp(dot(vec3(0.0, -1.0, 0.0), -V), 0.0, 1.0);
        float ripple = 0.75 + 0.5 * a;
        vec3 color = mix(deepColor * 1.4, shallowColor * 1.8 * ripple, pow(overhead, 3.0)) * (0.5 + 0.5 * daylight);
        result = vec4(color, 0.72);
    }

    float fog = fogRange.y > 0.0 ? smoothstep(fogRange.x, fogRange.y, distanceToEye) : 0.0;
    result.rgb = mix(result.rgb, fogColor, fog);
    FragColor = result;
}
