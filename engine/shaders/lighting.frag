#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
noperspective in vec2 AffineTexCoords;
in vec4 FragPosLightSpace;
in vec3 VertexDiffuse;
in vec3 VertexSpecular;

struct Material {
    sampler2D albedoMap;
    bool hasAlbedo;
    bool tintAlbedo;
};

uniform Material material;
uniform sampler2DShadow shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

uniform int enableLighting;
uniform int enableShadows;

// Retro rendering
uniform bool vertexLighting;
uniform bool affineTextures;

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



void main()
{
    // Albedo
    vec3 albedo = objectColor;
    if (material.hasAlbedo) {
        vec2 uv = affineTextures ? AffineTexCoords : TexCoords;
        albedo = texture(material.albedoMap, uv).rgb;
        if (material.tintAlbedo)
            albedo *= objectColor;
    }

    // Vectors
    vec3 N = normalize(Normal);
    vec3 L = normalize(lightPos - FragPos);
    vec3 V = normalize(viewPos - FragPos);
    vec3 R = reflect(-L, N);

    // Attenuation
    float dist = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.02 * dist + 0.001 * dist * dist);


    // Phong constants
    float ka = 0.35; //0.25
    float kd = 1.0;
    float ks = 0.5;
    float shininess = 32.0;

    // Phong lighting
    vec3 ambient = ka * albedo * lightColor;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = attenuation * kd * diff * albedo * lightColor;
    if (vertexLighting)
        diffuse = VertexDiffuse * kd * albedo;

    float spec = pow(max(dot(R, V), 0.0), shininess);
    vec3 specular = attenuation * ks * spec * lightColor;
    if (vertexLighting)
        specular = VertexSpecular;

    // Shadow
    //float shadow = ShadowCalculation(FragPosLightSpace, N, L);
    float shadow = 0.0;
    if (enableShadows == 1)
        shadow = ShadowCalculation(FragPosLightSpace, N, L);


    //vec3 color = ambient + (diffuse + specular) * (1.0 - shadow);
    //FragColor = vec4(color, 1.0);
    vec3 color;

    if (enableLighting == 1) {
        color = ambient + (diffuse + specular) * (1.0 - shadow);
    } else {
        color = albedo; // без освітлення — чистий колір
    }

    FragColor = vec4(color, 1.0);

}
