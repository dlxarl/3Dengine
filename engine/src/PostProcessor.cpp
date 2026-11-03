#include "PostProcessor.h"
#include "Log.h"
#include "Resources.h"

PostProcessor::PostProcessor(int width, int height) {
    shader = std::make_unique<Shader>(Resources::shader("post_process.vert").c_str(),
                                      Resources::shader("post_process.frag").c_str());
    firstFrame = true;
    prevViewProjection = glm::mat4(1.0f);

    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenFramebuffers(1, &FBO);
    resize(width, height);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log::error("Renderer", "Post-process framebuffer is not complete");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    initRenderData();
}

PostProcessor::~PostProcessor() {
    glDeleteFramebuffers(1, &FBO);
    glDeleteTextures(1, &colorTexture);
    glDeleteTextures(1, &depthTexture);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void PostProcessor::resize(int width, int height) {
    if (width == bufferWidth && height == bufferHeight) return;
    bufferWidth = width;
    bufferHeight = height;

    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
}

void PostProcessor::setUpscaleNearest(bool nearest) {
    const GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
}

void PostProcessor::blitDepth(unsigned int outputFramebuffer, int outputWidth, int outputHeight) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outputFramebuffer);
    glBlitFramebuffer(0, 0, bufferWidth, bufferHeight, 0, 0, outputWidth, outputHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
}

void PostProcessor::beginRender(glm::vec3 clearColor) {
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glViewport(0, 0, bufferWidth, bufferHeight);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
}

void PostProcessor::endRender(unsigned int outputFramebuffer) {
    glBindFramebuffer(GL_FRAMEBUFFER, outputFramebuffer);
    glDisable(GL_DEPTH_TEST);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void PostProcessor::draw(const glm::mat4& view, const glm::mat4& projection, float currentFPS) {
    shader->use();

    glm::mat4 currentViewProjection = projection * view;

    if (firstFrame) {
        prevViewProjection = currentViewProjection;
        firstFrame = false;
    }

    shader->setMat4("inverseViewProjection", glm::inverse(currentViewProjection));
    shader->setMat4("previousViewProjection", prevViewProjection);
    shader->setBool("useMotionBlur", enabled);
    shader->setFloat("fps", currentFPS);
    shader->setFloat("colorLevels", colorLevels);
    shader->setBool("dither", dither);
    shader->setVec2("internalSize", glm::vec2(bufferWidth, bufferHeight));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    shader->setInt("screenTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    shader->setInt("depthTexture", 1);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    prevViewProjection = currentViewProjection;
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