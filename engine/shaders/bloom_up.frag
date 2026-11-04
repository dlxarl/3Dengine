#version 330 core
// Tent upsample, added on top of the larger level by blending.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 texelSize;      // of the source
uniform float spread;
uniform float weight;

void main()
{
    vec2 o = texelSize * spread;
    vec3 sum = texture(screenTexture, TexCoords).rgb * 4.0;
    sum += texture(screenTexture, TexCoords + vec2(-o.x, 0.0)).rgb * 2.0;
    sum += texture(screenTexture, TexCoords + vec2( o.x, 0.0)).rgb * 2.0;
    sum += texture(screenTexture, TexCoords + vec2(0.0, -o.y)).rgb * 2.0;
    sum += texture(screenTexture, TexCoords + vec2(0.0,  o.y)).rgb * 2.0;
    sum += texture(screenTexture, TexCoords + vec2(-o.x, -o.y)).rgb;
    sum += texture(screenTexture, TexCoords + vec2( o.x, -o.y)).rgb;
    sum += texture(screenTexture, TexCoords + vec2(-o.x,  o.y)).rgb;
    sum += texture(screenTexture, TexCoords + vec2( o.x,  o.y)).rgb;
    FragColor = vec4(sum / 16.0 * weight, 1.0);
}
