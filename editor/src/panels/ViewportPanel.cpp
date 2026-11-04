#include "ViewportPanel.h"
#include "AssetImporter.h"
#include "Theme.h"
#include "Widgets.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {
    const float STRIP_HEIGHT = 34.0f;
    const float MARKER_RADIUS = 11.0f;
}

bool ViewportPanel::project(glm::vec3 world, ImVec2& screen) const {
    const glm::vec4 clip = sceneView.projection * sceneView.view * glm::vec4(world, 1.0f);
    if (clip.w <= 0.0f) return false;

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    screen.x = imageMin.x + (ndc.x * 0.5f + 0.5f) * imageSize.x;
    screen.y = imageMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * imageSize.y;
    return true;
}

glm::vec3 ViewportPanel::groundPointUnderMouse() const {
    const ImVec2 mouse = ImGui::GetMousePos();
    const glm::vec2 ndc(((mouse.x - imageMin.x) / imageSize.x) * 2.0f - 1.0f,
                        1.0f - ((mouse.y - imageMin.y) / imageSize.y) * 2.0f);

    const glm::mat4 inverse = glm::inverse(sceneView.projection * sceneView.view);
    glm::vec4 nearPoint = inverse * glm::vec4(ndc, -1.0f, 1.0f);
    glm::vec4 farPoint = inverse * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::vec3 origin(nearPoint);
    const glm::vec3 direction = glm::normalize(glm::vec3(farPoint - nearPoint));

    if (direction.y < -0.0001f) {
        const float t = -origin.y / direction.y;
        if (t > 0.0f && t < 200.0f) return origin + direction * t;
    }

    // Looking at or above the horizon: in front of the camera instead.
    return camera.getPivot();
}

void ViewportPanel::focusSelection(EditorContext& ctx) {
    SceneObject* object = ctx.getSelected();
    if (!object) return;

    const glm::vec3 size = object->getWorldScale();
    camera.focus(object->getWorldPosition(), 0.6f * std::max({size.x, size.y, size.z}));
}

void ViewportPanel::drawToolStrip(EditorContext& ctx) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + STRIP_HEIGHT), Theme::Panel);

    const float buttonSize = 26.0f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 8.0f, origin.y + (STRIP_HEIGHT - buttonSize) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

    if (ctx.viewMode == ViewMode::Scene) {
        const ImVec2 size(buttonSize, buttonSize);
        if (Widgets::iconButton("##move", Icons::move, operation == ImGuizmo::TRANSLATE, "Move (W)", size)) operation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (Widgets::iconButton("##rotate", Icons::rotate, operation == ImGuizmo::ROTATE, "Rotate (E)", size)) operation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (Widgets::iconButton("##scale", Icons::scale, operation == ImGuizmo::SCALE, "Scale (R)", size)) operation = ImGuizmo::SCALE;

        ImGui::SameLine(0.0f, 12.0f);
        const float segmentWidth = 54.0f;
        const float y = ImGui::GetCursorPosY();
        ImGui::SetCursorPosY(y + (buttonSize - ImGui::GetFrameHeight()) * 0.5f);

        // The manipulator can only scale along an object's own axes.
        ImGui::BeginDisabled(operation == ImGuizmo::SCALE);
        const bool local = gizmoMode == ImGuizmo::LOCAL || operation == ImGuizmo::SCALE;
        if (Widgets::segment("Local", local, segmentWidth, true, false)) gizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine(0.0f, 1.0f);
        if (Widgets::segment("World", !local, segmentWidth, false, true)) gizmoMode = ImGuizmo::WORLD;
        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 8.0f);
        if (Widgets::segment("Snap", snapping, segmentWidth, true, true)) snapping = !snapping;
        if (ImGui::IsItemHovered()) Widgets::tooltip("Move in steps of 0.5, rotate by 15 degrees, scale by 0.1.");

        ImGui::SameLine(0.0f, 12.0f);
        if (Widgets::segment("Persp", !camera.orthographic, segmentWidth, true, false)) camera.orthographic = false;
        if (ImGui::IsItemHovered()) Widgets::tooltip("Perspective view");
        ImGui::SameLine(0.0f, 1.0f);
        if (Widgets::segment("Ortho", camera.orthographic, segmentWidth, false, true)) camera.orthographic = true;
        if (ImGui::IsItemHovered()) Widgets::tooltip("Orthographic view");
    }

    ImGui::PopStyleVar();

    // What the image is really made of, which is the point of a retro renderer.
    const glm::ivec2 internal = ctx.project.graphics.internalSize(renderer.getWidth(), renderer.getHeight());
    char info[64];
    if (internal.x != renderer.getWidth() || internal.y != renderer.getHeight()) {
        std::snprintf(info, sizeof(info), "%d x %d, upscaled to %d x %d", internal.x, internal.y, renderer.getWidth(), renderer.getHeight());
    } else {
        std::snprintf(info, sizeof(info), "%d x %d", renderer.getWidth(), renderer.getHeight());
    }
    const float textWidth = ImGui::CalcTextSize(info).x;
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(origin.x + width - textWidth - 10.0f, origin.y + (STRIP_HEIGHT - ImGui::GetTextLineHeight()) * 0.5f),
        Theme::TextDim, info);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + STRIP_HEIGHT));
}

