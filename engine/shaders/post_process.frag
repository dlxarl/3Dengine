#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

uniform float colorLevels;
uniform bool dither;
uniform vec2 internalSize;
uniform float vignette;
uniform float vignetteSmoothness;  // 0.5 is the classic falloff
uniform float vignetteRoundness;   // 0 follows the screen's shape, 1 is a circle
uniform float aspect;
uniform float distortion;          // lens: negative barrel, positive pincushion
uniform float saturation;

uniform float scanlines;
uniform float crtCurvature;
uniform float chromaticAberration;
uniform float filmGrain;
uniform float bloom;
uniform float time;

const float bayer4x4[16] = float[16](
     0.0,  8.0,  2.0, 10.0,
    12.0,  4.0, 14.0,  6.0,
     3.0, 11.0,  1.0,  9.0,
    15.0,  7.0, 13.0,  5.0
);

// Curved cathode-ray tube screen distortion
vec2 curveUV(vec2 uv, float bend)
{
    vec2 p = uv * 2.0 - 1.0;
    p += p * dot(p, p) * (bend * 0.15);
    return p * 0.5 + 0.5;
}

// Samples the scene with radial chromatic aberration towards the screen edges
vec3 sampleScreen(vec2 coord)
{
    if (chromaticAberration > 0.0) {
        vec2 dir = coord - 0.5;
        float dist = dot(dir, dir);
        vec2 offset = dir * dist * (chromaticAberration * 0.025);
        float r = texture(screenTexture, coord + offset).r;
        float g = texture(screenTexture, coord).g;
        float b = texture(screenTexture, coord - offset).b;
        return vec3(r, g, b);
    }
    return texture(screenTexture, coord).rgb;
}

// Color strength and the darkened corners, before the color depth is reduced.
vec3 grade(vec3 color, vec2 coords)
{
    // Left alone at the defaults, so that an image without grading is exactly as rendered.
    if (saturation != 1.0) {
        float luma = dot(color, vec3(0.299, 0.587, 0.114));
        color = mix(vec3(luma), color, saturation);
        // Gritty dark-Nordic tone: subtle cold blue-grey tint in shadows
        vec3 coolTint = vec3(0.95, 0.97, 1.04);
        color = mix(color * coolTint, color, smoothstep(0.0, 0.75, luma));
        // Filmic S-curve contrast boost for gritty depth and punch
        vec3 contrast = color * color * (3.0 - 2.0 * color);
        color = mix(color, contrast, 0.20);
    }
    if (vignette > 0.0) {
        vec2 offset = coords - 0.5;
        offset.x *= mix(1.0, aspect, vignetteRoundness);
        float falloff = dot(offset, offset) * 2.2;
        if (vignetteSmoothness != 0.5) falloff = pow(falloff, vignetteSmoothness * 2.0);
        color *= 1.0 - vignette * falloff;
    }
    return color;
}

// CRT scanlines locked to the internal raster lines
vec3 applyScanlines(vec3 color, vec2 coords)
{
    if (scanlines <= 0.0) return color;
    float linesCount = (internalSize.y > 0.0) ? internalSize.y : 240.0;
    float linePos = coords.y * linesCount;
    float scan = sin(linePos * 3.14159265 * 2.0);
    float beam = 0.5 + 0.5 * scan;
    float dark = mix(1.0, 0.5 + 0.5 * beam, scanlines);
    return color * dark;
}

// Analog TV / film grain noise locked to the internal pixel grid
vec3 applyFilmGrain(vec3 color, vec2 coords)
{
    if (filmGrain <= 0.0) return color;
    vec2 res = (internalSize.x > 0.0 && internalSize.y > 0.0) ? internalSize : vec2(640.0, 480.0);
    vec2 grainPos = floor(coords * res);
    float noise = fract(sin(dot(grainPos + vec2(time * 17.13, -time * 31.41), vec2(12.9898, 78.233))) * 43758.5453);
    return color + (noise - 0.5) * (filmGrain * 0.15);
}

// Dithers and quantizes in internal-resolution pixels, so the pattern stays
// locked to the low-resolution image however far it is upscaled.
vec3 applyColorDepth(vec3 color, vec2 coords)
{
    color = grade(color, coords);
    color = applyScanlines(color, coords);
    color = applyFilmGrain(color, coords);

    if (colorLevels >= 256.0)
        return color;

    float steps = colorLevels - 1.0;

    if (dither) {
        ivec2 cell = ivec2(coords * internalSize) & 3;
        float threshold = (bayer4x4[cell.x + cell.y * 4] + 0.5) / 16.0 - 0.5;
        color += threshold / steps;
    }

    return floor(clamp(color, 0.0, 1.0) * steps + 0.5) / steps;
}

void main()
{
    vec2 uv = TexCoords;
    float bezel = 1.0;

    if (distortion != 0.0) {
        vec2 centered = uv - 0.5;
        float r2 = dot(centered, centered) * 4.0;
        uv = 0.5 + centered * (1.0 + distortion * r2 * 0.5);
    }

    if (crtCurvature > 0.0) {
        uv = curveUV(TexCoords, crtCurvature);
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        vec2 edge = smoothstep(vec2(0.0), vec2(0.02), uv) * smoothstep(vec2(0.0), vec2(0.02), 1.0 - uv);
        bezel = clamp(edge.x * edge.y * 1.05, 0.0, 1.0);
    }

    vec3 color = sampleScreen(uv);

    // Phosphor glow / bloom on bright areas
    if (bloom > 0.0) {
        vec2 texel = (internalSize.x > 0.0 && internalSize.y > 0.0) ? (1.0 / internalSize) : vec2(0.002);
        vec3 glow = texture(screenTexture, uv + vec2(-texel.x * 2.0, 0.0)).rgb
                  + texture(screenTexture, uv + vec2( texel.x * 2.0, 0.0)).rgb
                  + texture(screenTexture, uv + vec2(0.0, -texel.y * 2.0)).rgb
                  + texture(screenTexture, uv + vec2(0.0,  texel.y * 2.0)).rgb
                  + texture(screenTexture, uv + vec2(-texel.x * 1.5, -texel.y * 1.5)).rgb
                  + texture(screenTexture, uv + vec2( texel.x * 1.5,  texel.y * 1.5)).rgb;
        glow /= 6.0;
        vec3 highlights = max(glow - vec3(0.4), vec3(0.0)) * 1.7;
        color += highlights * (bloom * 0.45);
    }

    FragColor = vec4(applyColorDepth(color, uv) * bezel, 1.0);
}