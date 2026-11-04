#include "ModelEditorPanel.h"
#include "ObjectFactory.h"
#include "Theme.h"
#include "Widgets.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <system_error>

namespace fs = std::filesystem;

ModelEditorPanel::ModelEditorPanel() : mesh(EditMesh::cube(1.0f)) {}

void ModelEditorPanel::ensureScene(EditorContext& ctx) {
    if (shape) return;

    auto light = ObjectFactory::create(ObjectType::Light);
    light->setPosition(glm::vec3(6.0f, 10.0f, 8.0f));
    scene.add(light);

    auto model = std::make_shared<MeshShape>();
    model->name = "Model";
    model->setColor(glm::vec3(0.78f));
    model->castShadow = true;
    shape = static_cast<MeshShape*>(scene.add(model));
    (void)ctx;
}

void ModelEditorPanel::syncMesh() {
    if (!meshChanged || !shape) return;
    meshChanged = false;
    shape->setMesh(std::make_shared<GpuMesh>(mesh.toMeshData()), std::string());
}

void ModelEditorPanel::remember() {
    undoStack.push_back(mesh);
    if (undoStack.size() > 40) undoStack.erase(undoStack.begin());
    redoStack.clear();
    dirty = true;
}

void ModelEditorPanel::undo() {
    if (undoStack.empty()) return;
    redoStack.push_back(mesh);
    mesh = std::move(undoStack.back());
    undoStack.pop_back();
    clearSelection();
    meshChanged = true;
    dirty = true;
}

void ModelEditorPanel::redo() {
    if (redoStack.empty()) return;
    undoStack.push_back(mesh);
    mesh = std::move(redoStack.back());
    redoStack.pop_back();
    clearSelection();
    meshChanged = true;
    dirty = true;
}

void ModelEditorPanel::replace(EditMesh newMesh) {
    remember();
    mesh = std::move(newMesh);
    clearSelection();
    meshChanged = true;
}

void ModelEditorPanel::clearSelection() {
    selectedFaces.clear();
    selectedVertices.clear();
}

void ModelEditorPanel::selectAll() {
    selectedFaces.clear();
    for (size_t f = 0; f < mesh.faces.size(); f++) selectedFaces.insert((int)f);
    selectedVertices.clear();
    for (size_t v = 0; v < mesh.positions.size(); v++) selectedVertices.insert((int)v);
}

void ModelEditorPanel::setFaceSelection(const std::set<int>& faces) {
    selectedFaces = faces;
    selectedVertices = mesh.verticesOf(faces);
}

bool ModelEditorPanel::load(EditorContext& ctx, const fs::path& file) {
    EditMesh loaded;
    std::string error;
    if (!EditMesh::loadObj(file, loaded, &error)) {
        ctx.dialogs.error("Cannot open the model", file.filename().u8string() + ": " + error);
        return false;
    }
    mesh = std::move(loaded);
    path = file;
    undoStack.clear();
    redoStack.clear();
    clearSelection();
    meshChanged = true;
    dirty = false;
    framed = false;
    return true;
}

void ModelEditorPanel::save(EditorContext& ctx, const fs::path& file) {
    std::string error;
    if (!mesh.saveObj(file, &error)) {
        ctx.dialogs.error("Cannot save the model", error);
        return;
    }
    path = file;
    dirty = false;
    ctx.assets.invalidate(ctx.assets.toAssetPath(file));
    ctx.setStatus("Saved " + file.filename().u8string());
}

void ModelEditorPanel::addPrimitive(const EditMesh& primitive) {
    remember();
    const int offset = (int)mesh.positions.size();
    mesh.positions.insert(mesh.positions.end(), primitive.positions.begin(), primitive.positions.end());
    std::set<int> added;
    for (EditFace face : primitive.faces) {
        for (int& c : face.corners) c += offset;
        added.insert((int)mesh.faces.size());
        mesh.faces.push_back(std::move(face));
    }
    setFaceSelection(added);
    mode = Select::Face;
    meshChanged = true;
}

bool ModelEditorPanel::project(glm::vec3 point, ImVec2& screen) const {
    const glm::vec4 clip = view.projection * view.view * glm::vec4(point, 1.0f);
    if (clip.w <= 0.0f) return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    screen.x = imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x;
    screen.y = imageMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imageSize.y;
    return true;
}