void ViewportPanel::draw(EditorContext& ctx, bool fullscreen) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (fullscreen) {
        const ImGuiViewport* main = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(main->Pos);
        ImGui::SetNextWindowSize(main->Size);
        flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus;
    }
    // A separate window while it covers the screen, so that the docked tab keeps its place.
    const char* title = fullscreen ? "Fullscreen game###FullscreenGame"
                                   : (ctx.viewMode == ViewMode::Scene ? "Scene###Viewport" : "Game###Viewport");
    visible = ImGui::Begin(title, nullptr, flags);
    ImGui::PopStyleVar(2);

    if (!visible) {
        ImGui::End();
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!fullscreen) drawToolStrip(ctx);

    imageMin = ImGui::GetCursorScreenPos();
    imageSize = ImGui::GetContentRegionAvail();
    imageSize.x = std::max(imageSize.x, 1.0f);
    imageSize.y = std::max(imageSize.y, 1.0f);

    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    renderer.resize((int)(imageSize.x * scale.x), (int)(imageSize.y * scale.y));

    if (ctx.viewMode == ViewMode::Scene) {
        drawSceneView(ctx);
    } else {
        drawGameView(ctx);
    }

    ImGui::PopStyleVar();
    ImGui::End();
}

void ViewportPanel::drawImage() {
    ImGui::SetCursorScreenPos(imageMin);
    // OpenGL's first row is the bottom one, so the image is flipped for display.
    ImGui::Image((ImTextureID)(intptr_t)renderer.texture(), imageSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
}

void ViewportPanel::drawSceneView(EditorContext& ctx) {
    drawImage();
    const bool hovered = ImGui::IsItemHovered();

    const bool cameraBusy = camera.update(ImGui::GetIO().DeltaTime, hovered, imageSize.y);
    sceneView = camera.makeView(imageSize.x / imageSize.y);

    handleDrops(ctx);

    bool sculpting = false;
    if (ctx.terrainBrush.enabled && ctx.play.isStopped()) {
        if (auto* terrain = ctx.scene().findAs<TerrainObject>(ctx.selectedObject)) {
            sculpting = handleTerrainBrush(ctx, *terrain, hovered && !cameraBusy);
        }
    }

    drawObjectMarkers(ctx);
    if (!sculpting) drawGizmo(ctx);

    if (hovered && !cameraBusy && !sculpting) {
        handleClick(ctx);
    }
    if (!cameraBusy && !ImGui::GetIO().WantTextInput) {
        handleShortcuts(ctx);
    }
    if (ctx.focusRequested) {
        ctx.focusRequested = false;
        focusSelection(ctx);
    }
}

bool ViewportPanel::handleTerrainBrush(EditorContext& ctx, TerrainObject& terrain, bool hovered) {
    auto& brush = ctx.terrainBrush;

    // The mouse as a ray, taken into the terrain's own space.
    const ImVec2 mouse = ImGui::GetMousePos();
    const glm::vec2 ndc(((mouse.x - imageMin.x) / imageSize.x) * 2.0f - 1.0f, 1.0f - ((mouse.y - imageMin.y) / imageSize.y) * 2.0f);
    const glm::mat4 inverseView = glm::inverse(sceneView.projection * sceneView.view);
    glm::vec4 nearPoint = inverseView * glm::vec4(ndc, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseView * glm::vec4(ndc, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;

    const glm::mat4 world = terrain.getWorldMatrix();
    const glm::mat4 toLocal = glm::inverse(world);
    const glm::vec3 origin = glm::vec3(toLocal * nearPoint);
    const glm::vec3 direction = glm::vec3(toLocal * farPoint) - origin;

    glm::vec3 hit;
    if (!hovered || !terrain.raycast(origin, direction, hit)) return true;

    // The ring on the ground, following its shape.
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (float fraction : {1.0f, 1.0f - brush.falloff}) {
        if (fraction <= 0.02f) continue;
        std::vector<ImVec2> ring;
        for (int i = 0; i <= 48; i++) {
            const float angle = i / 48.0f * 6.2831853f;
            const float x = hit.x + std::cos(angle) * brush.radius * fraction;
            const float z = hit.z + std::sin(angle) * brush.radius * fraction;
            ImVec2 screen;
            if (project(glm::vec3(world * glm::vec4(x, terrain.heightAt(x, z) + 0.05f, z, 1.0f)), screen)) ring.push_back(screen);
        }
        if (ring.size() > 1) draw->AddPolyline(ring.data(), (int)ring.size(), fraction == 1.0f ? Theme::Accent : Theme::rgb(0xFFFFFF, 90), 0, 1.5f);
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) brush.flattenHeight = hit.y;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt) {
        const float dt = std::min(ImGui::GetIO().DeltaTime, 0.05f);
        if (terrain.applyBrush(brush.mode, glm::vec2(hit.x, hit.z), brush.radius, brush.strength, brush.falloff, dt, brush.flattenHeight, brush.tile)) {
            ctx.document.markDirty();
        }
    }
    return true;
}

void ViewportPanel::drawGameView(EditorContext& ctx) {
    hasGameCamera = ctx.scene().getMainCamera() != nullptr;

    if (hasGameCamera) {
        drawImage();

        // On release, so that the button's up event is not lost to the captured mouse.
        const GameStatus& status = ctx.play.getGameStatus();
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
            !ctx.play.isStopped() && status.hasPlayer) {
            ctx.wantsMouseCapture = true;
        }

        hud.draw(ctx, imageMin, imageSize);
        ui.draw(ctx, imageMin, imageSize);
        return;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
    draw->AddRectFilled(imageMin, max, Theme::Sunken);

    const char* message = "This scene has no camera. Add one from the Create menu to see the Game view.";
    const ImVec2 size = ImGui::CalcTextSize(message);
    draw->AddText(ImVec2(imageMin.x + (imageSize.x - size.x) * 0.5f, imageMin.y + (imageSize.y - size.y) * 0.5f),
                  Theme::TextDim, message);
    ui.draw(ctx, imageMin, imageSize);
}

void ViewportPanel::drawGizmo(EditorContext& ctx) {
    SceneObject* object = ctx.getSelected();
    if (!object) return;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetOrthographic(camera.orthographic);
    ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

    glm::mat4 world = object->getWorldMatrix();

    const float snap[3] = {
        operation == ImGuizmo::TRANSLATE ? 0.5f : (operation == ImGuizmo::ROTATE ? 15.0f : 0.1f),
        operation == ImGuizmo::TRANSLATE ? 0.5f : 0.1f,
        operation == ImGuizmo::TRANSLATE ? 0.5f : 0.1f,
    };

    const ImGuizmo::MODE mode = operation == ImGuizmo::SCALE ? ImGuizmo::LOCAL : gizmoMode;
    const bool changed = ImGuizmo::Manipulate(glm::value_ptr(sceneView.view), glm::value_ptr(sceneView.projection),
                                              operation, mode, glm::value_ptr(world), nullptr, snapping ? snap : nullptr);
    if (!changed) return;

    // Only the component being edited is written back, so that decomposing the
    // matrix does not turn a clean 90 into 89.99999 on the other fields.
    if (operation == ImGuizmo::TRANSLATE) {
        SceneObject* parent = object->getParent();
        const glm::mat4 local = parent ? glm::inverse(parent->getWorldMatrix()) * world : world;
        object->setPosition(glm::vec3(local[3]));
    } else if (operation == ImGuizmo::SCALE) {
        glm::vec3 position, rotation, scale;
        SceneObject* parent = object->getParent();
        decomposeTransform(parent ? glm::inverse(parent->getWorldMatrix()) * world : world, position, rotation, scale);
        object->setScale(scale);
    } else {
        object->setWorldMatrix(world);
    }

    ctx.document.markDirty();
}

void ViewportPanel::drawCameraFrustum(const CameraObject& cameraObject, ImDrawList* draw) {
    const CameraView view = cameraObject.makeView(imageSize.x / imageSize.y);
    const glm::mat4 inverse = glm::inverse(view.projection * view.view);

    // Drawn a short way along the view rather than out to the far plane.
    const float depth = 0.96f;
    const glm::vec3 eye = view.position;

    glm::vec3 corners[4];
    const glm::vec2 ndc[4] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
    for (int i = 0; i < 4; i++) {
        glm::vec4 point = inverse * glm::vec4(ndc[i], depth, 1.0f);
        point /= point.w;
        const glm::vec3 direction = glm::normalize(glm::vec3(point) - eye);
        corners[i] = eye + direction * 2.5f;
    }

    ImVec2 screenEye, screen[4];
    if (!project(eye, screenEye)) return;
    for (int i = 0; i < 4; i++) {
        if (!project(corners[i], screen[i])) return;
    }

    for (int i = 0; i < 4; i++) {
        draw->AddLine(screenEye, screen[i], Theme::AccentDim, 1.0f);
        draw->AddLine(screen[i], screen[(i + 1) % 4], Theme::Accent, 1.0f);
    }
}

void ViewportPanel::drawObjectMarkers(EditorContext& ctx) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);

    for (const auto& object : ctx.scene().getObjects()) {
        const ObjectType type = object->getType();
        if (type != ObjectType::Light && type != ObjectType::Camera && type != ObjectType::Empty && type != ObjectType::AudioSource) continue;

        ImVec2 screen;
        if (!project(object->getWorldPosition(), screen)) continue;

        const bool selected = object->id == ctx.selectedObject;
        const ImU32 color = selected ? Theme::Accent : Theme::Text;

        draw->AddCircleFilled(screen, MARKER_RADIUS + 2.0f, Theme::rgb(0x1C1E21, 170), 20);

        if (type == ObjectType::Light) {
            Icons::light(draw, screen, MARKER_RADIUS - 1.0f, color);
        } else if (type == ObjectType::Camera) {
            Icons::camera(draw, screen, MARKER_RADIUS * 1.5f, color);
            if (selected) {
                drawCameraFrustum(static_cast<const CameraObject&>(*object), draw);
            }
        } else if (type == ObjectType::AudioSource) {
            Icons::speaker(draw, screen, MARKER_RADIUS * 1.6f, color);
            const auto& source = static_cast<const AudioSourceObject&>(*object);
            if (selected && source.spatial) {
                // How far it carries: full volume inside the inner ring, silent outside the outer one.
                const glm::vec3 center = source.getWorldPosition();
                for (float radius : {source.nearDistance, source.farDistance}) {
                    ImVec2 previous;
                    bool havePrevious = false;
                    for (int i = 0; i <= 64; i++) {
                        const float angle = 6.2831853f * float(i) / 64.0f;
                        ImVec2 point;
                        const bool visible = project(center + glm::vec3(std::cos(angle), 0.0f, std::sin(angle)) * radius, point);
                        if (visible && havePrevious) draw->AddLine(previous, point, radius == source.nearDistance ? Theme::Accent : Theme::AccentDim, 1.0f);
                        previous = point;
                        havePrevious = visible;
                    }
                }
            }
        } else {
            draw->AddRect(ImVec2(screen.x - 4.0f, screen.y - 4.0f), ImVec2(screen.x + 4.0f, screen.y + 4.0f), color);
        }
    }

    draw->PopClipRect();
}

