#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

#include "Camera.h"
#include "PostProcessor.h"
#include "RenderSettings.h"
#include "Scene.h"
#include "Shader.h"
#include "ShadowMap.h"

// Where a frame ends up: the window (framebuffer 0) or an offscreen target.
struct RenderOutput {
    unsigned int framebuffer = 0;
    int width = 0;
    int height = 0;
};

// The engine's frame pipeline: shadow pass, scene pass at the internal resolution,
// then the post-process pass that upscales into the output.
class Renderer {
public:
    explicit Renderer(unsigned int shadowResolution = 8192);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void render(Scene& scene, const CameraView& camera, const RenderOutput& output, const RenderSettings& settings);

    // Gives the last frame's depth to the output, for overlays drawn on top of it.
    void copyDepthTo(const RenderOutput& output);

private:
    std::unique_ptr<Shader> lightingShader;
    std::unique_ptr<Shader> lampShader;
    std::unique_ptr<Shader> depthShader;

    std::unique_ptr<ShadowMap> shadowMap;
    std::unique_ptr<PostProcessor> postProcessor;

    unsigned int materialSampler = 0;
    unsigned int fallbackTexture = 0;
    TextureFilter samplerFilter = TextureFilter::Trilinear;

    void applyTextureFilter(TextureFilter filter);
    void bindMaterialSamplers(unsigned int sampler);
};
