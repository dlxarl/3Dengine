#include "PostProcessor.h"
#include "Log.h"
#include "Resources.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    // The projection's clip planes, read back from the matrix.
    void clipPlanes(const glm::mat4& projection, float& nearPlane, float& farPlane) {
        nearPlane = projection[3][2] / (projection[2][2] - 1.0f);
        farPlane = projection[3][2] / (projection[2][2] + 1.0f);
        if (!(nearPlane > 0.0f) || !(farPlane > nearPlane)) {
            nearPlane = 0.1f;
            farPlane = 1000.0f;
        }
    }

    std::unique_ptr<Shader> load(const char* fragment) {
        return std::make_unique<Shader>(Resources::shader("post_process.vert").c_str(), Resources::shader(fragment).c_str());
    }

    int toneMapperIndex(ToneMapper mapper) {
        switch (mapper) {
            case ToneMapper::Reinhard: return 1;
            case ToneMapper::Neutral: return 2;
            case ToneMapper::ACES: return 3;
            case ToneMapper::None: break;
        }
        return 0;
    }
}

void PostProcessor::Target::ensure(int w, int h, GLenum internalFormat) {
    w = std::max(1, w);
    h = std::max(1, h);
    if (texture != 0 && w == width && h == height && internalFormat == format) return;

    if (texture == 0) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    const GLenum dataFormat = internalFormat == GL_R8 ? GL_RED : (internalFormat == GL_RGB8 ? GL_RGB : GL_RGBA);
    const GLenum dataType = (internalFormat == GL_RGBA16F) ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataFormat, dataType, nullptr);

    if (fbo == 0) glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    width = w;
    height = h;
    format = internalFormat;
}

void PostProcessor::Target::release() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (texture) glDeleteTextures(1, &texture);
    fbo = texture = 0;
    width = height = 0;
    format = 0;
}

PostProcessor::PostProcessor(int width, int height) {
    finalShader = load("post_process.frag");

    glGenTextures(1, &scene.texture);
    glBindTexture(GL_TEXTURE_2D, scene.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &sceneFbo);
    resize(width, height);
    initRenderData();
}

