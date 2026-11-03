#include "InspectorPanel.h"
#include "AssetImporter.h"
#include "ObjectFactory.h"
#include "Theme.h"
#include "Widgets.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    bool beginProperties(const char* id) {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    std::string formatSize(uintmax_t bytes) {
        char buffer[32];
        if (bytes < 1024) {
            std::snprintf(buffer, sizeof(buffer), "%d bytes", (int)bytes);
        } else if (bytes < 1024 * 1024) {
            std::snprintf(buffer, sizeof(buffer), "%.1f KB", bytes / 1024.0);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.1f MB", bytes / (1024.0 * 1024.0));
        }
        return buffer;
    }
}

void InspectorPanel::draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open) {
    if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return;
    }

    if (SceneObject* object = ctx.getSelected()) {
        drawObject(ctx, *object);
    } else if (!ctx.selectedAsset.empty()) {
        drawAsset(ctx, thumbnails);
    } else {
        Widgets::wrappedDim("Select an object in the Hierarchy or the Scene view to edit it here.");
    }

    ImGui::End();
}

void InspectorPanel::drawObject(EditorContext& ctx, SceneObject& object) {
    ImGui::PushID((int)object.id);

    const ImVec2 iconPos = ImGui::GetCursorScreenPos();
    Icons::object(ImGui::GetWindowDrawList(), ImVec2(iconPos.x, iconPos.y + 3.0f), 18.0f, object.getType(), Theme::Accent);
    ImGui::SetCursorScreenPos(ImVec2(iconPos.x + 26.0f, iconPos.y));

    ImGui::SetNextItemWidth(-FLT_MIN);
    std::string name = object.name;
    Widgets::inputText("##name", name);
    if (ImGui::IsItemDeactivatedAfterEdit() && !name.empty()) {
        object.name = name;
        ctx.document.markDirty();
    } else if (ImGui::IsItemActive() && !name.empty()) {
        object.name = name;  // live, so the Hierarchy follows the typing
    }

    if (ImGui::Checkbox("Visible", &object.visible)) {
        ctx.document.markDirty();
    }
    ImGui::SameLine();
    Widgets::dimText(ObjectFactory::typeName(object.getType()));

    drawTransform(ctx, object);

    if (auto* shape = dynamic_cast<Shape*>(&object)) {
        drawShape(ctx, *shape);
    } else if (auto* light = dynamic_cast<LightObject*>(&object)) {
        drawLight(ctx, *light);
    } else if (auto* camera = dynamic_cast<CameraObject*>(&object)) {
        drawCamera(ctx, *camera);
    }

    ImGui::PopID();
}

void InspectorPanel::drawTransform(EditorContext& ctx, SceneObject& object) {
    if (!Widgets::section("Transform")) return;

    if (beginProperties("##transform")) {
        glm::vec3 position = object.position;
        if (Widgets::vec3Row("Position", position, 0.05f, 0.0f)) {
            object.setPosition(position);
            ctx.document.markDirty();
        }

        glm::vec3 rotation = object.rotation;
        if (Widgets::vec3Row("Rotation", rotation, 0.5f, 0.0f)) {
            object.setRotation(rotation);
            ctx.document.markDirty();
        }

        glm::vec3 scale = object.scale;
        if (Widgets::vec3Row("Scale", scale, 0.02f, 1.0f)) {
            object.setScale(scale);
            ctx.document.markDirty();
        }
        ImGui::EndTable();
    }
}

