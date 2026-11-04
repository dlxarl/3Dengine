#version 330 core
// Camera motion blur: each pixel's world position is reprojected with last frame's camera, and
// the image is sampled along that screen-space velocity.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform sampler2D depthTexture;
uniform mat4 inverseViewProjection;
uniform mat4 previousViewProjection;
uniform float fps;
uniform float strength;

void main()
{
    vec2 uv = TexCoords;
    vec3 color = texture(screenTexture, uv).rgb;

    float z = texture(depthTexture, uv).r * 2.0 - 1.0;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 worldPosition = inverseViewProjection * clipSpacePosition;
    worldPosition /= worldPosition.w;

    vec4 previousClipSpacePosition = previousViewProjection * worldPosition;
    previousClipSpacePosition /= previousClipSpacePosition.w;

    vec2 velocity = (clipSpacePosition.xy - previousClipSpacePosition.xy) / 2.0;

    float currentFPS = fps > 0.0 ? fps : 60.0;
    velocity *= 0.05 * (60.0 / currentFPS) * strength;

    if (length(velocity) < 0.0005) {
        FragColor = vec4(color, 1.0);
        return;
    }

    float maxVelocity = 0.005 * max(strength, 1.0);
    if (length(velocity) > maxVelocity) velocity = normalize(velocity) * maxVelocity;

    const int numSamples = 8;
    vec2 texCoord = uv;
    for (int i = 1; i < numSamples; ++i) {
        texCoord += velocity;
        if (texCoord.x >= 0.0 && texCoord.x <= 1.0 && texCoord.y >= 0.0 && texCoord.y <= 1.0)
            color += texture(screenTexture, texCoord).rgb;
        else
            color += texture(screenTexture, uv).rgb;
    }
    FragColor = vec4(color / float(numSamples), 1.0);
}
