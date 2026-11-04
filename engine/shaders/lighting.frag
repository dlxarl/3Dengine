#version 330 core
out vec4 FragColor;
layout(location = 1) out vec4 NormalOut;   // world normal, for the post-processing; unused unless it is attached

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
noperspective in vec2 AffineTexCoords;
in vec4 FragPosLightSpace;
in vec3 VertexDiffuse;
in vec3 VertexSpecular;
in vec3 VertexPoint;

struct Material {
    sampler2D albedoMap;
    bool hasAlbedo;
    sampler2D normalMap;      // normal in rgb (OpenGL convention), height in alpha
    bool hasNormal;
    sampler2D roughnessMap;   // ambient occlusion in r, roughness in g, metalness in b; a roughness of 0 is no data
    bool hasRoughness;
    bool tintAlbedo;
    bool emissive;
};

uniform Material material;
uniform sampler2DShadow shadowMap;

uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform bool directionalLight;
uniform vec3 lightDirection;
uniform vec3 ambientColor;
uniform float time;
uniform float waveHeight;

// The lights besides the sun (the sun is lightDirection and lightColor): point, spot and extra
// directional lights, all described the same way.
const int MAX_LIGHTS = 16;
uniform int lightCount;
uniform vec4 lightPositions[MAX_LIGHTS];    // xyz position; w range
uniform vec4 lightDirections[MAX_LIGHTS];   // xyz the way the light travels; w type: 0 directional, 1 point, 2 spot
uniform vec4 lightColors[MAX_LIGHTS];       // rgb color with intensity; a how bright its highlights are
uniform vec4 lightSpots[MAX_LIGHTS];        // x cosine of the cone's edge, y of its bright core; z casts shadows; w radius

// Shadow rays: the scene's still geometry as a bounding volume hierarchy in two float textures.
uniform sampler2D rtNodes;
uniform sampler2D rtTriangles;
uniform int rtShadows;       // trace the lights that have shadows on
uniform int rtSun;           // trace the sun too
uniform int rtSamples;
uniform float sunSoftness;   // the tangent of the sun's angular radius

uniform int enableLighting;
uniform int enableShadows;

// Retro rendering
uniform bool vertexLighting;
uniform bool affineTextures;

uniform vec2 fogRange;  // start and end distance; an end of 0 is no fog
uniform vec3 fogColor;

const float SUN_WRAP = 0.15;

// The material maps are packed in an atlas of 16 x 8 tiles, each a 256 px square that holds a
// 224 px texture wrapped by a border of 16 px.
const vec2 ATLAS_TILES = vec2(16.0, 8.0);
const float PARALLAX_DEPTH = 0.07;    // how deep the surface goes, as a fraction of a tile
const float PARALLAX_RANGE = 45.0;    // beyond this the surface is flat

// The edge of the tile that uv is in, in atlas coordinates, half a texel inside.
vec4 tileBounds(vec2 uv, sampler2D map)
{
    vec2 cell = floor(vec2(uv.x * ATLAS_TILES.x, (1.0 - uv.y) * ATLAS_TILES.y));
    vec2 texel = 0.5 / vec2(textureSize(map, 0));
    vec2 low = vec2(cell.x / ATLAS_TILES.x, 1.0 - (cell.y + 1.0) / ATLAS_TILES.y) + texel;
    vec2 high = vec2((cell.x + 1.0) / ATLAS_TILES.x, 1.0 - cell.y / ATLAS_TILES.y) - texel;
    return vec4(low, high);
}

