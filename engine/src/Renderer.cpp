#include "Renderer.h"
#include "Resources.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    const int SHADOW_TEX_UNIT = 10;
    const unsigned int MATERIAL_TEX_UNITS = 5;
    const float MOTION_BLUR_REFERENCE_FPS = 60.0f;
    const int MAX_LIGHTS = 16;
    const int RAY_NODE_UNIT = 11;
    const int RAY_TRIANGLE_UNIT = 12;
    const int RAY_TEXTURE_WIDTH = 4096;
    const float MIPMAPPED_NEAREST_MAX_LOD = 4.0f;  // further down, the tiles of a texture atlas run together

    glm::mat4 lightSpaceFor(glm::vec3 lightPos) {
        const float near_plane = 1.0f, far_plane = 200.0f;
        glm::mat4 lightProjection = glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, near_plane, far_plane);

        // lookAt degenerates when the light sits straight above the origin.
        const glm::vec3 direction = glm::normalize(-lightPos);
        const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        return lightProjection * glm::lookAt(lightPos, glm::vec3(0.0f), up);
    }

    // Sunlight has no position, so its shadow box can go wherever the camera looks. It moves
    // in whole shadow-map texels, or the edges of every shadow would crawl as the camera turns.
    glm::mat4 lightSpaceFollowing(glm::vec3 direction, const CameraView& camera, unsigned int resolution) {
        const float extent = 34.0f;
        const float depth = 120.0f;

        const glm::vec3 forward = -glm::vec3(glm::inverse(camera.view)[2]);
        glm::vec3 flatForward(forward.x, 0.0f, forward.z);
        const float flatLength = glm::length(flatForward);
        flatForward = flatLength > 0.001f ? flatForward / flatLength : glm::vec3(0.0f);
        const glm::vec3 center = camera.position + flatForward * (extent * 0.55f);

        const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f), direction, up);

        glm::vec3 c = glm::vec3(lightView * glm::vec4(center, 1.0f));
        const float texel = 2.0f * extent / (float)resolution;
        c.x = std::floor(c.x / texel) * texel;
        c.y = std::floor(c.y / texel) * texel;

        const glm::mat4 projection = glm::ortho(c.x - extent, c.x + extent, c.y - extent, c.y + extent, -c.z - depth, -c.z + depth);
        return projection * lightView;
    }
}