ObjectId ViewportPanel::markerAt(EditorContext& ctx, ImVec2 mouse) {
    float closest = (MARKER_RADIUS + 2.0f) * (MARKER_RADIUS + 2.0f);
    ObjectId found = InvalidObjectId;

    for (const auto& object : ctx.scene().getObjects()) {
        const ObjectType type = object->getType();
        if (type != ObjectType::Light && type != ObjectType::Camera && type != ObjectType::Empty && type != ObjectType::AudioSource) continue;

        ImVec2 screen;
        if (!project(object->getWorldPosition(), screen)) continue;

        const float dx = screen.x - mouse.x;
        const float dy = screen.y - mouse.y;
        const float distance = dx * dx + dy * dy;
        if (distance < closest) {
            closest = distance;
            found = object->id;
        }
    }
    return found;
}

void ViewportPanel::handleClick(EditorContext& ctx) {
    if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
    if (ImGui::GetIO().KeyAlt) return;
    if (ctx.selectedObject != InvalidObjectId && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) return;

    const ImVec2 mouse = ImGui::GetMousePos();

    const ObjectId marker = markerAt(ctx, mouse);
    if (marker != InvalidObjectId) {
        ctx.select(marker);
        return;
    }

    // The id pass needs the frame's camera, so it runs with the render step.
    const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
    pendingPickX = (int)((mouse.x - imageMin.x) * scale.x);
    pendingPickY = (int)((mouse.y - imageMin.y) * scale.y);
}

