#include "HierarchyPanel.h"
#include "ObjectFactory.h"
#include "Theme.h"
#include "Widgets.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace {
    const size_t MAX_OPEN_CHILDREN = 40;
    const float ICON_SIZE = 14.0f;

    bool matchesFilter(const std::string& name, const std::string& filter) {
        auto it = std::search(name.begin(), name.end(), filter.begin(), filter.end(), [](char a, char b) {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
        });
        return it != name.end();
    }

    int indexAmongSiblings(EditorContext& ctx, SceneObject& object) {
        const auto& siblings = object.getParent() ? object.getParent()->getChildren() : ctx.scene().getRoots();
        return (int)(std::find(siblings.begin(), siblings.end(), &object) - siblings.begin());
    }
}

void drawCreateObjectItems(EditorContext& ctx, ObjectId parent) {
    static const ObjectType SHAPE_TYPES[] = {ObjectType::Cube, ObjectType::Sphere, ObjectType::Cylinder, ObjectType::Plane};

    if (ImGui::MenuItem("Empty")) ctx.createObject(ObjectType::Empty, parent);
    ImGui::Separator();
    for (ObjectType type : SHAPE_TYPES) {
        if (ImGui::MenuItem(ObjectFactory::typeName(type))) ctx.createObject(type, parent);
    }
    ImGui::Separator();

    // The renderer lights a scene with one light, so a second would silently do nothing.
    const bool hasLight = ctx.scene().getLight() != nullptr;
    if (ImGui::MenuItem("Light", nullptr, false, !hasLight)) ctx.createObject(ObjectType::Light, parent);
    if (hasLight && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        Widgets::tooltip("This scene already has a light, and the renderer uses one.");
    }

    if (ImGui::MenuItem("Camera")) ctx.createObject(ObjectType::Camera, parent);
    if (ImGui::MenuItem("Audio Source")) ctx.createObject(ObjectType::AudioSource, parent);

    if (ImGui::BeginMenu("Effects")) {
        if (ImGui::MenuItem("Particles")) ctx.createObject(ObjectType::Particles, parent);
        if (ImGui::MenuItem("Fire")) ctx.createObject(ObjectType::Fire, parent);
        if (ImGui::MenuItem("Water")) ctx.createObject(ObjectType::Water, parent);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("2D")) {
        for (const char* what : {"Panel", "Text", "Image", "Bar", "Button"}) {
            if (ImGui::MenuItem(what)) ctx.createUi(what, parent);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("HUD")) ctx.createUi("HUD", parent);
        if (ImGui::MenuItem("Menu")) ctx.createUi("Menu", parent);
        ImGui::EndMenu();
    }

    // The game rules drive one player, for the same reason.
    const bool hasPlayer = ctx.scene().getPlayer() != nullptr;
    if (ImGui::MenuItem("Player", nullptr, false, !hasPlayer)) ctx.createObject(ObjectType::Player, parent);
    if (hasPlayer && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        Widgets::tooltip("This scene already has a player.");
    }
}

