#pragma once
#include "SceneObject.h"

// The scene's light source. The renderer currently supports one; see SceneGraph::getLight().
class LightObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Light; }

    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;

    // Sunlight: the same direction everywhere, along the object's forward (-Z) axis, with no
    // falloff and shadows that follow the camera. Otherwise it shines from its position.
    bool directional = false;

    glm::vec3 getDirection() const { return -glm::normalize(glm::vec3(model[2])); }
};