void InspectorPanel::drawShape(EditorContext& ctx, Shape& shape) {
    if (Widgets::section("Rendering")) {
        if (beginProperties("##rendering")) {
            Widgets::propertyLabel("Color");
            glm::vec3 color = shape.getColor();
            if (ImGui::ColorEdit3("##color", &color.x, ImGuiColorEditFlags_Float)) {
                shape.setColor(color);
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Texture", "The surface image. Drag a texture here from the Project panel.");
            const std::shared_ptr<Texture> texture = shape.getTexture("texture_albedo");
            std::string assetPath = texture ? texture->assetPath : std::string();
            if (Widgets::assetSlot("texture", assetPath, AssetKind::Texture)) {
                ctx.setObjectTexture(shape.id, assetPath);
            }

            if (texture) {
                Widgets::propertyLabel("Tint texture", "Multiplies the texture by the color above.");
                if (ImGui::Checkbox("##tint", &shape.tintTexture)) {
                    ctx.document.markDirty();
                }
            }

            Widgets::propertyLabel("Cast shadows");
            if (ImGui::Checkbox("##castShadow", &shape.castShadow)) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }
    }

    if (Widgets::section("Physics")) {
        if (beginProperties("##physics")) {
            Widgets::propertyLabel("Static", "Static objects never move during play.");
            if (ImGui::Checkbox("##static", &shape.isStatic)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Gravity", "Falls during play. Has no effect on static objects.");
            if (ImGui::Checkbox("##gravity", &shape.useGravity)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Collision");
            if (ImGui::Checkbox("##collision", &shape.hasCollision)) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }
    }

    drawGameplay(ctx, shape);
}

void InspectorPanel::drawGameplay(EditorContext& ctx, Shape& shape) {
    if (!Widgets::section("Gameplay")) return;

    if (shape.getType() == ObjectType::Player) {
        Widgets::wrappedDim("Controlled with the keyboard in the Game view. It starts from where you place it.");
        return;
    }

    static const char* const ROLE_LABELS[] = {"None", "Pickup", "Goal", "Hazard"};
    static const char* const ROLE_HINTS[] = {
        nullptr,
        "Collected by touching it. All of them open the goal.",
        "Reaching it wins the game, once every pickup is collected.",
        "Touching it sends the player back to the start.",
    };

    if (beginProperties("##gameplay")) {
        Widgets::propertyLabel("Role");
        int role = (int)shape.gameRole;
        if (ImGui::Combo("##role", &role, ROLE_LABELS, 4)) {
            shape.gameRole = (GameRole)role;
            ctx.document.markDirty();
        }

        if (shape.gameRole == GameRole::Hazard) {
            if (Widgets::vec3Row("Patrol", shape.patrolOffset, 0.05f, 0.0f)) {
                ctx.document.markDirty();
            }
            if (ImGui::IsItemHovered()) Widgets::tooltip("How far it swings either side of its position.");

            Widgets::propertyLabel("Patrol seconds", "How long one full swing takes.");
            if (ImGui::DragFloat("##patrolSeconds", &shape.patrolSeconds, 0.05f, 0.1f, 60.0f, "%.2f")) {
                ctx.document.markDirty();
            }
        }
        ImGui::EndTable();
    }

    if (const char* hint = ROLE_HINTS[(int)shape.gameRole]) {
        ImGui::Spacing();
        Widgets::wrappedDim(hint);
    }
}

void InspectorPanel::drawLight(EditorContext& ctx, LightObject& light) {
    if (Widgets::section("Light")) {
        if (beginProperties("##light")) {
            Widgets::propertyLabel("Color");
            if (ImGui::ColorEdit3("##lightColor", &light.color.x, ImGuiColorEditFlags_Float)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Intensity");
            if (ImGui::DragFloat("##intensity", &light.intensity, 0.01f, 0.0f, 20.0f, "%.2f")) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }

        if (ctx.scene().getLight() != &light) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::Warning);
            ImGui::TextWrapped("The renderer uses one light, and this is not it. The first light in the scene is used.");
            ImGui::PopStyleColor();
        }
    }
}

void InspectorPanel::drawCamera(EditorContext& ctx, CameraObject& camera) {
    if (Widgets::section("Camera")) {
        if (beginProperties("##camera")) {
            Widgets::propertyLabel("First person", "In a game, look through the player's eyes instead of following.");
            if (ImGui::Checkbox("##firstPerson", &camera.firstPerson)) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Orthographic");
            if (ImGui::Checkbox("##orthographic", &camera.orthographic)) {
                ctx.document.markDirty();
            }

            if (camera.orthographic) {
                Widgets::propertyLabel("Size", "Half of the visible height, in scene units.");
                if (ImGui::DragFloat("##size", &camera.orthographicSize, 0.05f, 0.01f, 1000.0f, "%.2f")) {
                    ctx.document.markDirty();
                }
            } else {
                Widgets::propertyLabel("Field of view");
                if (ImGui::DragFloat("##fov", &camera.fieldOfView, 0.25f, 1.0f, 170.0f, "%.1f")) {
                    ctx.document.markDirty();
                }
            }

            Widgets::propertyLabel("Near plane");
            if (ImGui::DragFloat("##near", &camera.nearPlane, 0.01f, 0.001f, camera.farPlane, "%.3f")) {
                ctx.document.markDirty();
            }

            Widgets::propertyLabel("Far plane");
            if (ImGui::DragFloat("##far", &camera.farPlane, 1.0f, camera.nearPlane, 100000.0f, "%.1f")) {
                ctx.document.markDirty();
            }
            ImGui::EndTable();
        }

        if (ctx.scene().getMainCamera() != &camera) {
            ImGui::Spacing();
            Widgets::wrappedDim("The Game view looks through the first camera in the scene, which is not this one.");
        }
    }
}

void InspectorPanel::drawAsset(EditorContext& ctx, AssetThumbnails& thumbnails) {
    const fs::path file = ctx.assets.resolve(ctx.selectedAsset);

    std::error_code ec;
    if (file.empty() || !fs::exists(file, ec)) {
        ctx.selectedAsset.clear();
        return;
    }

    ImGui::PushFont(nullptr, 16.0f);
    ImGui::TextWrapped("%s", file.filename().u8string().c_str());
    ImGui::PopFont();

    const AssetKind kind = classifyAsset(file);

    if (kind == AssetKind::Texture) {
        if (const unsigned int preview = thumbnails.get(ctx.selectedAsset)) {
            ImGui::Spacing();
            const float side = std::min(ImGui::GetContentRegionAvail().x, 220.0f);
            ImGui::Image((ImTextureID)(intptr_t)preview, ImVec2(side, side));
        }
    }

    if (Widgets::section("File")) {
        if (beginProperties("##file")) {
            Widgets::propertyLabel("Path");
            ImGui::TextWrapped("%s", ctx.selectedAsset.c_str());

            if (kind != AssetKind::Folder) {
                Widgets::propertyLabel("Size");
                ImGui::TextUnformatted(formatSize(fs::file_size(file, ec)).c_str());
            }
            ImGui::EndTable();
        }
    }
}
