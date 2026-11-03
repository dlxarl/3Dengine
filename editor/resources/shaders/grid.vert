#version 330 core
out vec3 NearPoint;
out vec3 FarPoint;

uniform mat4 inverseViewProjection;

vec3 unproject(vec2 ndc, float depth)
{
    vec4 point = inverseViewProjection * vec4(ndc, depth, 1.0);
    return point.xyz / point.w;
}

void main()
{
    vec2 ndc = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    NearPoint = unproject(ndc, -1.0);
    FarPoint = unproject(ndc, 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
