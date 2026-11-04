#pragma once
#include "EditorCamera.h"
#include "EditorContext.h"
#include "GameHud.h"
#include "UiLayer.h"
#include "ViewportRenderer.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include <glm/glm.hpp>

// The Scene and Game views. This class handles interaction and layout; drawing the image
// itself is ViewportRenderer's job.
class ViewportPanel {
public:
    // Fullscreen covers the whole window with the game and nothing else.
    void draw(EditorContext& ctx, bool fullscreen = false);

    // Runs after the panels, so the image shown this frame includes this frame's edits.
    void render(EditorContext& ctx);

private:
    ViewportRenderer renderer;
    GameHud hud;
    UiLayer ui;
    EditorCamera camera;

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE gizmoMode = ImGuizmo::WORLD;
    bool snapping = false;

    bool visible = false;
    bool hasGameCamera = false;

    ImVec2 imageMin;
    ImVec2 imageSize;
    CameraView sceneView;

    int pendingPickX = -1;
    int pendingPickY = -1;

    void drawToolStrip(EditorContext& ctx);
    void drawSceneView(EditorContext& ctx);
    void drawGameView(EditorContext& ctx);
    void drawImage();
    void drawGizmo(EditorContext& ctx);
    void drawObjectMarkers(EditorContext& ctx);
    void drawCameraFrustum(const CameraObject& camera, ImDrawList* draw);
    void handleShortcuts(EditorContext& ctx);
    void handleClick(EditorContext& ctx);
    void handleDrops(EditorContext& ctx);
    void focusSelection(EditorContext& ctx);

    bool project(glm::vec3 world, ImVec2& screen) const;
    glm::vec3 groundPointUnderMouse() const;
    ObjectId markerAt(EditorContext& ctx, ImVec2 mouse);
};
