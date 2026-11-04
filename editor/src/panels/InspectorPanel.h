#pragma once
#include "AssetThumbnails.h"
#include "EditorContext.h"

// Shows the selected object or asset. Every control writes straight to the live scene object.
class Player;
class UiObject;

class InspectorPanel {
public:
    void draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open);

private:
    // Listening to a clip from the Inspector, with the settings it has. Its own output, so that
    // it works in the editor outside Play mode.
    AudioEngine preview;
    SoundHandle previewSound = NoSound;
    ObjectId previewOwner = InvalidObjectId;

    void drawObject(EditorContext& ctx, SceneObject& object);
    void drawTransform(EditorContext& ctx, SceneObject& object);
    void drawShape(EditorContext& ctx, Shape& shape);
    void drawLight(EditorContext& ctx, LightObject& light);
    void drawCamera(EditorContext& ctx, CameraObject& camera);
    void drawParticles(EditorContext& ctx, ParticleEmitter& emitter);
    void drawFire(EditorContext& ctx, FireObject& fire);
    void drawWater(EditorContext& ctx, WaterObject& water);
    void drawScript(EditorContext& ctx, SceneObject& object);
    void drawUi(EditorContext& ctx, UiObject& ui);
    void drawPlayer(EditorContext& ctx, Player& player);
    void drawAudioSource(EditorContext& ctx, AudioSourceObject& source);
    void drawAsset(EditorContext& ctx, AssetThumbnails& thumbnails);
};
