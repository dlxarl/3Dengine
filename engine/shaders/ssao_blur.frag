#version 330 core
// Blurs the occlusion over the noise tile without crossing depth edges, and multiplies it into the image.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D depthTexture;
uniform vec2 texelSize;
uniform float nearPlane;
uniform float farPlane;
uniform float intensity;

float linearDepth(float depth)
{
    return 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (depth * 2.0 - 1.0) * (farPlane - nearPlane));
}

void main()
{
    float centerDepth = linearDepth(texture(depthTexture, TexCoords).r);
    float sum = 0.0;
    float total = 0.0;
    for (int x = -2; x < 2; x++) {
        for (int y = -2; y < 2; y++) {
            vec2 uv = TexCoords + (vec2(x, y) + 0.5) * texelSize;
            float d = linearDepth(texture(depthTexture, uv).r);
            float w = exp(-abs(d - centerDepth) / (0.03 * centerDepth + 0.05));
            sum += texture(occlusionTexture, uv).r * w;
            total += w;
        }
    }
    float ao = sum / max(total, 1e-4);
    vec3 color = texture(screenTexture, TexCoords).rgb;
    FragColor = vec4(color * pow(clamp(ao, 0.0, 1.0), intensity * 2.0), 1.0);
}
