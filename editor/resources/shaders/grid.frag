#version 330 core
in vec3 NearPoint;
in vec3 FarPoint;
out vec4 FragColor;

uniform mat4 viewProjection;
uniform vec3 cameraPos;
uniform float fadeDistance;

float lineStrength(vec2 coord, float spacing)
{
    vec2 scaled = coord / spacing;
    vec2 width = fwidth(scaled);
    vec2 grid = abs(fract(scaled - 0.5) - 0.5) / width;
    return 1.0 - min(min(grid.x, grid.y), 1.0);
}

void main()
{
    // Where this pixel's view ray meets the ground plane.
    float t = -NearPoint.y / (FarPoint.y - NearPoint.y);
    if (t <= 0.0 || t > 1.0)
        discard;

    vec3 point = NearPoint + t * (FarPoint - NearPoint);

    vec4 clip = viewProjection * vec4(point, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;

    float minor = lineStrength(point.xz, 1.0);
    float major = lineStrength(point.xz, 10.0);

    vec3 color = vec3(0.56, 0.58, 0.62);
    float alpha = max(minor * 0.28, major * 0.5);

    // The world axes, in the same muted tints as the Inspector.
    vec2 axisWidth = fwidth(point.xz);
    if (abs(point.z) < axisWidth.y) {
        color = vec3(0.79, 0.38, 0.35);
        alpha = 0.9;
    } else if (abs(point.x) < axisWidth.x) {
        color = vec3(0.36, 0.56, 0.79);
        alpha = 0.9;
    }

    float fade = 1.0 - smoothstep(fadeDistance * 0.35, fadeDistance, distance(point, cameraPos));
    alpha *= fade;

    if (alpha <= 0.002)
        discard;

    FragColor = vec4(color, alpha);
}
