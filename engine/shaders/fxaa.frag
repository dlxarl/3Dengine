#version 330 core
// FXAA 3.11, quality preset 12: finds edges by luma, walks along them to their ends, and
// moves the sample half a pixel across the edge in proportion to where it sits.
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D screenTexture;
uniform vec2 texelSize;

const float EDGE_THRESHOLD_MIN = 0.0312;
const float EDGE_THRESHOLD_MAX = 0.125;
const float SUBPIXEL_QUALITY = 0.75;
const int ITERATIONS = 12;
const float STEP_QUALITY[ITERATIONS] = float[ITERATIONS](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

float luma(vec3 c)
{
    return dot(c, vec3(0.299, 0.587, 0.114));
}

float lumaAt(vec2 uv)
{
    return luma(texture(screenTexture, uv).rgb);
}

void main()
{
    vec2 uv = TexCoords;
    vec3 colorCenter = texture(screenTexture, uv).rgb;
    float lumaCenter = luma(colorCenter);

    float lumaDown = luma(textureOffset(screenTexture, uv, ivec2(0, -1)).rgb);
    float lumaUp = luma(textureOffset(screenTexture, uv, ivec2(0, 1)).rgb);
    float lumaLeft = luma(textureOffset(screenTexture, uv, ivec2(-1, 0)).rgb);
    float lumaRight = luma(textureOffset(screenTexture, uv, ivec2(1, 0)).rgb);

    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;

    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX)) {
        FragColor = vec4(colorCenter, 1.0);
        return;
    }

    float lumaDownLeft = luma(textureOffset(screenTexture, uv, ivec2(-1, -1)).rgb);
    float lumaUpRight = luma(textureOffset(screenTexture, uv, ivec2(1, 1)).rgb);
    float lumaUpLeft = luma(textureOffset(screenTexture, uv, ivec2(-1, 1)).rgb);
    float lumaDownRight = luma(textureOffset(screenTexture, uv, ivec2(1, -1)).rgb);

    float lumaDownUp = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;
    float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
    float lumaDownCorners = lumaDownLeft + lumaDownRight;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaUpCorners = lumaUpRight + lumaUpLeft;

    float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 + abs(-2.0 * lumaRight + lumaRightCorners);
    float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + abs(-2.0 * lumaDown + lumaDownCorners);
    bool isHorizontal = edgeHorizontal >= edgeVertical;

    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;
    bool is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2));

    float stepLength = isHorizontal ? texelSize.y : texelSize.x;
    float lumaLocalAverage;
    if (is1Steepest) {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    } else {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }

    vec2 currentUv = uv;
    if (isHorizontal) currentUv.y += stepLength * 0.5;
    else currentUv.x += stepLength * 0.5;

    vec2 offset = isHorizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);
    vec2 uv1 = currentUv - offset;
    vec2 uv2 = currentUv + offset;
    float lumaEnd1 = lumaAt(uv1) - lumaLocalAverage;
    float lumaEnd2 = lumaAt(uv2) - lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    if (!reached1) uv1 -= offset;
    if (!reached2) uv2 += offset;

    if (!reachedBoth) {
        for (int i = 2; i < ITERATIONS; i++) {
            if (!reached1) lumaEnd1 = lumaAt(uv1) - lumaLocalAverage;
            if (!reached2) lumaEnd2 = lumaAt(uv2) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;
            if (!reached1) uv1 -= offset * STEP_QUALITY[i];
            if (!reached2) uv2 += offset * STEP_QUALITY[i];
            if (reachedBoth) break;
        }
    }

    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    float pixelOffset = -distanceFinal / edgeThickness + 0.5;

    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    float lumaAverage = (1.0 / 12.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
    finalOffset = max(finalOffset, subPixelOffsetFinal);

    vec2 finalUv = uv;
    if (isHorizontal) finalUv.y += finalOffset * stepLength;
    else finalUv.x += finalOffset * stepLength;

    FragColor = vec4(texture(screenTexture, finalUv).rgb, 1.0);
}
