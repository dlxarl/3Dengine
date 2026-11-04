#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
noperspective out vec2 AffineTexCoords;
out vec4 FragPosLightSpace;
out vec3 VertexDiffuse;
out vec3 VertexSpecular;
out vec3 VertexPoint;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform vec3 viewPos;
uniform vec3 lightColor;
uniform bool directionalLight;
uniform vec3 lightDirection;

uniform float time;
uniform float waveHeight;
uniform float sway;

// The lights besides the sun; see lighting.frag.
const int MAX_LIGHTS = 16;
uniform int lightCount;
uniform vec4 lightPositions[MAX_LIGHTS];
uniform vec4 lightDirections[MAX_LIGHTS];
uniform vec4 lightColors[MAX_LIGHTS];
uniform vec4 lightSpots[MAX_LIGHTS];

// Retro rendering
uniform bool vertexLighting;
uniform vec2 vertexGrid; // snapping grid in cells per screen; zero disables it

// Sunlight reaches a little way round a surface, so the side away from it is not flat black.
const float SUN_WRAP = 0.15;

float rangeFade(float distance, float range)
{
    float reach = distance / range;
    float fade = clamp(1.0 - reach * reach, 0.0, 1.0);
    return fade * fade;
}

vec3 vertexLights(vec3 position, vec3 normal)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec3 toLight;
        float strength = 1.0;
        if (lightDirections[i].w < 0.5) {
            toLight = -lightDirections[i].xyz;
        } else {
            vec3 offset = lightPositions[i].xyz - position;
            float distance = length(offset);
            toLight = offset / max(distance, 0.001);
            strength = rangeFade(distance, lightPositions[i].w);
            if (lightDirections[i].w > 1.5)
                strength *= smoothstep(lightSpots[i].x, lightSpots[i].y, dot(-toLight, lightDirections[i].xyz));
        }
        total += lightColors[i].rgb * strength * (max(dot(normal, toLight), 0.0) * 0.75 + 0.25 * step(0.001, strength));
    }
    return total;
}

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(transpose(inverse(mat3(model))) * aNormal);

    if (waveHeight > 0.0) {
        float a = FragPos.x * 0.55 + time * 1.4;
        float b = FragPos.z * 0.45 + time * 1.1;
        FragPos.y += waveHeight * sin(a) * cos(b);
        // The slope of the swell, exaggerated, so that light glints off it.
        vec2 slope = waveHeight * vec2(0.55 * cos(a) * cos(b), -0.45 * sin(a) * sin(b)) * 3.0;
        Normal = normalize(vec3(-slope.x, 1.0, -slope.y));
    }

    if (sway > 0.0) {
        // Bends more the higher up it is, each object in its own time.
        float height = max(aPos.y, 0.0);
        float phase = model[3].x * 0.37 + model[3].z * 0.23;
        float gust = 0.6 + 0.4 * sin(time * 0.31 + phase * 0.1);
        vec2 bend = vec2(sin(time * 1.3 + phase), cos(time * 1.05 + phase * 1.3)) * sway * gust * 0.02 * pow(height, 1.5);
        FragPos.xz += bend;
    }

    TexCoords = aTexCoords;
    AffineTexCoords = aTexCoords;

    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    VertexPoint = vec3(0.0);
    VertexDiffuse = vec3(0.0);
    VertexSpecular = vec3(0.0);
    if (vertexLighting) {
        vec3 V = normalize(viewPos - FragPos);
        if (directionalLight) {
            vec3 L = -lightDirection;
            float facing = max((dot(Normal, L) + SUN_WRAP) / (1.0 + SUN_WRAP), 0.0);
            vec3 R = reflect(-L, Normal);
            VertexDiffuse = facing * lightColor;
            VertexSpecular = 0.5 * pow(max(dot(R, V), 0.0), 32.0) * lightColor;
        }
        VertexPoint = vertexLights(FragPos, Normal);
    }

    vec4 clipPos = projection * view * vec4(FragPos, 1.0);

    // Without sub-pixel precision, vertices wobble between integer screen positions.
    if (vertexGrid.y > 0.0 && clipPos.w > 0.0) {
        vec2 ndc = clipPos.xy / clipPos.w;
        ndc = floor(ndc * vertexGrid * 0.5 + 0.5) / (vertexGrid * 0.5);
        clipPos.xy = ndc * clipPos.w;
    }

    gl_Position = clipPos;
}
