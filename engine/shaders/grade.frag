#version 330 core
// Bloom, exposure, white balance, contrast and color filter in the HDR range, then the tone
// curve into the displayable one, then split toning.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D bloomTexture;
uniform bool useBloom;
uniform float bloomIntensity;

uniform int toneMapper;          // 0 none, 1 Reinhard, 2 Neutral, 3 ACES
uniform bool useAdjustments;
uniform float exposure;          // already 2^stops
uniform float contrast;          // 1 leaves it
uniform vec3 colorFilter;
uniform bool useWhiteBalance;
uniform float temperature;
uniform float tint;
uniform bool useSplitToning;
uniform vec3 shadowsTint;
uniform vec3 highlightsTint;
uniform float splitBalance;

const vec3 LUMA = vec3(0.299, 0.587, 0.114);

vec3 reinhard(vec3 c)
{
    return c / (1.0 + c);
}

// Khronos PBR Neutral: leaves most of the range alone and rolls off only the highlights.
vec3 neutral(vec3 color)
{
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;
    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;
    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;
    float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;
    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, vec3(newPeak), g);
}

// The ACES filmic curve, fitted by Krzysztof Narkowicz, with the usual exposure bias.
vec3 aces(vec3 x)
{
    x *= 0.6;
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

// Pushes toward a tint that is not mid grey: a soft overlay, strongest in the midtones.
vec3 tone(vec3 color, vec3 tintColor, float amount)
{
    return color + (tintColor - 0.5) * 2.0 * color * (1.0 - color) * 2.0 * amount;
}

void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;
    if (useBloom) color += texture(bloomTexture, TexCoords).rgb * bloomIntensity;

    if (useAdjustments) {
        color *= exposure;
        color = max((color - 0.18) * contrast + 0.18, 0.0);
        color *= colorFilter;
    }
    if (useWhiteBalance) {
        // Warm moves red up and blue down; magenta pulls green down.
        color *= vec3(1.0 + 0.25 * temperature, 1.0 - 0.25 * tint, 1.0 - 0.25 * temperature);
    }

    if (toneMapper == 1) color = reinhard(color);
    else if (toneMapper == 2) color = neutral(color);
    else if (toneMapper == 3) color = aces(color);
    color = clamp(color, 0.0, 1.0);

    if (useSplitToning) {
        float t = clamp(dot(color, LUMA) + splitBalance * 0.5, 0.0, 1.0);
        color = tone(color, shadowsTint, 1.0 - t);
        color = tone(color, highlightsTint, t);
        color = clamp(color, 0.0, 1.0);
    }

    FragColor = vec4(color, 1.0);
}
