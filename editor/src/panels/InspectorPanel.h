#pragma once
#include "AssetThumbnails.h"
#include "EditorContext.h"

// Shows the selected object or asset. Every control writes straight to the live scene object.
class InspectorPanel {
public:
    void draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open);

private:
    void drawObject(EditorContext& ctx, SceneObject& object);
    void drawTransform(EditorContext& ctx, SceneObject& object);
    void drawShape(EditorContext& ctx, Shape& shape);
    void drawLight(EditorContext& ctx, LightObject& light);
    void drawCamera(EditorContext& ctx, CameraObject& camera);
    void drawAsset(EditorContext& ctx, AssetThumbnails& thumbnails);
};
