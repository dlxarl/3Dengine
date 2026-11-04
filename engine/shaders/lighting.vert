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

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform bool directionalLight;
uniform vec3 lightDirection;

uniform float time;
uniform float waveHeight;
uniform float sway;

// Fires, lanterns and lit windows: position and range, and color.
const int MAX_POINT_LIGHTS = 8;
uniform int pointLightCount;
uniform vec4 pointLights[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
const float POINT_LIGHT_STRENGTH = 1.8;   // a fire at night outshines the moon

// Retro rendering
uniform bool vertexLighting;
uniform vec2 vertexGrid; // snapping grid in cells per screen; zero disables it

// Sunlight reaches a little way round a surface, so the side away from it is not flat black.
const float SUN_WRAP = 0.15;

vec3 pointLighting(vec3 position, vec3 normal)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < pointLightCount; i++) {
        vec3 offset = pointLights[i].xyz - position;
        float distance = length(offset);
        float reach = distance / pointLights[i].w;
        float fade = clamp(1.0 - reach * reach, 0.0, 1.0);
        float flicker = 0.88 + 0.12 * sin(time * 9.0 + float(i) * 2.3) * sin(time * 5.7 + float(i));
        float facing = max(dot(normal, offset / max(distance, 0.001)), 0.0) * 0.75 + 0.25;
        total += pointLightColors[i] * POINT_LIGHT_STRENGTH * fade * fade * facing * flicker;
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
    if (vertexLighting) {
        vec3 L = normalize(lightPos - FragPos);
        vec3 V = normalize(viewPos - FragPos);

        float dist = length(lightPos - FragPos);
        float attenuation = 1.0 / (1.0 + 0.02 * dist + 0.001 * dist * dist);
        float facing = max(dot(Normal, L), 0.0);
        if (directionalLight) {
            L = -lightDirection;
            attenuation = 1.0;
            facing = max((dot(Normal, L) + SUN_WRAP) / (1.0 + SUN_WRAP), 0.0);
        }
        vec3 R = reflect(-L, Normal);

        VertexDiffuse = attenuation * facing * lightColor;
        VertexSpecular = attenuation * 0.5 * pow(max(dot(R, V), 0.0), 32.0) * lightColor;
        VertexPoint = pointLighting(FragPos, Normal);
    } else {
        VertexDiffuse = vec3(0.0);
        VertexSpecular = vec3(0.0);
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
