#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "RenderSettings.h"
#include "Shader.h"
#include "Underwater.h"
#include <memory>
#include <vector>

// The post-processing stack. The scene is rendered into an offscreen buffer (floating point
// when an effect needs HDR, with a normal buffer when SSAO does), then a chain of passes
// ping-pongs between two buffers:
//
//   SSAO -> underwater -> depth of field -> motion blur -> grade (bloom, tone mapping, color) -> FXAA
//   -> final (lens, vignette, grain and the retro screen) -> output
//
// A pass that is off is not run, and a buffer no enabled pass needs is never allocated.
class PostProcessor {
public:
    PostProcessor(int width, int height);
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    // The size the scene is rendered at; reallocates only when it changes.
    void resize(int width, int height);

    // Chooses the scene buffer's format and attachments for the effects in use. Call before beginRender.
    void prepare(const RenderSettings& settings);

    void beginRender(glm::vec3 clearColor = glm::vec3(0.1f));
    void endRender(unsigned int outputFramebuffer = 0);

    // Runs the chain into the framebuffer endRender bound; the viewport is the output's.
    void draw(const glm::mat4& view, const glm::mat4& projection, float currentFPS, const RenderSettings& settings, float time);

    // The scene buffer takes one color attachment while effects such as water and particles are
    // drawn, which write no normals.
    void beginEffects();
    void endEffects();

    // Whether the camera is under water this frame; the pass runs only when it is.
    void setUnderwater(const UnderwaterView& view) { underwater = view; }

    // Pixel-art upscaling keeps hard edges; 1:1 output keeps the smoother motion blur sampling.
    void setUpscaleNearest(bool nearest);

    // Copies the scene depth into the output, so that overlays can be depth tested against it.
    void blitDepth(unsigned int outputFramebuffer, int outputWidth, int outputHeight);

private:
    struct Target {
        unsigned int fbo = 0;
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
        GLenum format = 0;

        void ensure(int w, int h, GLenum internalFormat);
        void release();
    };

    static constexpr int BLOOM_LEVELS = 5;
    static constexpr int MAX_KERNEL = 32;

    unsigned int sceneFbo = 0;
    unsigned int depthTexture = 0;
    unsigned int normalTexture = 0;
    Target scene;
    Target ping[2];
    Target graded;
    Target occlusion;
    Target bloom[BLOOM_LEVELS];
    unsigned int noiseTexture = 0;
    glm::vec3 kernel[MAX_KERNEL];

    unsigned int VAO = 0, VBO = 0;
    std::unique_ptr<Shader> finalShader;
    std::unique_ptr<Shader> ssaoShader, ssaoBlurShader, bloomDownShader, bloomUpShader;
    std::unique_ptr<Shader> underwaterShader, dofShader, motionShader, gradeShader, fxaaShader;

    int bufferWidth = 0;
    int bufferHeight = 0;
    GLenum colorFormat = GL_RGB8;
    bool hasNormals = false;
    UnderwaterView underwater;
    bool upscaleNearest = false;
    unsigned int outputFramebuffer = 0;

    glm::mat4 prevViewProjection{1.0f};
    bool firstFrame = true;

    void initRenderData();
    void initKernel();
    void attachScene();

    void bindTexture(int unit, unsigned int texture, bool nearest = false) const;
    void drawQuad() const;
    Target& other(unsigned int current);

    unsigned int ssaoPass(unsigned int source, const glm::mat4& view, const glm::mat4& projection, const PostSettings& post, Target& into);
    unsigned int bloomPass(unsigned int source, const PostSettings& post);
};
