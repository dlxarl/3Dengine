#version 330 core
// Screen-space ambient occlusion: a hemisphere of samples around each pixel, tipped to the
// surface normal and turned by a 4x4 noise tile, counted where the depth buffer is in front.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D depthTexture;
uniform sampler2D normalTexture;
uniform sampler2D noiseTexture;

uniform mat4 projection;
uniform mat4 invProjection;
uniform mat4 view;

const int MAX_KERNEL = 32;
uniform vec3 kernel[MAX_KERNEL];
uniform int sampleCount;
uniform float radius;
uniform float bias;
uniform vec2 noiseScale;

vec3 viewPosition(vec2 uv, float depth)
{
    vec4 p = invProjection * vec4(vec3(uv, depth) * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

void main()
{
    float depth = texture(depthTexture, TexCoords).r;
    if (depth >= 1.0) {
        FragColor = vec4(1.0);
        return;
    }

    vec3 position = viewPosition(TexCoords, depth);

    // The normal buffer is world space; an alpha of zero marks a pixel that did not write one.
    vec4 stored = texture(normalTexture, TexCoords);
    vec3 normal;
    if (stored.a > 0.5) {
        normal = normalize(mat3(view) * (stored.xyz * 2.0 - 1.0));
    } else {
        normal = normalize(cross(dFdx(position), dFdy(position)));
        if (dot(normal, position) > 0.0) normal = -normal;
    }

    vec3 random = vec3(texture(noiseTexture, TexCoords * noiseScale).xy, 0.0);
    vec3 tangent = normalize(random - normal * dot(random, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 tbn = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < sampleCount; i++) {
        vec3 samplePosition = position + tbn * kernel[i] * radius;
        vec4 projected = projection * vec4(samplePosition, 1.0);
        vec2 uv = projected.xy / projected.w * 0.5 + 0.5;
        float sampleDepth = texture(depthTexture, uv).r;
        float sceneZ = viewPosition(uv, sampleDepth).z;
        // Surfaces far in front of the pixel are not occluding it, only nearby geometry.
        float range = smoothstep(0.0, 1.0, radius / max(abs(position.z - sceneZ), 1e-4));
        occlusion += (sceneZ >= samplePosition.z + bias ? 1.0 : 0.0) * range;
    }

    FragColor = vec4(vec3(1.0 - occlusion / float(sampleCount)), 1.0);
}
