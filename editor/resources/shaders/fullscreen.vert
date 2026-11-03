#version 330 core
out vec2 TexCoords;

// One triangle covering the screen, with no vertex buffer.
void main()
{
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    TexCoords = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