void ModelEditorPanel::drawOverlay() {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);

    auto at = [&](int vertex, ImVec2& out) { return project(mesh.positions[(size_t)vertex], out); };

    // Selected faces tinted, so that what is chosen can be seen through the shading.
    for (int f : selectedFaces) {
        if (f < 0 || f >= (int)mesh.faces.size()) continue;
        std::vector<ImVec2> outline;
        for (int c : mesh.faces[(size_t)f].corners) {
            ImVec2 p;
            if (at(c, p)) outline.push_back(p);
        }
        if (outline.size() >= 3) draw->AddConvexPolyFilled(outline.data(), (int)outline.size(), Theme::rgb(0x6FBFAE, 90));
    }

    if (showWireframe) {
        for (size_t f = 0; f < mesh.faces.size(); f++) {
            const auto& corners = mesh.faces[f].corners;
            const bool selected = selectedFaces.count((int)f) > 0;
            for (size_t i = 0; i < corners.size(); i++) {
                ImVec2 a, b;
                if (at(corners[i], a) && at(corners[(i + 1) % corners.size()], b)) {
                    draw->AddLine(a, b, selected ? Theme::Accent : Theme::rgb(0xFFFFFF, 70), selected ? 1.5f : 1.0f);
                }
            }
        }
    }

    if (mode == Select::Vertex && showVertices) {
        for (size_t v = 0; v < mesh.positions.size(); v++) {
            ImVec2 p;
            if (!at((int)v, p)) continue;
            const bool selected = selectedVertices.count((int)v) > 0;
            draw->AddCircleFilled(p, selected ? 4.5f : 3.0f, selected ? Theme::Accent : Theme::rgb(0xFFFFFF, 200));
        }
    }
    draw->PopClipRect();
}

void ModelEditorPanel::handleClick(EditorContext& ctx) {
    (void)ctx;
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::GetIO().KeyAlt) return;
    if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;

    const bool additive = ImGui::GetIO().KeyShift;
    const ImVec2 mouse = ImGui::GetMousePos();

    if (mode == Select::Vertex) {
        int nearest = -1;
        float best = 12.0f;
        for (size_t v = 0; v < mesh.positions.size(); v++) {
            ImVec2 p;
            if (!project(mesh.positions[v], p)) continue;
            const float d = std::hypot(p.x - mouse.x, p.y - mouse.y);
            if (d < best) {
                best = d;
                nearest = (int)v;
            }
        }
        if (!additive) selectedVertices.clear();
        if (nearest >= 0) {
            if (additive && selectedVertices.count(nearest)) selectedVertices.erase(nearest);
            else selectedVertices.insert(nearest);
        }
        selectedFaces = mesh.facesUsing(selectedVertices);
        return;
    }

    const glm::vec2 ndc(((mouse.x - imageMin.x) / imageSize.x) * 2.0f - 1.0f, 1.0f - ((mouse.y - imageMin.y) / imageSize.y) * 2.0f);
    const glm::mat4 inverse = glm::inverse(view.projection * view.view);
    glm::vec4 nearPoint = inverse * glm::vec4(ndc, -1.0f, 1.0f);
    glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    float distance;
    const int face = mesh.pick(glm::vec3(nearPoint), glm::normalize(glm::vec3(farPoint - nearPoint)), distance);

    std::set<int> faces = additive ? selectedFaces : std::set<int>();
    if (face >= 0) {
        if (additive && faces.count(face)) faces.erase(face);
        else faces.insert(face);
    }
    setFaceSelection(faces);
}

void ModelEditorPanel::handleGizmo() {
    if (selectedVertices.empty()) {
        wasUsingGizmo = false;
        return;
    }

    glm::vec3 sum(0.0f);
    for (int v : selectedVertices) sum += mesh.positions[(size_t)v];
    const glm::vec3 pivot = sum / float(selectedVertices.size());
    gizmoMatrix = glm::translate(glm::mat4(1.0f), pivot);

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetOrthographic(camera.orthographic);
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

    const glm::mat4 before = gizmoMatrix;
    glm::mat4 working = gizmoMatrix;
    const bool changed = ImGuizmo::Manipulate(glm::value_ptr(view.view), glm::value_ptr(view.projection), operation, ImGuizmo::WORLD,
                                              glm::value_ptr(working));
    const bool using_ = ImGuizmo::IsUsing();
    if (using_ && !wasUsingGizmo) remember();
    wasUsingGizmo = using_;

    if (changed) {
        mesh.transform(selectedVertices, working * glm::inverse(before));
        meshChanged = true;
        dirty = true;
    }
}