void ViewportPanel::handleShortcuts(EditorContext& ctx) {
    if (!ImGui::IsWindowHovered() && !ImGui::IsWindowFocused()) return;
    if (ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper) return;

    if (ImGui::IsKeyPressed(ImGuiKey_W, false)) operation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false)) operation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) operation = ImGuizmo::SCALE;
    if (ImGui::IsKeyPressed(ImGuiKey_F, false)) focusSelection(ctx);
}

void ViewportPanel::handleDrops(EditorContext& ctx) {
    std::string dropped;

    if (Widgets::acceptAssetDrop(AssetKind::Model, dropped)) {
        ctx.addModel(dropped, groundPointUnderMouse());
    }

    if (Widgets::acceptAssetDrop(AssetKind::Scene, dropped)) {
        ctx.openScene(dropped);
    }

    if (Widgets::acceptAssetDrop(AssetKind::Texture, dropped)) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
        const ObjectId target = renderer.pick(ctx.scene(), sceneView, (int)((mouse.x - imageMin.x) * scale.x),
                                              (int)((mouse.y - imageMin.y) * scale.y));
        if (target != InvalidObjectId) {
            ctx.setObjectTexture(target, dropped);
            ctx.select(target);
        }
    }
}

void ViewportPanel::render(EditorContext& ctx) {
    if (!visible) return;

    if (pendingPickX >= 0) {
        ctx.select(renderer.pick(ctx.scene(), sceneView, pendingPickX, pendingPickY));
        pendingPickX = -1;
        pendingPickY = -1;
    }

    const RenderSettings& settings = ctx.project.graphics;

    if (ctx.viewMode == ViewMode::Scene) {
        renderer.renderScene(ctx.scene(), sceneView, settings, ctx.selectedObject);
    } else if (hasGameCamera) {
        CameraObject* gameCamera = ctx.scene().getMainCamera();
        renderer.renderGame(ctx.scene(), gameCamera->makeView(imageSize.x / imageSize.y), settings);
    }
}
