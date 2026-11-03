#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.h"
#include <memory>

class PostProcessor {
public:
    PostProcessor(int width, int height);
    ~PostProcessor();

    void beginRender(glm::vec3 clearColor = glm::vec3(0.1f));
    void endRender(unsigned int outputFramebuffer = 0);
    void draw(const glm::mat4& view, const glm::mat4& projection, float currentFPS);

    // The size the scene is rendered at; reallocates only when it changes.
    void resize(int width, int height);

    // Pixel-art upscaling keeps hard edges; 1:1 output keeps the smoother motion blur sampling.
    void setUpscaleNearest(bool nearest);

    // Copies the scene depth into the output, so that overlays can be depth tested against it.
    void blitDepth(unsigned int outputFramebuffer, int outputWidth, int outputHeight);

    bool enabled = true;

    float colorLevels = 256.0f;  // quantization levels per channel
    bool dither = false;

private:
    unsigned int FBO;
    unsigned int colorTexture;
    unsigned int depthTexture;
    unsigned int VAO, VBO;
    std::unique_ptr<Shader> shader;

    int bufferWidth = 0;
    int bufferHeight = 0;

    glm::mat4 prevViewProjection;
    bool firstFrame;

    void initRenderData();
};
