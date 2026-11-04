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

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform bool directionalLight;
uniform vec3 lightDirection;
uniform vec3 ambientColor;
uniform float time;
uniform float waveHeight;

const int MAX_POINT_LIGHTS = 8;
uniform int pointLightCount;
uniform vec4 pointLights[MAX_POINT_LIGHTS];
uniform vec3 pointLightColors[MAX_POINT_LIGHTS];
const float POINT_LIGHT_STRENGTH = 1.8;   // a fire at night outshines the moon

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

vec3 pointLighting(vec3 position, vec3 normal)
{
    vec3 total = vec3(0.0);
    for (int i = 0; i < pointLightCount; i++) {
        vec3 offset = pointLights[i].xyz - position;
        float distance = length(offset);
        float reach = distance / pointLights[i].w;
        float fade = clamp(1.0 - reach * reach, 0.0, 1.0);
        float flicker = 0.88 + 0.12 * sin(time * 9.0 + float(i) * 2.3) * sin(time * 5.7 + float(i));
        float facing = max(dot(normal, offset / max(distance, 0.001)), 0.0) * 0.75 + 0.25;
        total += pointLightColors[i] * POINT_LIGHT_STRENGTH * fade * fade * facing * flicker;
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
    vec3 L = directionalLight ? -lightDirection : normalize(lightPos - FragPos);
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-L, N);

    // Attenuation
    float dist = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.02 * dist + 0.001 * dist * dist);
    if (directionalLight)
        attenuation = 1.0;


    // Material-aware roughness and specular:
    // Organic surfaces (horse fur, skin, cloth, wood, stone) are matte without plastic gloss.
    // Metals (armor, weapons, buckles) retain crisp specular highlights.
    float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
    float sat = max(max(albedo.r, albedo.g), albedo.b) - min(min(albedo.r, albedo.g), albedo.b);
    bool isMetallic = (sat < 0.12 && luma > 0.28 && luma < 0.92);
    float ka = 0.35;
    float kd = 1.0;
    float ks = isMetallic ? 0.65 : 0.04;
    float shininess = isMetallic ? 48.0 : 8.0;
    if (roughness > 0.0) {
        // A measured surface: dull when rough, with a tight bright highlight when smooth.
        ks = mix(0.3, 0.03, roughness);
        shininess = mix(110.0, 5.0, roughness);
    }

    // Phong lighting
    vec3 ambient = ka * albedo * lightColor;
    if (directionalLight) {
        // Brighter from the sky above than from the ground below.
        vec3 sky = ambientColor * 1.12;
        vec3 ground = ambientColor * vec3(0.62, 0.58, 0.52);
        ambient = mix(ground, sky, N.y * 0.5 + 0.5) * albedo;
    }
    ambient *= ambientOcclusion;

    float diff = max(dot(N, L), 0.0);
    if (directionalLight)
        diff = max((dot(N, L) + SUN_WRAP) / (1.0 + SUN_WRAP), 0.0);
    vec3 diffuse = attenuation * kd * diff * albedo * lightColor;
    if (!pixelLit)
        diffuse = VertexDiffuse * kd * albedo;

    float spec = pow(max(dot(R, V), 0.0), shininess);
    vec3 specular = attenuation * ks * spec * lightColor;
    if (!pixelLit)
        specular = VertexSpecular;

    vec3 nearby = pointLightCount == 0 ? vec3(0.0) : (!pixelLit ? VertexPoint : pointLighting(FragPos, N));

    // Shadow
    //float shadow = ShadowCalculation(FragPosLightSpace, N, L);
    float shadow = 0.0;
    if (enableShadows == 1)
        shadow = ShadowCalculation(FragPosLightSpace, N, L);


    //vec3 color = ambient + (diffuse + specular) * (1.0 - shadow);
    //FragColor = vec4(color, 1.0);
    vec3 color;

    vec3 haze = fogColor;
    if (directionalLight)
        haze += scattering(-V, fogColor);

    if (material.emissive) {
        color = albedo;
    } else if (enableLighting == 1) {
        color = ambient + (diffuse + specular) * (1.0 - shadow);
        if (pointLightCount > 0)
            color += nearby * albedo;

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