void ModelEditorPanel::drawViewport(EditorContext& ctx) {
    imageMin = ImGui::GetCursorScreenPos();
    imageSize = ImGui::GetContentRegionAvail();
    imageSize.x = std::max(imageSize.x, 64.0f);
    imageSize.y = std::max(imageSize.y, 64.0f);

    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    renderer.resize((int)(imageSize.x * scale.x), (int)(imageSize.y * scale.y));

    if (!framed) {
        glm::vec3 low(1e9f), high(-1e9f);
        for (const glm::vec3& p : mesh.positions) {
            low = glm::min(low, p);
            high = glm::max(high, p);
        }
        if (!mesh.positions.empty()) camera.focus((low + high) * 0.5f, std::max(0.5f, glm::length(high - low) * 0.5f));
        framed = true;
    }

    ImGui::InvisibleButton("##modelView", imageSize, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered = ImGui::IsItemHovered();
    const bool busy = camera.update(ImGui::GetIO().DeltaTime, hovered, imageSize.y);
    view = camera.makeView(imageSize.x / imageSize.y);

    syncMesh();
    renderer.renderScene(scene, view, ctx.project.graphics, InvalidObjectId);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddImage((ImTextureID)(intptr_t)renderer.texture(), imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y),
                   ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    drawOverlay();
    handleGizmo();
    if (hovered && !busy) handleClick(ctx);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) operation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) operation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) operation = ImGuizmo::SCALE;
        if (ImGui::IsKeyPressed(ImGuiKey_1, false)) mode = Select::Face;
        if (ImGui::IsKeyPressed(ImGuiKey_2, false)) mode = Select::Vertex;
        if (ImGui::IsKeyPressed(ImGuiKey_X, false) || ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
            if (!selectedFaces.empty()) {
                remember();
                mesh.deleteFaces(selectedFaces);
                clearSelection();
                meshChanged = true;
            }
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !mesh.positions.empty()) camera.focus(mesh.center(), 2.0f);
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) undo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z)) redo();
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S) && !path.empty()) save(ctx, path);
        ctx.textEditorFocused = true;
    }
}

void ModelEditorPanel::drawToolbar(EditorContext& ctx) {
    if (ImGui::Button("Add")) ImGui::OpenPopup("##addPrimitive");
    if (ImGui::BeginPopup("##addPrimitive")) {
        if (ImGui::MenuItem("Cube")) addPrimitive(EditMesh::cube());
        if (ImGui::MenuItem("Plane")) addPrimitive(EditMesh::plane(1.0f, 2));
        if (ImGui::MenuItem("Cylinder")) addPrimitive(EditMesh::cylinder());
        if (ImGui::MenuItem("Cone")) addPrimitive(EditMesh::cone());
        if (ImGui::MenuItem("Sphere")) addPrimitive(EditMesh::sphere());
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("New")) {
        replace(EditMesh::cube());
        path.clear();
        framed = false;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(undoStack.empty());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack.empty());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(path.empty() || !dirty);
    if (ImGui::Button("Save")) save(ctx, path);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Save as")) {
        const fs::path folder = path.empty() ? ctx.project.assetsDir() / "Models" : path.parent_path();
        ctx.dialogs.promptText("Save model as", "File name", path.empty() ? "NewModel" : path.stem().u8string(), "Save",
                               [this, &ctx, folder](const std::string& name) {
            if (name.empty() || name.find_first_of("/\\:") != std::string::npos) return std::string("A file name cannot contain / \\ or :.");
            const fs::path target = folder / fs::u8path(name + ".obj");
            std::error_code ec;
            if (fs::exists(target, ec)) return std::string("A file with this name already exists.");
            fs::create_directories(folder, ec);
            save(ctx, target);
            return std::string();
        });
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(path.empty());
    if (ImGui::Button("Add to scene")) {
        if (dirty) save(ctx, path);
        ctx.addModel(ctx.assets.toAssetPath(path), glm::vec3(0.0f));
    }
    ImGui::EndDisabled();
}