// Parallax occlusion mapping: marches along the line of sight through the height map until it
// meets the surface, and returns where.
vec2 parallaxUv(vec2 uv, vec3 viewTangent, float distanceToEye)
{
    vec4 bounds = tileBounds(uv, material.normalMap);
    float layers = mix(18.0, 6.0, clamp(distanceToEye / PARALLAX_RANGE, 0.0, 1.0));
    float layerDepth = 1.0 / layers;
    vec2 tileSize = vec2(1.0 / ATLAS_TILES.x, 1.0 / ATLAS_TILES.y);
    vec2 shift = viewTangent.xy / max(viewTangent.z, 0.25) * PARALLAX_DEPTH * tileSize * vec2(0.875);
    vec2 step = shift / layers;

    vec2 current = uv;
    float depth = 0.0;
    float surface = 1.0 - textureLod(material.normalMap, current, 0.0).a;
    for (int i = 0; i < 18; i++) {
        if (float(i) >= layers || depth >= surface) break;
        current = clamp(current - step, bounds.xy, bounds.zw);
        surface = 1.0 - textureLod(material.normalMap, current, 0.0).a;
        depth += layerDepth;
    }
    // Between the last two samples, to hide the layers.
    vec2 before = clamp(current + step, bounds.xy, bounds.zw);
    float after = surface - depth;
    float beforeGap = (1.0 - textureLod(material.normalMap, before, 0.0).a) - (depth - layerDepth);
    float weight = after / (after - beforeGap + 1e-5);
    return mix(current, before, clamp(weight, 0.0, 1.0));
}

// Shadow calculation
float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    projCoords.xy = clamp(projCoords.xy, 0.001, 0.999);

    float bias = 0.0006;

    //float bias = max(0.008 * (1.0 - dot(normal, lightDir)), 0.0006);


    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(
                shadowMap,
                vec3(projCoords.xy + vec2(x,y)*texelSize,
                     projCoords.z - bias)
            );

    shadow /= 9.0;
    return 1.0 - shadow;
}

// ---- Ray tracing -------------------------------------------------------------------------

vec4 rtFetch(sampler2D data, int index)
{
    return texelFetch(data, ivec2(index & 4095, index >> 12), 0);
}

// Whether anything lies between origin and maxDistance along direction: a walk down the
// hierarchy of boxes, testing the triangles of the leaves it reaches.
bool rtOccluded(vec3 origin, vec3 direction, float maxDistance)
{
    vec3 safe = vec3(abs(direction.x) < 1e-8 ? 1e-8 : direction.x,
                     abs(direction.y) < 1e-8 ? 1e-8 : direction.y,
                     abs(direction.z) < 1e-8 ? 1e-8 : direction.z);
    vec3 inverse = 1.0 / safe;

    int stack[48];
    int top = 0;
    int node = 0;
    for (int guard = 0; guard < 4096; guard++) {
        vec4 a = rtFetch(rtNodes, node * 2);
        vec4 b = rtFetch(rtNodes, node * 2 + 1);
        vec3 t0 = (a.xyz - origin) * inverse;
        vec3 t1 = (b.xyz - origin) * inverse;
        vec3 nearT = min(t0, t1);
        vec3 farT = max(t0, t1);
        float enter = max(max(nearT.x, nearT.y), nearT.z);
        float leave = min(min(farT.x, farT.y), farT.z);
        if (leave >= max(enter, 0.0) && enter < maxDistance) {
            if (b.w > 0.5) {
                int first = int(a.w);
                int count = int(b.w);
                for (int i = 0; i < count; i++) {
                    int base = (first + i) * 3;
                    vec3 corner = rtFetch(rtTriangles, base).xyz;
                    vec3 e1 = rtFetch(rtTriangles, base + 1).xyz;
                    vec3 e2 = rtFetch(rtTriangles, base + 2).xyz;
                    vec3 p = cross(direction, e2);
                    float det = dot(e1, p);
                    if (abs(det) < 1e-9) continue;
                    float invDet = 1.0 / det;
                    vec3 s = origin - corner;
                    float u = dot(s, p) * invDet;
                    if (u < 0.0 || u > 1.0) continue;
                    vec3 q = cross(s, e1);
                    float v = dot(direction, q) * invDet;
                    if (v < 0.0 || u + v > 1.0) continue;
                    float t = dot(e2, q) * invDet;
                    if (t > 1e-4 && t < maxDistance) return true;
                }
            } else {
                if (top < 48) stack[top++] = int(a.w);
                node += 1;
                continue;
            }
        }
        if (top == 0) break;
        node = stack[--top];
    }
    return false;
}

// Interleaved gradient noise: a different value at every pixel, so that few rays still average out.
float rtNoise(float offset)
{
    return fract(52.9829189 * fract(dot(gl_FragCoord.xy + offset * vec2(17.0, 31.0), vec2(0.06711056, 0.00583715))));
}