void HierarchyPanel::draw(EditorContext& ctx, bool* open) {
    if (!ImGui::Begin("Hierarchy", open)) {
        ImGui::End();
        return;
    }

    const float buttonSize = ImGui::GetFrameHeight();
    if (Widgets::iconButton("##create", Icons::plus, false, "Create an object", ImVec2(buttonSize, buttonSize))) {
        ImGui::OpenPopup("CreateObject");
    }
    if (ImGui::BeginPopup("CreateObject")) {
        drawCreateObjectItems(ctx, InvalidObjectId);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    Widgets::inputTextHint("##filter", "Filter by name", filter);

    const ObjectId requested = ctx.takeRenameRequest();
    if (SceneObject* target = ctx.scene().find(requested)) {
        beginRename(*target);
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::BeginChild("##tree", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    pendingMove = PendingMove();
    deleteRequested = false;

    if (ctx.scene().getRoots().empty()) {
        ImGui::SetCursorPosX(10.0f);
        Widgets::dimText("This scene is empty.");
    } else if (filter.empty()) {
        // By index: a row's context menu can add a sibling while this loop is running.
        const auto& roots = ctx.scene().getRoots();
        for (size_t i = 0; i < roots.size(); i++) {
            drawRow(ctx, *roots[i]);
        }
    } else {
        drawFlatList(ctx);
    }

    // Everything under the last row: dropping here moves an object to the top level,
    // and clicking clears the selection.
    const ImVec2 remaining = ImGui::GetContentRegionAvail();
    if (remaining.y > 1.0f) {
        ImGui::InvisibleButton("##background", remaining);
        if (ImGui::IsItemClicked()) ctx.select(InvalidObjectId);

        if (ImGui::BeginPopupContextItem("##backgroundMenu")) {
            drawCreateObjectItems(ctx, InvalidObjectId);
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_OBJECT)) {
                pendingMove.object = *static_cast<const ObjectId*>(payload->Data);
                pendingMove.parent = InvalidObjectId;
                pendingMove.index = -1;
                pendingMove.requested = true;
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(3);

    if (pendingMove.requested && ctx.scene().reparent(pendingMove.object, pendingMove.parent, pendingMove.index)) {
        ctx.document.markDirty();
    }
    if (deleteRequested) {
        ctx.deleteSelected();
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F2) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (SceneObject* selected = ctx.getSelected()) beginRename(*selected);
        }
    }

    ImGui::End();
}

void HierarchyPanel::beginRename(const SceneObject& object) {
    renaming = object.id;
    renameText = object.name;
    renameFocus = true;
}

void HierarchyPanel::commitRename(EditorContext& ctx) {
    SceneObject* object = ctx.scene().find(renaming);
    if (object && !renameText.empty() && renameText != object->name) {
        object->name = renameText;
        ctx.document.markDirty();
    }
    renaming = InvalidObjectId;
}

void HierarchyPanel::drawRowLabel(EditorContext& ctx, SceneObject& object, float x, float y) {
    const bool selected = object.id == ctx.selectedObject;
    const bool dimmed = !object.isVisibleInHierarchy();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    Icons::object(draw, ImVec2(x, y + 1.0f), ICON_SIZE, object.getType(), selected ? Theme::Accent : Theme::TextDim);

    const float textX = x + ICON_SIZE + 6.0f;

    if (object.id == renaming) {
        ImGui::SetCursorScreenPos(ImVec2(textX, y - 3.0f));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionMax().x - (textX - ImGui::GetWindowPos().x) - 6.0f);
        if (renameFocus) {
            ImGui::SetKeyboardFocusHere();
            renameFocus = false;
        }
        const bool submitted = Widgets::inputText("##rename", renameText, ImGuiInputTextFlags_EnterReturnsTrue);
        if (submitted || ImGui::IsItemDeactivated()) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                renaming = InvalidObjectId;
            } else {
                commitRename(ctx);
            }
        }
    } else {
        draw->AddText(ImVec2(textX, y), dimmed ? Theme::TextDim : Theme::Text, object.name.c_str());
    }
}

