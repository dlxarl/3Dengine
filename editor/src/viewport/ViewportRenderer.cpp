#include "ViewportRenderer.h"
#include "Resources.h"
#include "Theme.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>

namespace {
    // Well below the engine's default: it covers the same area and the Editor renders continuously.
    const unsigned int SHADOW_RESOLUTION = 4096;
    const float OUTLINE_RADIUS = 2.0f;

    std::string editorShader(const char* fileName) {
        return Resources::path(std::string("editor/shaders/") + fileName);
    }

    glm::vec4 encodeId(ObjectId id) {
        return glm::vec4((id & 0xFF) / 255.0f, ((id >> 8) & 0xFF) / 255.0f, ((id >> 16) & 0xFF) / 255.0f, 1.0f);
    }

    ObjectId decodeId(const unsigned char* pixel) {
        return (ObjectId)pixel[0] | ((ObjectId)pixel[1] << 8) | ((ObjectId)pixel[2] << 16);
    }
}

ViewportRenderer::ViewportRenderer() : renderer(SHADOW_RESOLUTION) {
    gridShader = std::make_unique<Shader>(editorShader("grid.vert").c_str(), editorShader("grid.frag").c_str());
    flatShader = std::make_unique<Shader>(editorShader("flat.vert").c_str(), editorShader("flat.frag").c_str());
    outlineShader = std::make_unique<Shader>(editorShader("fullscreen.vert").c_str(), editorShader("outline.frag").c_str());

    glGenVertexArrays(1, &emptyVAO);
}

ViewportRenderer::~ViewportRenderer() {
    glDeleteVertexArrays(1, &emptyVAO);
}

void ViewportRenderer::resize(int width, int height) {
    output.resize(width, height);
    mask.resize(width, height);
}

void ViewportRenderer::renderGame(SceneGraph& scene, const CameraView& camera, const RenderSettings& settings) {
    if (output.getWidth() == 0) return;

    renderer.render(scene, camera, RenderOutput{output.framebuffer(), output.getWidth(), output.getHeight()}, settings);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportRenderer::renderScene(SceneGraph& scene, const CameraView& camera, const RenderSettings& settings, ObjectId selected) {
    if (output.getWidth() == 0) return;

    const RenderOutput target{output.framebuffer(), output.getWidth(), output.getHeight()};
    renderer.render(scene, camera, target, settings);
    renderer.copyDepthTo(target);

    drawGrid(camera);

    if (selected != InvalidObjectId) {
        drawOutline(scene, camera, selected);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportRenderer::drawGrid(const CameraView& camera) {
    const glm::mat4 viewProjection = camera.projection * camera.view;

    glBindFramebuffer(GL_FRAMEBUFFER, output.framebuffer());
    glViewport(0, 0, output.getWidth(), output.getHeight());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    // Color only: the image's alpha must stay 1, or the panel would show through when displayed.
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);

    gridShader->use();
    gridShader->setMat4("viewProjection", viewProjection);
    gridShader->setMat4("inverseViewProjection", glm::inverse(viewProjection));
    gridShader->setVec3("cameraPos", camera.position);
    // Follows height, so that the lines reach the horizon however far up the camera is.
    gridShader->setFloat("fadeDistance", std::max(60.0f, std::abs(camera.position.y) * 12.0f));

    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
}

void ViewportRenderer::drawSubtree(SceneObject& object) {
    if (auto* shape = dynamic_cast<Shape*>(&object)) {
        if (shape->isVisibleInHierarchy()) {
            shape->draw(*flatShader);
        }
    }
    for (SceneObject* child : object.getChildren()) {
        drawSubtree(*child);
    }
}

void ViewportRenderer::drawOutline(SceneGraph& scene, const CameraView& camera, ObjectId selected) {
    SceneObject* object = scene.find(selected);
    if (!object) return;

    // The silhouette goes to a mask without depth testing: an outline should show the whole
    // object, even where something stands in front of it.
    glBindFramebuffer(GL_FRAMEBUFFER, mask.framebuffer());
    glViewport(0, 0, mask.getWidth(), mask.getHeight());
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    flatShader->use();
    flatShader->setMat4("viewProjection", camera.projection * camera.view);
    flatShader->setVec4("flatColor", glm::vec4(1.0f));
    drawSubtree(*object);

    glBindFramebuffer(GL_FRAMEBUFFER, output.framebuffer());
    glViewport(0, 0, output.getWidth(), output.getHeight());

    outlineShader->use();
    outlineShader->setInt("mask", 0);
    outlineShader->setFloat("radius", OUTLINE_RADIUS);
    const ImVec4 accent = Theme::color(Theme::Accent);
    outlineShader->setVec3("outlineColor", glm::vec3(accent.x, accent.y, accent.z));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mask.colorTexture());

    glBindVertexArray(emptyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_DEPTH_TEST);
}

ObjectId ViewportRenderer::pick(SceneGraph& scene, const CameraView& camera, int x, int y) {
    if (x < 0 || y < 0 || x >= mask.getWidth() || y >= mask.getHeight()) return InvalidObjectId;

    // Each object is drawn with its id as a color, into just the one pixel that matters.
    const int glY = mask.getHeight() - 1 - y;

    glBindFramebuffer(GL_FRAMEBUFFER, mask.framebuffer());
    glViewport(0, 0, mask.getWidth(), mask.getHeight());

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, glY, 1, 1);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    flatShader->use();
    flatShader->setMat4("viewProjection", camera.projection * camera.view);
    for (const auto& shape : scene.getShapes()) {
        if (!shape->isVisibleInHierarchy()) continue;
        flatShader->setVec4("flatColor", encodeId(shape->id));
        shape->draw(*flatShader);
    }

    unsigned char pixel[4] = {0, 0, 0, 0};
    glReadPixels(x, glY, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return decodeId(pixel);
}
