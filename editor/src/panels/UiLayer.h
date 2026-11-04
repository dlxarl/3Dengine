#pragma once
#include "EditorContext.h"
#include "UiObject.h"

#include "imgui.h"

// Draws the scene's 2D elements over the Game view, in hierarchy order, and tells buttons
// when the mouse is on them.
class UiLayer {
public:
    // imageMin and imageSize are the Game view's image on screen. While editing the elements
    // are drawn as a preview, and the selected one is outlined.
    void draw(EditorContext& ctx, ImVec2 imageMin, ImVec2 imageSize);

private:
    struct Rect {
        ImVec2 min;
        ImVec2 max;
    };

    ImDrawList* list = nullptr;
    ImFont* font = nullptr;
    EditorContext* context = nullptr;
    float scale = 1.0f;

    void visit(SceneObject& object, const Rect& parent, bool interactive);
    void drawElement(UiObject& ui, const Rect& rect, bool interactive);
    Rect layout(const UiObject& ui, const Rect& parent) const;
};
