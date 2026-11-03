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

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

// Retro rendering
uniform bool vertexLighting;
uniform vec2 vertexGrid; // snapping grid in cells per screen; zero disables it

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(transpose(inverse(mat3(model))) * aNormal);
    TexCoords = aTexCoords;
    AffineTexCoords = aTexCoords;

    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    if (vertexLighting) {
        vec3 L = normalize(lightPos - FragPos);
        vec3 V = normalize(viewPos - FragPos);
        vec3 R = reflect(-L, Normal);

        float dist = length(lightPos - FragPos);
        float attenuation = 1.0 / (1.0 + 0.02 * dist + 0.001 * dist * dist);

        VertexDiffuse = attenuation * max(dot(Normal, L), 0.0) * lightColor;
        VertexSpecular = attenuation * 0.5 * pow(max(dot(R, V), 0.0), 32.0) * lightColor;
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
