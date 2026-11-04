#version 330 core
// Depth of field: the circle of confusion grows with the distance from the focal zone, and
// each pixel gathers a disc of neighbours that are blurred enough to reach it. A sharp pixel
// still collects blurred foreground, so near objects bleed over what is behind them.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform float nearPlane;
uniform float farPlane;
uniform float focalDistance;
uniform float focalRange;
uniform float maxBlurRadius;   // pixels
uniform vec2 texelSize;

const int TAPS = 32;
const float GOLDEN_ANGLE = 2.39996323;

float linearDepth(float depth)
{
    return 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (depth * 2.0 - 1.0) * (farPlane - nearPlane));
}

// Blur radius in pixels, from 0 in focus to maxBlurRadius.
float confusion(vec2 uv)
{
    float depth = texture(depthTexture, uv).r;
    float distance = depth >= 1.0 ? farPlane : linearDepth(depth);
    float outside = max(abs(distance - focalDistance) - focalRange * 0.5, 0.0);
    return clamp(outside / max(focalRange, 0.01), 0.0, 1.0) * maxBlurRadius;
}

void main()
{
    float centerRadius = confusion(TexCoords);
    float centerDepth = linearDepth(texture(depthTexture, TexCoords).r);

    vec3 sum = texture(screenTexture, TexCoords).rgb;
    float total = 1.0;

    for (int i = 0; i < TAPS; i++) {
        float r = sqrt((float(i) + 0.5) / float(TAPS)) * maxBlurRadius;
        float a = float(i) * GOLDEN_ANGLE;
        vec2 uv = TexCoords + vec2(cos(a), sin(a)) * r * texelSize;
        float tapRadius = confusion(uv);
        float tapDepth = linearDepth(texture(depthTexture, uv).r);

        // A tap reaches this pixel when its own blur radius covers the distance between them.
        float reach = smoothstep(r - 1.0, r + 1.0, tapRadius);
        // What lies behind a sharper pixel must not smear over it.
        float behind = tapDepth > centerDepth ? smoothstep(0.0, 1.0, centerRadius / max(maxBlurRadius * 0.25, 1e-3)) : 1.0;
        float w = reach * behind;
        sum += texture(screenTexture, uv).rgb * w;
        total += w;
    }

    FragColor = vec4(sum / total, 1.0);
}