void ModelEditorPanel::drawSidebar(EditorContext& ctx) {
    ImGui::TextUnformatted("Select");
    if (ImGui::RadioButton("Faces", mode == Select::Face)) mode = Select::Face;
    ImGui::SameLine();
    if (ImGui::RadioButton("Vertices", mode == Select::Vertex)) mode = Select::Vertex;
    if (ImGui::Button("All")) selectAll();
    ImGui::SameLine();
    if (ImGui::Button("None")) clearSelection();
    ImGui::SameLine();
    if (ImGui::Button("Invert")) {
        std::set<int> inverted;
        if (mode == Select::Face) {
            for (size_t f = 0; f < mesh.faces.size(); f++) if (!selectedFaces.count((int)f)) inverted.insert((int)f);
            setFaceSelection(inverted);
        } else {
            for (size_t v = 0; v < mesh.positions.size(); v++) if (!selectedVertices.count((int)v)) inverted.insert((int)v);
            selectedVertices = inverted;
            selectedFaces = mesh.facesUsing(inverted);
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Transform");
    if (ImGui::RadioButton("Move", operation == ImGuizmo::TRANSLATE)) operation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", operation == ImGuizmo::ROTATE)) operation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", operation == ImGuizmo::SCALE)) operation = ImGuizmo::SCALE;

    ImGui::Separator();
    ImGui::TextUnformatted("Faces");
    const bool hasFaces = !selectedFaces.empty();
    ImGui::BeginDisabled(!hasFaces);
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragFloat("##extrude", &extrudeAmount, 0.01f, -10.0f, 10.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Extrude")) {
        remember();
        setFaceSelection(mesh.extrude(selectedFaces, extrudeAmount));
        meshChanged = true;
    }
    if (ImGui::Button("Subdivide")) {
        remember();
        setFaceSelection(mesh.subdivide(selectedFaces));
        meshChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip")) {
        remember();
        mesh.flip(selectedFaces);
        meshChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        remember();
        mesh.deleteFaces(selectedFaces);
        clearSelection();
        meshChanged = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextUnformatted("Mesh");
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragFloat("##merge", &mergeDistance, 0.0005f, 0.0f, 1.0f, "%.4f");
    ImGui::SameLine();
    if (ImGui::Button("Merge by distance")) {
        remember();
        const int merged = mesh.mergeByDistance(mergeDistance);
        clearSelection();
        meshChanged = true;
        ctx.setStatus("Merged " + std::to_string(merged) + " vertices");
    }
    ImGui::SetNextItemWidth(90.0f);
    ImGui::DragFloat("##uv", &uvScale, 0.01f, 0.01f, 100.0f, "%.2f");
    ImGui::SameLine();
    if (ImGui::Button("Box UVs")) {
        remember();
        mesh.boxProjectUVs(uvScale);
        meshChanged = true;
    }
    if (ImGui::Checkbox("Smooth shading", &mesh.smooth)) {
        dirty = true;
        meshChanged = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("View");
    ImGui::Checkbox("Wireframe", &showWireframe);
    ImGui::Checkbox("Vertex dots", &showVertices);
    ImGui::TextUnformatted("Texture");
    if (Widgets::assetSlot("modelTexture", texture, AssetKind::Texture) && shape) {
        if (texture.empty()) shape->setTexture("texture_albedo", nullptr);
        else shape->setTexture("texture_albedo", ctx.assets.getTexture(texture));
    }

    ImGui::Separator();
    char info[128];
    std::snprintf(info, sizeof(info), "%zu vertices, %zu faces\n%zu faces selected", mesh.positions.size(), mesh.faces.size(), selectedFaces.size());
    Widgets::dimText(info);
    Widgets::dimText("1 faces, 2 vertices, W E R move rotate scale, X delete, F frame. Shift adds to the selection.");
}

void ModelEditorPanel::draw(EditorContext& ctx, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(940.0f, 620.0f), ImGuiCond_FirstUseEver);
    const std::string title = std::string("Model Editor") + (dirty ? " *" : "") + "###ModelEditor";
    if (!ImGui::Begin(title.c_str(), open)) {
        ImGui::End();
        return;
    }
    ensureScene(ctx);

    if (!ctx.toolRequests.model.empty()) {
        load(ctx, ctx.toolRequests.model);
        ctx.toolRequests.model.clear();
    }

    drawToolbar(ctx);
    ImGui::Separator();

    ImGui::BeginChild("##modelSidebar", ImVec2(250.0f, 0.0f), ImGuiChildFlags_Borders);
    drawSidebar(ctx);
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##modelViewport", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    drawViewport(ctx);
    ImGui::EndChild();

    ImGui::End();
}
