#include "Renderer.h"
#include "Resources.h"

#include <cmath>

namespace {
    const int SHADOW_TEX_UNIT = 10;
    const unsigned int MATERIAL_TEX_UNITS = 5;
    const float MOTION_BLUR_REFERENCE_FPS = 60.0f;

    glm::mat4 lightSpaceFor(glm::vec3 lightPos) {
        const float near_plane = 1.0f, far_plane = 200.0f;
        glm::mat4 lightProjection = glm::ortho(-35.0f, 35.0f, -35.0f, 35.0f, near_plane, far_plane);

        // lookAt degenerates when the light sits straight above the origin.
        const glm::vec3 direction = glm::normalize(-lightPos);
        const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        return lightProjection * glm::lookAt(lightPos, glm::vec3(0.0f), up);
    }
}

Renderer::Renderer(unsigned int shadowResolution) {
    lightingShader = std::make_unique<Shader>(Resources::shader("lighting.vert").c_str(),
                                              Resources::shader("lighting.frag").c_str());
    lampShader = std::make_unique<Shader>(Resources::shader("lighting.vert").c_str(),
                                          Resources::shader("lamp.frag").c_str());
    depthShader = std::make_unique<Shader>(Resources::shader("shadow_depth.vert").c_str(),
                                           Resources::shader("shadow_depth.frag").c_str());

    shadowMap = std::make_unique<ShadowMap>(shadowResolution);
    postProcessor = std::make_unique<PostProcessor>(1, 1);

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
    glDeleteSamplers(1, &materialSampler);
    glDeleteTextures(1, &fallbackTexture);
}

void Renderer::applyTextureFilter(TextureFilter filter) {
    if (filter == samplerFilter) return;
    samplerFilter = filter;

    GLint minFilter = GL_LINEAR_MIPMAP_LINEAR;
    GLint magFilter = GL_LINEAR;
    if (filter == TextureFilter::Nearest) {
        minFilter = GL_NEAREST;
        magFilter = GL_NEAREST;
    } else if (filter == TextureFilter::Bilinear) {
        minFilter = GL_LINEAR;
    }

    glSamplerParameteri(materialSampler, GL_TEXTURE_MIN_FILTER, minFilter);
    glSamplerParameteri(materialSampler, GL_TEXTURE_MAG_FILTER, magFilter);
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
    const glm::mat4 lightSpaceMatrix = lightSpaceFor(lightPos);

    const bool useShadows = settings.shadows && settings.lighting;

    // Shadow pass
    if (useShadows) {
        depthShader->use();
        depthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

        shadowMap->bind();
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        scene.drawDepth(*depthShader);

        shadowMap->unbind(output.width, output.height);
    }

    // Scene pass
    postProcessor->resize(internal.x, internal.y);
    postProcessor->setUpscaleNearest(upscaled);
    postProcessor->beginRender(scene.getClearColor());

    lightingShader->use();
    lightingShader->setMat4("projection", camera.projection);
    lightingShader->setMat4("view", camera.view);
    lightingShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

    lightingShader->setVec3("viewPos", camera.position);
    lightingShader->setVec3("lightPos", lightPos);
    lightingShader->setVec3("lightColor", scene.getLightColor());

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

    // Post-process pass
    postProcessor->enabled = settings.motionBlur;
    postProcessor->colorLevels = settings.colorBits >= 8 ? 256.0f : (float)(1 << settings.colorBits);
    postProcessor->dither = settings.dithering;

    postProcessor->endRender(output.framebuffer);
    glViewport(0, 0, output.width, output.height);
    postProcessor->draw(camera.view, camera.projection, MOTION_BLUR_REFERENCE_FPS);

    glEnable(GL_DEPTH_TEST);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::copyDepthTo(const RenderOutput& output) {
    postProcessor->blitDepth(output.framebuffer, output.width, output.height);
}
