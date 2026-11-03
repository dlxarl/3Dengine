#pragma once
#include <glad/glad.h>

// An offscreen color + depth framebuffer whose color texture can be displayed or sampled.
class RenderTarget {
public:
    RenderTarget();
    ~RenderTarget();

    RenderTarget(const RenderTarget&) = delete;
    RenderTarget& operator=(const RenderTarget&) = delete;

    // Reallocates only when the size actually changes.
    void resize(int newWidth, int newHeight);

    unsigned int framebuffer() const { return FBO; }
    unsigned int colorTexture() const { return color; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

private:
    unsigned int FBO = 0;
    unsigned int color = 0;
    unsigned int depth = 0;
    int width = 0;
    int height = 0;
};