vec3 rtDirectionInSphere(int sample)
{
    float a = rtNoise(float(sample)) * 6.2831853;
    float b = rtNoise(float(sample) + 7.0) * 2.0 - 1.0;
    float r = sqrt(max(1.0 - b * b, 0.0));
    return vec3(r * cos(a), b, r * sin(a)) * pow(rtNoise(float(sample) + 3.0), 0.333);
}

// How much of a point or spot light reaches a distance, smoothly gone at its range.
float rangeFade(float distance, float range)
{
    float reach = distance / range;
    float fade = clamp(1.0 - reach * reach, 0.0, 1.0);
    return fade * fade;
}

// Lambert only, for the retro look that lights per vertex; the light itself is the same.
vec3 vertexLights(vec3 position, vec3 normal)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec3 toLight;
        float strength = 1.0;
        if (lightDirections[i].w < 0.5) {
            toLight = -lightDirections[i].xyz;
        } else {
            vec3 offset = lightPositions[i].xyz - position;
            float distance = length(offset);
            toLight = offset / max(distance, 0.001);
            strength = rangeFade(distance, lightPositions[i].w);
            if (lightDirections[i].w > 1.5)
                strength *= smoothstep(lightSpots[i].x, lightSpots[i].y, dot(-toLight, lightDirections[i].xyz));
        }
        total += lightColors[i].rgb * strength * (max(dot(normal, toLight), 0.0) * 0.75 + 0.25 * step(0.001, strength));
    }
    return total;
}

// Cook-Torrance: a microfacet highlight (GGX distribution, Smith geometry, Schlick's Fresnel)
// plus the diffuse light the highlight does not take.
vec3 surfaceLight(vec3 toLight, vec3 radiance, float specular, float intensityScale, vec3 N, vec3 V, float NV,
                  vec3 albedo, vec3 F0, float metalness, float alpha, float wrap)
{
    float NLraw = dot(N, toLight);
    float NL = max(NLraw, 0.0);
    vec3 H = normalize(toLight + V);
    float NH = max(dot(N, H), 0.0);
    float VH = max(dot(V, H), 0.0);

    float a2 = alpha * alpha;
    float d = NH * NH * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159265 * d * d + 1e-5);
    float k = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    float G = (NV / (NV * (1.0 - k) + k)) * (NL / (NL * (1.0 - k) + k));
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VH, 5.0);

    vec3 highlight = D * G * F / max(4.0 * NV * NL, 1e-3) * NL * specular;
    float wrapped = wrap > 0.0 ? max((NLraw + wrap) / (1.0 + wrap), 0.0) : NL;
    vec3 diffuse = (vec3(1.0) - F) * (1.0 - metalness) * albedo * wrapped;
    return (diffuse + highlight) * radiance * intensityScale;
}

// How much of the light gets through to a point: 1 in the open, 0 in shadow.
float lightShadow(int index, vec3 position, vec3 normal, vec3 toLight, float distanceToLight)
{
    if (rtShadows == 0) return 1.0;

    float radius = lightSpots[index].w;
    int samples = radius > 0.0 ? rtSamples : 1;
    vec3 origin = position + normal * 0.04 + toLight * 0.02;
    float visible = 0.0;
    for (int s = 0; s < 8; s++) {
        if (s >= samples) break;
        vec3 target = lightPositions[index].xyz;
        if (lightDirections[index].w < 0.5) {
            // A directional light with a size: the direction wobbles inside a cone.
            vec3 direction = normalize(-lightDirections[index].xyz + rtDirectionInSphere(s) * radius * 0.05);
            visible += rtOccluded(origin, direction, 400.0) ? 0.0 : 1.0;
            continue;
        }
        if (radius > 0.0) target += rtDirectionInSphere(s) * radius;
        vec3 offset = target - origin;
        float span = max(length(offset), 0.001);
        visible += rtOccluded(origin, offset / span, span - 0.05) ? 0.0 : 1.0;
    }
    return visible / float(samples);
}

// The sun's shadow from rays, reaching as far as the geometry goes.
float sunRayShadow(vec3 position, vec3 normal, vec3 toSun)
{
    int samples = sunSoftness > 0.0 ? rtSamples : 1;
    vec3 origin = position + normal * 0.05 + toSun * 0.03;
    float visible = 0.0;
    for (int s = 0; s < 8; s++) {
        if (s >= samples) break;
        vec3 direction = normalize(toSun + rtDirectionInSphere(s) * sunSoftness);
        visible += rtOccluded(origin, direction, 600.0) ? 0.0 : 1.0;
    }
    return 1.0 - visible / float(samples);
}

