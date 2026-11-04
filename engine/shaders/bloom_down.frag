#version 330 core
// Dual-filter downsample: the center and four diagonals, each already a bilinear 2x2 average.
// The first level also keeps only what is brighter than the threshold, with a soft knee.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 texelSize;      // of the source
uniform bool prefilter;
uniform float threshold;
uniform float knee;

vec3 fetch(vec2 uv)
{
    vec3 c = max(texture(screenTexture, uv).rgb, vec3(0.0));
    if (prefilter) {
        float brightness = max(c.r, max(c.g, c.b));
        float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
        soft = soft * soft / (4.0 * knee + 1e-4);
        c *= max(soft, brightness - threshold) / max(brightness, 1e-4);
    }
    return c;
}

void main()
{
    vec3 sum = fetch(TexCoords) * 4.0;
    sum += fetch(TexCoords + texelSize * vec2(-1.0, -1.0));
    sum += fetch(TexCoords + texelSize * vec2( 1.0, -1.0));
    sum += fetch(TexCoords + texelSize * vec2(-1.0,  1.0));
    sum += fetch(TexCoords + texelSize * vec2( 1.0,  1.0));
    FragColor = vec4(sum / 8.0, 1.0);
}