Renderer::Renderer(unsigned int shadowResolution) {
    lightingShader = std::make_unique<Shader>(Resources::shader("lighting.vert").c_str(),
                                              Resources::shader("lighting.frag").c_str());
    lampShader = std::make_unique<Shader>(Resources::shader("lighting.vert").c_str(),
                                          Resources::shader("lamp.frag").c_str());
    depthShader = std::make_unique<Shader>(Resources::shader("shadow_depth.vert").c_str(),
                                           Resources::shader("shadow_depth.frag").c_str());
    skyShader = std::make_unique<Shader>(Resources::shader("sky.vert").c_str(),
                                         Resources::shader("sky.frag").c_str());
    glGenVertexArrays(1, &skyVertexArray);
    effects = std::make_unique<EffectRenderer>();

    shadowMap = std::make_unique<ShadowMap>(shadowResolution);
    postProcessor = std::make_unique<PostProcessor>(1, 1);

    // Empty until a scene is traced; the shader's samplers always have something behind them.
    glGenTextures(1, &rayNodeTexture);
    glGenTextures(1, &rayTriangleTexture);
    const float emptyTexel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (unsigned int texture : {rayNodeTexture, rayTriangleTexture}) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 1, 1, 0, GL_RGBA, GL_FLOAT, emptyTexel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenSamplers(1, &materialSampler);
    glSamplerParameteri(materialSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glSamplerParameteri(materialSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
    samplerFilter = TextureFilter::Nearest;
    applyTextureFilter(TextureFilter::Trilinear);

    const unsigned char white[4] = {255, 255, 255, 255};
    glGenTextures(1, &fallbackTexture);
    glBindTexture(GL_TEXTURE_2D, fallbackTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Renderer::~Renderer() {
    glDeleteTextures(1, &rayNodeTexture);
    glDeleteTextures(1, &rayTriangleTexture);
    glDeleteSamplers(1, &materialSampler);
    glDeleteTextures(1, &fallbackTexture);
    glDeleteVertexArrays(1, &skyVertexArray);
}

void Renderer::uploadRayTrace(const std::shared_ptr<const RayTraceData>& data) {
    rayUploaded = data;

    auto upload = [](unsigned int texture, const std::vector<glm::vec4>& texels) {
        const int count = std::max<int>(1, (int)texels.size());
        const int rows = (count + RAY_TEXTURE_WIDTH - 1) / RAY_TEXTURE_WIDTH;
        std::vector<glm::vec4> padded(texels);
        padded.resize((size_t)rows * RAY_TEXTURE_WIDTH, glm::vec4(0.0f));
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, RAY_TEXTURE_WIDTH, rows, 0, GL_RGBA, GL_FLOAT, padded.data());
    };
    upload(rayNodeTexture, data->nodes);
    upload(rayTriangleTexture, data->triangles);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::applyTextureFilter(TextureFilter filter) {
    if (filter == samplerFilter) return;
    samplerFilter = filter;

    GLint minFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLint magFilter = GL_LINEAR;
    float maxLod = 1000.0f;
    if (filter == TextureFilter::Nearest) {
        minFilter = GL_NEAREST;
        magFilter = GL_NEAREST;
    } else if (filter == TextureFilter::Bilinear) {
        minFilter = GL_LINEAR;
    } else if (filter == TextureFilter::NearestMipmapped) {
        // Square texels up close; smaller copies further off, so distant ground does not sparkle.
        minFilter = GL_NEAREST_MIPMAP_LINEAR;
        magFilter = GL_NEAREST;
        maxLod = MIPMAPPED_NEAREST_MAX_LOD;
    }

    glSamplerParameteri(materialSampler, GL_TEXTURE_MIN_FILTER, minFilter);
    glSamplerParameteri(materialSampler, GL_TEXTURE_MAG_FILTER, magFilter);
    glSamplerParameterf(materialSampler, GL_TEXTURE_MAX_LOD, maxLod);
}

void Renderer::bindMaterialSamplers(unsigned int sampler) {
    for (unsigned int unit = 0; unit < MATERIAL_TEX_UNITS; unit++) {
        glBindSampler(unit, sampler);

        // Objects without a texture still have the sampler in their shader, and between
        // frames this unit may hold whatever the UI drew last.
        if (sampler != 0) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, fallbackTexture);
        }
    }
}

void Renderer::render(Scene& scene, const CameraView& camera, const RenderOutput& output, const RenderSettings& settings) {
    if (output.width <= 0 || output.height <= 0) return;

    const glm::ivec2 internal = settings.internalSize(output.width, output.height);
    const bool upscaled = internal.x != output.width || internal.y != output.height;
    const float aspectRatio = (float)output.width / (float)output.height;

    const glm::vec3 lightPos = scene.getLightPos();
    glm::vec3 lightDirection(0.0f, -1.0f, 0.0f);
    const bool directional = scene.getLightDirection(lightDirection);
    const glm::mat4 lightSpaceMatrix = directional
        ? lightSpaceFollowing(lightDirection, camera, shadowMap->SHADOW_WIDTH)
        : lightSpaceFor(lightPos);

    const bool useShadows = settings.shadows && settings.lighting;

    // Shadow pass
    if (useShadows) {
        depthShader->use();
        depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

        shadowMap->bind();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        scene.drawShadowCasters(*depthShader, lightSpaceMatrix);

        shadowMap->unbind(output.width, output.height);
    }

    // Scene pass
    postProcessor->resize(internal.x, internal.y);
    postProcessor->prepare(settings);
    postProcessor->setUpscaleNearest(upscaled);
    postProcessor->beginRender(scene.getClearColor());
    if (scene.hasGradientSky()) drawSky(scene, camera, lightDirection);

    lightingShader->use();
    lightingShader->setMat4("projection", camera.projection);
    lightingShader->setMat4("view", camera.view);
    lightingShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    lightingShader->setVec3("viewPos", camera.position);
        lightingShader->setVec3("lightColor", directional ? scene.getLightColor() : glm::vec3(0.0f));
    lightingShader->setBool("directionalLight", directional);
    lightingShader->setVec3("lightDirection", lightDirection);
    lightingShader->setVec3("ambientColor", scene.getAmbientColor());
    lightingShader->setFloat("time", scene.getAnimationTime());

    // The sun is the first light when it is directional; every other light goes in the list.
    SceneLight lights[MAX_LIGHTS];
    const int lightTotal = settings.lighting ? scene.getLights(camera.position, lights, MAX_LIGHTS) : 0;
    const int firstOther = (lightTotal > 0 && lights[0].type == SceneLight::Directional && directional) ? 1 : 0;
    const int others = lightTotal - firstOther;
    lightingShader->setInt("lightCount", others);
    for (int i = 0; i < others; i++) {
        const SceneLight& light = lights[firstOther + i];
        const std::string at = "[" + std::to_string(i) + "]";
        lightingShader->setVec4("lightPositions" + at, glm::vec4(light.position, light.range));
        lightingShader->setVec4("lightDirections" + at, glm::vec4(light.direction, (float)light.type));
        lightingShader->setVec4("lightColors" + at, glm::vec4(light.color, light.specular));
        lightingShader->setVec4("lightSpots" + at, glm::vec4(light.spotCosOuter, light.spotCosInner, light.castShadows ? 1.0f : 0.0f, light.radius));
    }

    // Shadow rays, against the scene's still geometry.
    std::shared_ptr<const RayTraceData> rays;
    if (settings.lighting && (settings.rayTracedShadows || settings.rayTracedSun)) rays = scene.getRayTraceData();
    if (rays && rays != rayUploaded) uploadRayTrace(rays);
    const bool tracing = rays && !rays->empty();
    lightingShader->setInt("rtShadows", tracing && settings.rayTracedShadows ? 1 : 0);
    lightingShader->setInt("rtSun", tracing && settings.rayTracedSun ? 1 : 0);
    lightingShader->setInt("rtSamples", std::clamp(settings.raySamples, 1, 8));
    const float sunDegrees = (lightTotal > 0 && lights[0].type == SceneLight::Directional) ? lights[0].radius : 0.0f;
    lightingShader->setFloat("sunSoftness", std::tan(glm::radians(std::min(sunDegrees, 20.0f))));
    glActiveTexture(GL_TEXTURE0 + RAY_NODE_UNIT);
    glBindTexture(GL_TEXTURE_2D, rayNodeTexture);
    glActiveTexture(GL_TEXTURE0 + RAY_TRIANGLE_UNIT);
    glBindTexture(GL_TEXTURE_2D, rayTriangleTexture);
    lightingShader->setInt("rtNodes", RAY_NODE_UNIT);
    lightingShader->setInt("rtTriangles", RAY_TRIANGLE_UNIT);

    const glm::vec2 fog = scene.getFogRange();
    lightingShader->setVec2("fogRange", fog);
    lightingShader->setVec3("fogColor", scene.getClearColor());

    lightingShader->setInt("enableLighting", settings.lighting ? 1 : 0);
    lightingShader->setInt("enableShadows", useShadows ? 1 : 0);

    lightingShader->setBool("vertexLighting", settings.mode != RenderMode::Modern);
    lightingShader->setBool("affineTextures", settings.mode == RenderMode::PS1);
    lightingShader->setVec2("vertexGrid", settings.vertexGrid(aspectRatio));

    glActiveTexture(GL_TEXTURE0 + SHADOW_TEX_UNIT);
    glBindTexture(GL_TEXTURE_2D, shadowMap->depthMap);
    lightingShader->setInt("shadowMap", SHADOW_TEX_UNIT);

    applyTextureFilter(settings.textureFilter);
    bindMaterialSamplers(materialSampler);

    glDisable(GL_CULL_FACE);

    scene.draw(*lightingShader, *lampShader, camera.view, camera.projection);

    bindMaterialSamplers(0);
    glDisable(GL_CULL_FACE);

    // Water and particles, over the scene and against its depth.
    EffectFrame frame;
    frame.view = camera.view;
    frame.projection = camera.projection;
    frame.eye = camera.position;
    frame.fogRange = fog;
    frame.fogColor = scene.getClearColor();
    frame.lightDirection = lightDirection;
    frame.lightColor = scene.getLightColor();
    frame.ambientColor = scene.getAmbientColor();
    frame.horizonColor = scene.getClearColor();
    frame.time = scene.getAnimationTime();
    postProcessor->beginEffects();
    effects->begin(frame);
    scene.drawEffects(*effects);
    effects->end();
    postProcessor->endEffects();
    postProcessor->setUnderwater(scene.getUnderwater(camera.position));

    // Post-process pass
    postProcessor->endRender(output.framebuffer);
    glViewport(0, 0, output.width, output.height);
    postProcessor->draw(camera.view, camera.projection, MOTION_BLUR_REFERENCE_FPS, settings, scene.getAnimationTime());

    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawSky(Scene& scene, const CameraView& camera, glm::vec3 lightDirection) {
    // Behind everything: drawn first, with no depth, at the internal resolution like the rest.
    const glm::mat4 rotationOnly = camera.projection * glm::mat4(glm::mat3(camera.view));
    skyShader->use();
    skyShader->setMat4("inverseViewProjection", glm::inverse(rotationOnly));
    skyShader->setVec3("horizonColor", scene.getClearColor());
    skyShader->setVec3("lightDirection", lightDirection);
    skyShader->setVec3("lightColor", scene.getLightColor());
    skyShader->setFloat("time", scene.getAnimationTime());

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(skyVertexArray);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::copyDepthTo(const RenderOutput& output) {
    postProcessor->blitDepth(output.framebuffer, output.width, output.height);
}