PostProcessor::~PostProcessor() {
    glDeleteFramebuffers(1, &sceneFbo);
    glDeleteTextures(1, &scene.texture);
    glDeleteTextures(1, &depthTexture);
    if (normalTexture) glDeleteTextures(1, &normalTexture);
    if (noiseTexture) glDeleteTextures(1, &noiseTexture);
    for (Target& target : ping) target.release();
    for (Target& target : bloom) target.release();
    graded.release();
    occlusion.release();
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

// (Re)specifies the scene buffer's storage and attachments; the texture names stay the same.
void PostProcessor::attachScene() {
    const GLenum dataFormat = colorFormat == GL_RGB8 ? GL_RGB : GL_RGBA;
    const GLenum dataType = colorFormat == GL_RGBA16F ? GL_FLOAT : GL_UNSIGNED_BYTE;
    glBindTexture(GL_TEXTURE_2D, scene.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, colorFormat, bufferWidth, bufferHeight, 0, dataFormat, dataType, nullptr);

    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, bufferWidth, bufferHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    if (normalTexture) {
        glBindTexture(GL_TEXTURE_2D, normalTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bufferWidth, bufferHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene.texture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    if (normalTexture)
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normalTexture, 0);

    const GLenum one[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, one);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::error("Renderer", "Post-process framebuffer is not complete");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::resize(int width, int height) {
    if (width == bufferWidth && height == bufferHeight) return;
    bufferWidth = width;
    bufferHeight = height;
    attachScene();
}

void PostProcessor::prepare(const RenderSettings& settings) {
    const PostSettings& post = settings.post;

    const GLenum wanted = post.needsHdr() ? GL_RGBA16F : GL_RGB8;
    const bool wantNormals = post.ssao;

    bool changed = wanted != colorFormat;
    colorFormat = wanted;

    if (wantNormals && normalTexture == 0) {
        glGenTextures(1, &normalTexture);
        glBindTexture(GL_TEXTURE_2D, normalTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        changed = true;
    }
    if (changed) attachScene();

    hasNormals = wantNormals && normalTexture != 0;

    // Unused buffers do not linger: turning an effect off gives its memory back.
    if (!post.ssao) occlusion.release();
    if (!post.bloom) for (Target& level : bloom) level.release();
    if (!post.needsGrade() && !post.fxaa) graded.release();
    if (!post.ssao && !post.depthOfField && !settings.motionBlur && !post.needsGrade() && !post.fxaa) {
        for (Target& target : ping) target.release();
    }
}

void PostProcessor::setUpscaleNearest(bool nearest) {
    upscaleNearest = nearest;
}

void PostProcessor::blitDepth(unsigned int outputFramebuffer, int outputWidth, int outputHeight) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFramebuffer);
    glBlitFramebuffer(0, 0, bufferWidth, bufferHeight, 0, 0, outputWidth, outputHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
}

void PostProcessor::beginEffects() {
    const GLenum one[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, one);
}

void PostProcessor::endEffects() {
    if (!hasNormals) return;
    const GLenum both[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, both);
}

void PostProcessor::beginRender(glm::vec3 clearColor) {
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
    glViewport(0, 0, bufferWidth, bufferHeight);

    if (hasNormals) {
        const GLenum both[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
        glDrawBuffers(2, both);
        // Nothing drawn there has a normal: an alpha of zero marks it, so the sky and the
        // objects that do not write one are left out of the occlusion.
        const float none[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glClearBufferfv(GL_COLOR, 1, none);
    } else {
        const GLenum one[] = {GL_COLOR_ATTACHMENT0};
        glDrawBuffers(1, one);
    }

    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostProcessor::endRender(unsigned int framebuffer) {
    outputFramebuffer = framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glDisable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessor::bindTexture(int unit, unsigned int texture, bool nearest) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    const GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void PostProcessor::drawQuad() const {
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

PostProcessor::Target& PostProcessor::other(unsigned int current) {
    Target& target = ping[ping[0].texture == current ? 1 : 0];
    target.ensure(bufferWidth, bufferHeight, colorFormat);
    return target;
}

unsigned int PostProcessor::ssaoPass(unsigned int source, const glm::mat4& view, const glm::mat4& projection,
                                     const PostSettings& post, Target& into) {
    if (!ssaoShader) {
        ssaoShader = load("ssao.frag");
        ssaoBlurShader = load("ssao_blur.frag");
        initKernel();
    }
    occlusion.ensure(bufferWidth, bufferHeight, GL_R8);
    float nearPlane, farPlane;
    clipPlanes(projection, nearPlane, farPlane);

    glBindFramebuffer(GL_FRAMEBUFFER, occlusion.fbo);
    ssaoShader->use();
    ssaoShader->setMat4("projection", projection);
    ssaoShader->setMat4("invProjection", glm::inverse(projection));
    ssaoShader->setMat4("view", view);
    const int count = post.ssaoSamples >= 32 ? 32 : 16;
    ssaoShader->setInt("sampleCount", count);
    for (int i = 0; i < count; i++) ssaoShader->setVec3("kernel[" + std::to_string(i) + "]", kernel[i]);
    ssaoShader->setFloat("radius", post.ssaoRadius);
    ssaoShader->setFloat("bias", post.ssaoBias);
    ssaoShader->setVec2("noiseScale", glm::vec2(bufferWidth, bufferHeight) / 4.0f);
    bindTexture(0, depthTexture, true);
    ssaoShader->setInt("depthTexture", 0);
    bindTexture(1, normalTexture, true);
    ssaoShader->setInt("normalTexture", 1);
    bindTexture(2, noiseTexture, true);
    ssaoShader->setInt("noiseTexture", 2);
    drawQuad();

    // A 4x4 blur, as wide as the noise tile, that does not cross depth edges; it multiplies
    // the occlusion into the image as it goes.
    glBindFramebuffer(GL_FRAMEBUFFER, into.fbo);
    ssaoBlurShader->use();
    ssaoBlurShader->setVec2("texelSize", glm::vec2(1.0f / bufferWidth, 1.0f / bufferHeight));
    ssaoBlurShader->setFloat("nearPlane", nearPlane);
    ssaoBlurShader->setFloat("farPlane", farPlane);
    ssaoBlurShader->setFloat("intensity", post.ssaoIntensity);
    bindTexture(0, source);
    ssaoBlurShader->setInt("screenTexture", 0);
    bindTexture(1, occlusion.texture, true);
    ssaoBlurShader->setInt("occlusionTexture", 1);
    bindTexture(2, depthTexture, true);
    ssaoBlurShader->setInt("depthTexture", 2);
    drawQuad();
    return into.texture;
}

// Thresholds into half resolution, blurs down a pyramid and tents back up, adding as it climbs.
unsigned int PostProcessor::bloomPass(unsigned int source, const PostSettings& post) {
    if (!bloomDownShader) {
        bloomDownShader = load("bloom_down.frag");
        bloomUpShader = load("bloom_up.frag");
    }
    int w = bufferWidth, h = bufferHeight;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
        bloom[i].ensure(w, h, GL_RGBA16F);
    }

    bloomDownShader->use();
    unsigned int from = source;
    int fromW = bufferWidth, fromH = bufferHeight;
    for (int i = 0; i < BLOOM_LEVELS; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloom[i].fbo);
        glViewport(0, 0, bloom[i].width, bloom[i].height);
        bloomDownShader->setBool("prefilter", i == 0);
        bloomDownShader->setFloat("threshold", post.bloomThreshold);
        bloomDownShader->setFloat("knee", post.bloomThreshold * post.bloomSoftKnee);
        bloomDownShader->setVec2("texelSize", glm::vec2(1.0f / fromW, 1.0f / fromH));
        bindTexture(0, from);
        bloomDownShader->setInt("screenTexture", 0);
        drawQuad();
        from = bloom[i].texture;
        fromW = bloom[i].width;
        fromH = bloom[i].height;
    }

    bloomUpShader->use();
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    bloomUpShader->setFloat("spread", 0.5f + post.bloomScatter);
    bloomUpShader->setFloat("weight", 0.35f + 0.65f * post.bloomScatter);
    for (int i = BLOOM_LEVELS - 1; i > 0; i--) {
        glBindFramebuffer(GL_FRAMEBUFFER, bloom[i - 1].fbo);
        glViewport(0, 0, bloom[i - 1].width, bloom[i - 1].height);
        bloomUpShader->setVec2("texelSize", glm::vec2(1.0f / bloom[i].width, 1.0f / bloom[i].height));
        bindTexture(0, bloom[i].texture);
        bloomUpShader->setInt("screenTexture", 0);
        drawQuad();
    }
    glDisable(GL_BLEND);
    glViewport(0, 0, bufferWidth, bufferHeight);
    return bloom[0].texture;
}

void PostProcessor::draw(const glm::mat4& view, const glm::mat4& projection, float currentFPS,
                         const RenderSettings& settings, float time) {
    const PostSettings& post = settings.post;

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    const glm::mat4 currentViewProjection = projection * view;
    if (firstFrame) {
        prevViewProjection = currentViewProjection;
        firstFrame = false;
    }

    float nearPlane, farPlane;
    clipPlanes(projection, nearPlane, farPlane);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, bufferWidth, bufferHeight);

    unsigned int current = scene.texture;

    if (hasNormals) {
        Target& into = other(current);
        current = ssaoPass(current, view, projection, post, into);
    }

    if (underwater.active) {
        if (!underwaterShader) underwaterShader = load("underwater.frag");
        Target& into = other(current);
        glBindFramebuffer(GL_FRAMEBUFFER, into.fbo);
        underwaterShader->use();
        underwaterShader->setFloat("nearPlane", nearPlane);
        underwaterShader->setFloat("farPlane", farPlane);
        underwaterShader->setFloat("time", time);
        underwaterShader->setVec3("tint", underwater.tint);
        underwaterShader->setFloat("density", underwater.density);
        underwaterShader->setFloat("depth", underwater.depth);
        bindTexture(0, current);
        underwaterShader->setInt("screenTexture", 0);
        bindTexture(1, depthTexture, true);
        underwaterShader->setInt("depthTexture", 1);
        drawQuad();
        current = into.texture;
    }

    if (post.depthOfField) {
        if (!dofShader) dofShader = load("dof.frag");
        Target& into = other(current);
        glBindFramebuffer(GL_FRAMEBUFFER, into.fbo);
        dofShader->use();
        dofShader->setFloat("nearPlane", nearPlane);
        dofShader->setFloat("farPlane", farPlane);
        dofShader->setFloat("focalDistance", post.focalDistance);
        dofShader->setFloat("focalRange", post.focalRange);
        dofShader->setFloat("maxBlurRadius", post.maxBlurRadius);
        dofShader->setVec2("texelSize", glm::vec2(1.0f / bufferWidth, 1.0f / bufferHeight));
        bindTexture(0, current);
        dofShader->setInt("screenTexture", 0);
        bindTexture(1, depthTexture, true);
        dofShader->setInt("depthTexture", 1);
        drawQuad();
        current = into.texture;
    }

    if (settings.motionBlur && post.motionBlurStrength > 0.0f) {
        if (!motionShader) motionShader = load("motion_blur.frag");
        Target& into = other(current);
        glBindFramebuffer(GL_FRAMEBUFFER, into.fbo);
        motionShader->use();
        motionShader->setMat4("inverseViewProjection", glm::inverse(currentViewProjection));
        motionShader->setMat4("previousViewProjection", prevViewProjection);
        motionShader->setFloat("fps", currentFPS);
        motionShader->setFloat("strength", post.motionBlurStrength);
        bindTexture(0, current);
        motionShader->setInt("screenTexture", 0);
        bindTexture(1, depthTexture, true);
        motionShader->setInt("depthTexture", 1);
        drawQuad();
        current = into.texture;
    }

    if (post.needsGrade()) {
        if (!gradeShader) gradeShader = load("grade.frag");
        const unsigned int glow = post.bloom ? bloomPass(current, post) : 0;

        graded.ensure(bufferWidth, bufferHeight, GL_RGBA8);
        glBindFramebuffer(GL_FRAMEBUFFER, graded.fbo);
        glViewport(0, 0, bufferWidth, bufferHeight);
        gradeShader->use();
        bindTexture(0, current);
        gradeShader->setInt("screenTexture", 0);
        bindTexture(1, glow ? glow : current);
        gradeShader->setInt("bloomTexture", 1);
        gradeShader->setBool("useBloom", glow != 0);
        gradeShader->setFloat("bloomIntensity", post.bloomIntensity);
        gradeShader->setInt("toneMapper", toneMapperIndex(post.toneMapper));
        gradeShader->setBool("useAdjustments", post.colorAdjustments);
        gradeShader->setFloat("exposure", std::exp2(post.exposure));
        gradeShader->setFloat("contrast", 1.0f + post.contrast);
        gradeShader->setVec3("colorFilter", post.colorFilter);
        gradeShader->setBool("useWhiteBalance", post.whiteBalance);
        gradeShader->setFloat("temperature", post.temperature);
        gradeShader->setFloat("tint", post.tint);
        gradeShader->setBool("useSplitToning", post.splitToning);
        gradeShader->setVec3("shadowsTint", post.shadowsTint);
        gradeShader->setVec3("highlightsTint", post.highlightsTint);
        gradeShader->setFloat("splitBalance", post.splitBalance);
        drawQuad();
        current = graded.texture;
    }

    if (post.fxaa) {
        if (!fxaaShader) fxaaShader = load("fxaa.frag");
        Target& into = other(current);
        glBindFramebuffer(GL_FRAMEBUFFER, into.fbo);
        fxaaShader->use();
        fxaaShader->setVec2("texelSize", glm::vec2(1.0f / bufferWidth, 1.0f / bufferHeight));
        bindTexture(0, current);
        fxaaShader->setInt("screenTexture", 0);
        drawQuad();
        current = into.texture;
    }

    // The final pass: the lens, the vignette and the retro screen, into the output.
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    finalShader->use();
    finalShader->setFloat("colorLevels", settings.colorBits >= 8 ? 256.0f : (float)(1 << settings.colorBits));
    finalShader->setBool("dither", settings.dithering);
    finalShader->setFloat("vignette", settings.vignette);
    finalShader->setFloat("vignetteSmoothness", post.vignetteSmoothness);
    finalShader->setFloat("vignetteRoundness", post.vignetteRoundness);
    finalShader->setFloat("aspect", viewport[3] > 0 ? (float)viewport[2] / (float)viewport[3] : 1.0f);
    finalShader->setFloat("saturation", settings.saturation);
    finalShader->setFloat("scanlines", settings.scanlines);
    finalShader->setFloat("crtCurvature", settings.crtCurvature);
    finalShader->setFloat("chromaticAberration", settings.chromaticAberration);
    finalShader->setFloat("filmGrain", settings.filmGrain);
    finalShader->setFloat("bloom", settings.bloom);
    finalShader->setFloat("distortion", post.lensDistortion ? post.distortion : 0.0f);
    finalShader->setFloat("time", time);
    finalShader->setVec2("internalSize", glm::vec2(bufferWidth, bufferHeight));
    bindTexture(0, current, upscaleNearest);
    finalShader->setInt("screenTexture", 0);
    drawQuad();

    glActiveTexture(GL_TEXTURE0);
    prevViewProjection = currentViewProjection;
}

void PostProcessor::initKernel() {
    // Samples in a hemisphere around +Z, packed towards the center, so that near occluders count most.
    std::mt19937 random(1337);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    for (int i = 0; i < MAX_KERNEL; i++) {
        glm::vec3 sample(unit(random) * 2.0f - 1.0f, unit(random) * 2.0f - 1.0f, unit(random));
        sample = glm::normalize(sample) * unit(random);
        const float scale = (float)i / MAX_KERNEL;
        sample *= 0.1f + 0.9f * scale * scale;
        kernel[i] = sample;
    }

    // Every 4x4 block turns the kernel differently; the blur takes the pattern out again.
    glm::vec3 noise[16];
    for (glm::vec3& n : noise) n = glm::vec3(unit(random) * 2.0f - 1.0f, unit(random) * 2.0f - 1.0f, 0.0f);
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void PostProcessor::initRenderData() {
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
}