vec3 pixelLights(vec3 position, vec3 N, vec3 V, float NV, vec3 albedo, vec3 F0, float metalness, float alpha)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < lightCount; i++) {
        vec3 toLight;
        float strength = 1.0;
        float distanceToLight = 1e4;
        if (lightDirections[i].w < 0.5) {
            toLight = -lightDirections[i].xyz;
        } else {
            vec3 offset = lightPositions[i].xyz - position;
            distanceToLight = length(offset);
            if (distanceToLight > lightPositions[i].w) continue;
            toLight = offset / max(distanceToLight, 0.001);
            strength = rangeFade(distanceToLight, lightPositions[i].w);
            if (lightDirections[i].w > 1.5)
                strength *= smoothstep(lightSpots[i].x, lightSpots[i].y, dot(-toLight, lightDirections[i].xyz));
        }
        if (strength <= 0.0 || dot(N, toLight) <= -0.2) continue;

        float lit = lightSpots[i].z > 0.5 ? lightShadow(i, position, N, toLight, distanceToLight) : 1.0;
        if (lit <= 0.0) continue;
        total += surfaceLight(toLight, lightColors[i].rgb, lightColors[i].a, strength * lit, N, V, NV, albedo, F0, metalness, alpha, 0.35);
    }
    return total;
}

// Sunlight scattered in the haze, brightest looking towards the sun; the same as the sky's.
vec3 scattering(vec3 direction, vec3 horizon)
{
    float towards = max(dot(direction, -lightDirection), 0.0);
    float day = smoothstep(0.12, 0.3, dot(horizon, vec3(0.299, 0.587, 0.114)));
    return lightColor * (pow(towards, 8.0) * 0.35 + pow(towards, 64.0) * 0.4) * (0.3 + 0.7 * day);
}

