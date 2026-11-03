#pragma once
#include "Camera.h"
#include "SceneObject.h"

// A viewpoint placed in the scene; the Game view looks through the first one.
class CameraObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Camera; }

    float fieldOfView = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 200.0f;
    bool orthographic = false;
    float orthographicSize = 5.0f;  // half of the vertical extent

    CameraView makeView(float aspectRatio) const;
};
