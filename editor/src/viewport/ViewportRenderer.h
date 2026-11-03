#pragma once
#include "Camera.h"
#include "RenderSettings.h"
#include "RenderTarget.h"
#include "Renderer.h"
#include "SceneGraph.h"
#include "Shader.h"

#include <memory>

// Draws a viewport's image: the engine's frame, plus the Editor's overlays in the Scene view.
// All OpenGL work for the viewport lives here; the panel itself only deals in UI.
class ViewportRenderer {
public:
    ViewportRenderer();
    ~ViewportRenderer();

    ViewportRenderer(const ViewportRenderer&) = delete;
    ViewportRenderer& operator=(const ViewportRenderer&) = delete;

    void resize(int width, int height);

    // The engine's frame, as the game would see it.
    void renderGame(SceneGraph& scene, const CameraView& camera, const RenderSettings& settings);

    // The same frame with the ground grid and the selection outline on top.
    void renderScene(SceneGraph& scene, const CameraView& camera, const RenderSettings& settings, ObjectId selected);

    // The object under a pixel, in target coordinates with the origin at the top left.
    ObjectId pick(SceneGraph& scene, const CameraView& camera, int x, int y);

    unsigned int texture() const { return output.colorTexture(); }
    int getWidth() const { return output.getWidth(); }
    int getHeight() const { return output.getHeight(); }

private:
    Renderer renderer;
    RenderTarget output;
    RenderTarget mask;

    std::unique_ptr<Shader> gridShader;
    std::unique_ptr<Shader> flatShader;
    std::unique_ptr<Shader> outlineShader;

    unsigned int emptyVAO = 0;

    void drawGrid(const CameraView& camera);
    void drawOutline(SceneGraph& scene, const CameraView& camera, ObjectId selected);
    void drawSubtree(SceneObject& object);
};
