#version 330 core
// One triangle that covers the screen, with no vertex data.
out vec2 Screen;

void main()
{
    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    Screen = corner * 2.0 - 1.0;
    gl_Position = vec4(Screen, 1.0, 1.0);
}
