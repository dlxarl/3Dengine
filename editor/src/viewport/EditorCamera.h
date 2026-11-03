#pragma once
#include "Camera.h"

#include <glm/glm.hpp>

// The Scene view's camera. It belongs to the Editor, not the scene: it is never saved,
// and Game view looks through a CameraObject instead.
class EditorCamera {
public:
    bool orthographic = false;

    CameraView makeView(float aspectRatio) const;

    // Input for one frame. Returns true while the camera has the mouse.
    bool update(float deltaTime, bool viewportHovered, float viewportHeight);

    // Frames a sphere around center.
    void focus(glm::vec3 center, float radius);

    glm::vec3 getPosition() const;
    glm::vec3 getPivot() const { return pivot; }

private:
    glm::vec3 pivot = glm::vec3(0.0f, 0.5f, 0.0f);
    float distance = 11.0f;
    float yaw = -125.0f;   // degrees around Y; -90 looks down -Z
    float pitch = -24.0f;  // degrees; negative looks down
    float fieldOfView = 50.0f;
    float orthographicSize = 6.0f;
    float flySpeed = 6.0f;

    enum class Drag { None, Look, Orbit, Pan };
    Drag drag = Drag::None;

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    void pan(float deltaX, float deltaY, float viewportHeight);
    void zoom(float wheel);
};