void HierarchyPanel::handleDrop(EditorContext& ctx, SceneObject& target) {
    if (!ImGui::BeginDragDropTarget()) return;

    const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
        PAYLOAD_OBJECT, ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect);

    if (payload) {
        const ObjectId draggedId = *static_cast<const ObjectId*>(payload->Data);
        SceneObject* dragged = ctx.scene().find(draggedId);

        if (dragged && dragged != &target && !target.isDescendantOf(dragged)) {
            const ImVec2 min = ImGui::GetItemRectMin();
            const ImVec2 max = ImGui::GetItemRectMax();
            const float position = (ImGui::GetMousePos().y - min.y) / (max.y - min.y);

            ImDrawList* draw = ImGui::GetWindowDrawList();
            PendingMove move;
            move.object = draggedId;

            if (position < 0.25f || position > 0.75f) {
                // Between rows: a sibling of the target.
                const bool after = position > 0.75f;
                const float lineY = after ? max.y : min.y;
                draw->AddLine(ImVec2(min.x, lineY), ImVec2(max.x, lineY), Theme::Accent, 2.0f);

                SceneObject* parent = target.getParent();
                int index = indexAmongSiblings(ctx, target) + (after ? 1 : 0);
                if (dragged->getParent() == parent && indexAmongSiblings(ctx, *dragged) < index) {
                    index--;  // the dragged object leaves its old slot first
                }
                move.parent = parent ? parent->id : InvalidObjectId;
                move.index = index;
            } else {
                // On the row: a child of the target.
                draw->AddRect(min, max, Theme::Accent);
                move.parent = target.id;
                move.index = -1;
            }

            if (payload->IsDelivery()) {
                move.requested = true;
                pendingMove = move;
            }
        }
    }
    ImGui::EndDragDropTarget();
}

void HierarchyPanel::drawContextMenu(EditorContext& ctx, SceneObject& object) {
    if (!ImGui::BeginPopupContextItem("##objectMenu")) return;

    ctx.select(object.id);

    if (ImGui::BeginMenu("Create child")) {
        drawCreateObjectItems(ctx, object.id);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Rename", "F2")) beginRename(object);
    if (ImGui::MenuItem("Duplicate", "Cmd+D")) ctx.duplicateSelected();
    if (ImGui::MenuItem("Delete", "Delete")) deleteRequested = true;

    ImGui::EndPopup();
}

void HierarchyPanel::drawRow(EditorContext& ctx, SceneObject& object) {
    ImGui::PushID((int)object.id);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                               ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
    // Groups start open, unless they hold so much that the list would be all one group.
    if (object.getChildren().size() <= MAX_OPEN_CHILDREN) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (object.getChildren().empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (object.id == ctx.selectedObject) flags |= ImGuiTreeNodeFlags_Selected;

    const float labelX = ImGui::GetCursorScreenPos().x + ImGui::GetTreeNodeToLabelSpacing();
    const float rowY = ImGui::GetCursorScreenPos().y + ImGui::GetStyle().FramePadding.y;

    const bool nodeOpen = ImGui::TreeNodeEx("##row", flags);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        ctx.select(object.id);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        beginRename(object);
    }

    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(PAYLOAD_OBJECT, &object.id, sizeof(ObjectId));
        ImGui::TextUnformatted(object.name.c_str());
        ImGui::EndDragDropSource();
    }

    handleDrop(ctx, object);
    drawContextMenu(ctx, object);

    const float rowEndY = ImGui::GetCursorScreenPos().y;
    drawRowLabel(ctx, object, labelX, rowY);
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, rowEndY));

    if (nodeOpen) {
        const auto& children = object.getChildren();
        for (size_t i = 0; i < children.size(); i++) {
            drawRow(ctx, *children[i]);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void HierarchyPanel::drawFlatList(EditorContext& ctx) {
    ImDrawList* draw = ImGui::GetWindowDrawList();

    for (const auto& object : ctx.scene().getObjects()) {
        if (!matchesFilter(object->name, filter)) continue;

        ImGui::PushID((int)object->id);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetTextLineHeight() + 6.0f;

        if (ImGui::Selectable("##row", object->id == ctx.selectedObject, ImGuiSelectableFlags_None, ImVec2(0.0f, height))) {
            ctx.select(object->id);
        }
        drawContextMenu(ctx, *object);

        Icons::object(draw, ImVec2(pos.x + 10.0f, pos.y + 4.0f), ICON_SIZE, object->getType(), Theme::TextDim);
        draw->AddText(ImVec2(pos.x + 30.0f, pos.y + 3.0f), Theme::Text, object->name.c_str());
        ImGui::PopID();
    }
}
