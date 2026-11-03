#pragma once
#include "EditorContext.h"

#include <string>
#include <vector>

// The scene as a tree: selecting, creating, renaming, deleting and rearranging objects.
class HierarchyPanel {
public:
    void draw(EditorContext& ctx, bool* open);

    // The scene changes only between rows being drawn, never during the walk over them.
    struct PendingMove {
        ObjectId object = InvalidObjectId;
        ObjectId parent = InvalidObjectId;
        int index = -1;
        bool requested = false;
    };

private:
    ObjectId renaming = InvalidObjectId;
    std::string renameText;
    bool renameFocus = false;

    std::string filter;
    PendingMove pendingMove;
    bool deleteRequested = false;

    void drawRow(EditorContext& ctx, SceneObject& object);
    void drawFlatList(EditorContext& ctx);
    void drawContextMenu(EditorContext& ctx, SceneObject& object);
    void drawRowLabel(EditorContext& ctx, SceneObject& object, float x, float y);
    void handleDrop(EditorContext& ctx, SceneObject& target);
    void beginRename(const SceneObject& object);
    void commitRename(EditorContext& ctx);
};

// Create menu entries, shared with the main menu bar.
void drawCreateObjectItems(EditorContext& ctx, ObjectId parent);
