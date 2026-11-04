#version 330 core
// A sky behind everything: the fog color at the horizon deepening overhead, drifting clouds,
// the sun or the moon where the light comes from, and stars once it is dark.
in vec2 Screen;
out vec4 FragColor;

uniform mat4 inverseViewProjection;  // without the camera's position
uniform vec3 horizonColor;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform float time;

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 5; i++) {
        value += amplitude * noise(p);
        p = p * 2.03 + vec2(17.1, 9.2);
        amplitude *= 0.5;
    }
    return value;
}

// The same as lighting.frag's, so that fogged land meets the sky without a seam.
vec3 scattering(vec3 direction, vec3 horizon)
{
    float towards = max(dot(direction, -lightDirection), 0.0);
    float day = smoothstep(0.12, 0.3, dot(horizon, vec3(0.299, 0.587, 0.114)));
    return lightColor * (pow(towards, 8.0) * 0.35 + pow(towards, 64.0) * 0.4) * (0.3 + 0.7 * day);
}

void main()
{
    vec4 far = inverseViewProjection * vec4(Screen, 1.0, 1.0);
    vec3 direction = normalize(far.xyz / far.w);
    float up = direction.y;

    float brightness = dot(horizonColor, vec3(0.299, 0.587, 0.114));
    float day = smoothstep(0.12, 0.3, brightness);

    vec3 zenith = horizonColor * mix(vec3(0.35, 0.42, 0.75), vec3(0.58, 0.64, 0.78), day);
    vec3 sky = mix(horizonColor, zenith, smoothstep(0.0, 0.65, up));
    sky += scattering(direction, horizonColor);

    // The disc of the sun, or a paler moon at night.
    float towards = dot(direction, -lightDirection);
    float disc = smoothstep(0.9986, 0.9993, towards);
    vec3 discColor = mix(vec3(0.82, 0.88, 1.0), lightColor * 1.4 + vec3(0.25), day);

    float cover = 0.0;
    if (up > 0.0) {
        // Clouds on a plane overhead, thinning into the haze towards the horizon.
        vec2 uv = direction.xz / (up + 0.12) * 1.3 + vec2(time * 0.012, time * 0.005);
        float n = fbm(uv);
        cover = smoothstep(0.22, 0.55, n) * smoothstep(0.0, 0.18, up);
        vec3 lit = horizonColor * 0.95 + lightColor * 0.10 * (0.3 + 0.7 * day);
        vec3 shade = horizonColor * mix(0.55, 0.50, day);
        vec3 cloud = mix(lit, shade, smoothstep(0.5, 0.9, n));
        sky = mix(sky, cloud, cover * 0.96);

        // Stars, where the clouds let them through.
        vec2 cell = floor(direction.xz / (up + 0.35) * 140.0);
        float star = step(0.9965, hash(cell)) * (1.0 - day) * smoothstep(0.05, 0.35, up) * (1.0 - cover);
        sky += vec3(0.75, 0.8, 0.9) * star * (0.6 + 0.4 * sin(time * 3.0 + hash(cell + 3.0) * 40.0));
    }

    sky = mix(sky, discColor, disc * (1.0 - cover * 0.85));
    FragColor = vec4(sky, 1.0);
}
