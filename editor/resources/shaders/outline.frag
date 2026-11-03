#version 330 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D mask;
uniform vec3 outlineColor;
uniform float radius;

// Paints only just outside the selected silhouette, so the object itself stays untouched.
void main()
{
    if (texture(mask, TexCoords).r > 0.5)
        discard;

    vec2 texel = radius / vec2(textureSize(mask, 0));
    float neighbours = 0.0;
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            neighbours = max(neighbours, texture(mask, TexCoords + vec2(x, y) * texel).r);
        }
    }

    if (neighbours < 0.5)
        discard;

    FragColor = vec4(outlineColor, 1.0);
}
