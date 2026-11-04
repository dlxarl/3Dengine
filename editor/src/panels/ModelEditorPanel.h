#pragma once
#include "AssetImporter.h"
#include "EditMesh.h"
#include "EditorCamera.h"
#include "EditorContext.h"

#include "imgui.h"
#include "ImGuizmo.h"

#include "ViewportRenderer.h"

#include <filesystem>
#include <memory>
#include <set>
#include <vector>

// A small modeler: a model is drawn by the same renderer and shaders as the game, and edited by
// selecting faces or vertices, moving, turning and scaling them, extruding, cutting faces up,
// and adding primitives. Saves .obj files.
class ModelEditorPanel {
public:
    ModelEditorPanel();

    void draw(EditorContext& ctx, bool* open);
    bool load(EditorContext& ctx, const std::filesystem::path& file);

private:
    enum class Select { Face, Vertex };

    EditMesh mesh;
    std::filesystem::path path;       // empty for a model that has not been saved yet
    std::vector<EditMesh> undoStack;
    std::vector<EditMesh> redoStack;
    bool meshChanged = true;          // the GPU copy is out of date
    bool dirty = false;

    Select mode = Select::Face;
    std::set<int> selectedFaces;
    std::set<int> selectedVertices;

    ViewportRenderer renderer;
    EditorCamera camera;
    SceneGraph scene;
    MeshShape* shape = nullptr;
    std::string texture;

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    bool wasUsingGizmo = false;
    glm::mat4 gizmoMatrix = glm::mat4(1.0f);
    float extrudeAmount = 0.5f;
    float mergeDistance = 0.001f;
    float uvScale = 1.0f;
    bool showWireframe = true;
    bool showVertices = true;
    bool framed = false;

    ImVec2 imageMin;
    ImVec2 imageSize;
    CameraView view;

    void ensureScene(EditorContext& ctx);
    void syncMesh();
    void remember();
    void undo();
    void redo();
    void replace(EditMesh newMesh);
    void clearSelection();
    void selectAll();
    void setFaceSelection(const std::set<int>& faces);
    void save(EditorContext& ctx, const std::filesystem::path& file);

    void drawToolbar(EditorContext& ctx);
    void drawSidebar(EditorContext& ctx);
    void drawViewport(EditorContext& ctx);
    void drawOverlay();
    void handleClick(EditorContext& ctx);
    void handleGizmo();
    bool project(glm::vec3 point, ImVec2& screen) const;
    void addPrimitive(const EditMesh& primitive);
};