void main()
{
    // Surface frame from the screen-space derivatives, so that meshes need no tangents.
    vec3 geometricNormal = normalize(Normal);
    vec3 dp1 = dFdx(FragPos), dp2 = dFdy(FragPos);
    vec2 baseUv = affineTextures ? AffineTexCoords : TexCoords;
    vec2 duv1 = dFdx(baseUv), duv2 = dFdy(baseUv);
    vec3 dp2perp = cross(dp2, geometricNormal), dp1perp = cross(geometricNormal, dp1);
    vec3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    float frame = inversesqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-12));
    mat3 TBN = mat3(tangent * frame, bitangent * frame, geometricNormal);

    float eyeDistance = length(viewPos - FragPos);
    bool detailed = material.hasNormal && material.hasAlbedo && enableLighting == 1;
    float ambientOcclusion = 1.0;
    float roughness = -1.0;   // negative: no data, and the guess below is used
    vec3 detailNormal = geometricNormal;
    vec2 litUv = baseUv;
    if (detailed) {
        vec4 packed = material.hasRoughness ? texture(material.roughnessMap, baseUv) : vec4(1.0, 0.0, 0.0, 1.0);
        if (packed.g > 0.0 && eyeDistance < PARALLAX_RANGE) {
            vec3 toEye = normalize(viewPos - FragPos);
            litUv = parallaxUv(baseUv, transpose(TBN) * toEye, eyeDistance);
        }
        if (packed.g > 0.0) {
            vec4 maps = textureGrad(material.roughnessMap, litUv, duv1, duv2);
            ambientOcclusion = maps.r;
            roughness = maps.g;
            vec3 tangentNormal = textureGrad(material.normalMap, litUv, duv1, duv2).xyz * 2.0 - 1.0;
            // The detail fades out with distance, where it would only shimmer.
            tangentNormal = normalize(mix(tangentNormal, vec3(0.0, 0.0, 1.0), smoothstep(30.0, 80.0, eyeDistance)));
            detailNormal = normalize(TBN * tangentNormal);
        }
    }
    bool pixelLit = !vertexLighting || roughness > 0.0;

    // Albedo
    vec3 albedo = objectColor;
    if (material.hasAlbedo) {
        vec2 uv = detailed ? litUv : baseUv;
        // Water moves by its swell alone: sliding the texture would run off its tile in an atlas.
        vec4 texel = texture(material.albedoMap, uv);
        // Cut-out transparency, as the PlayStation did it: a pixel is there or it is not.
        if (texel.a < 0.5)
            discard;
        albedo = texel.rgb;
        if (material.tintAlbedo)
            albedo *= objectColor;
    }

    // Vectors
    vec3 N = detailNormal;
    vec3 V = normalize(viewPos - FragPos);
    vec3 L = directionalLight ? -lightDirection : vec3(0.0, 1.0, 0.0);

    // The surface: how rough it is and how metallic. A measured roughness is used where the
    // material has one; otherwise organic surfaces (fur, skin, cloth, wood, stone) are matte and
    // grey, bright ones (armor, weapons, buckles) are metal.
    float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
    float sat = max(max(albedo.r, albedo.g), albedo.b) - min(min(albedo.r, albedo.g), albedo.b);
    bool isMetallic = (sat < 0.12 && luma > 0.28 && luma < 0.92);
    float metalness = isMetallic ? 0.3 : 0.0;   // a guess: grey and mid-bright is iron or stone, and without a map there is no knowing
    float surfaceRoughness = roughness > 0.0 ? roughness : (isMetallic ? 0.55 : 0.8);
    float alpha = max(surfaceRoughness * surfaceRoughness, 0.03);
    vec3 F0 = mix(vec3(0.04), albedo, metalness);
    float NV = max(dot(N, V), 1e-3);

    // Light from the sky above is brighter than from the ground below.
    vec3 sky = ambientColor * 1.12;
    vec3 ground = ambientColor * vec3(0.62, 0.58, 0.52);
    vec3 ambient = mix(ground, sky, N.y * 0.5 + 0.5) * albedo * ambientOcclusion;

    // Shadow of the sun, from its shadow map
    float shadow = 0.0;
    if (enableShadows == 1 && directionalLight)
        shadow = ShadowCalculation(FragPosLightSpace, N, L);
    if (rtSun == 1 && directionalLight && dot(N, L) > -0.1)
        shadow = max(shadow, sunRayShadow(FragPos, N, L));

    // The sun
    vec3 sun = vec3(0.0);
    if (directionalLight) {
        sun = surfaceLight(L, lightColor, 1.0, 1.0, N, V, NV, albedo, F0, metalness, alpha, SUN_WRAP) * (1.0 - shadow);
    }

    // Everything else that shines
    vec3 others = vec3(0.0);
    if (lightCount > 0)
        others = !pixelLit ? VertexPoint * albedo : pixelLights(FragPos, N, V, NV, albedo, F0, metalness, alpha);

    if (!pixelLit) {
        // Per-vertex lighting, as the consoles did it: computed at the corners and blended across.
        vec3 kd = (1.0 - metalness) * albedo;
        sun = (VertexDiffuse * kd + VertexSpecular) * (1.0 - shadow);
    }

    vec3 color;

    vec3 haze = fogColor;
    if (directionalLight)
        haze += scattering(-V, fogColor);

    if (material.emissive) {
        color = albedo;
    } else if (enableLighting == 1) {
        color = ambient + sun + others;

        if (waveHeight > 0.0 && directionalLight) {
            // Water mirrors the sky, more so at a glancing angle, and glints where the sun is.
            float fresnel = pow(1.0 - max(dot(N, V), 0.0), 3.0);
            color = mix(color, haze * 1.05, 0.25 + 0.55 * fresnel);
            color += lightColor * pow(max(dot(reflect(-L, N), V), 0.0), 64.0) * 1.5 * (1.0 - shadow);
        }
    } else {
        color = albedo; // без освітлення — чистий колір
    }

    if (fogRange.y > 0.0) {
        float fog = clamp((length(viewPos - FragPos) - fogRange.x) / (fogRange.y - fogRange.x), 0.0, 1.0);
        // Under the sun the haze thickens soon and thins slowly, as air does.
        if (directionalLight)
            fog = 1.0 - (1.0 - fog) * (1.0 - fog);
        color = mix(color, haze, fog);
    }

    FragColor = vec4(color, 1.0);
    NormalOut = vec4(N * 0.5 + 0.5, 1.0);

}
