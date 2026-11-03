#pragma once
#include "SceneObject.h"

// The scene's light source. The renderer currently supports one; see SceneGraph::getLight().
class LightObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Light; }

    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
};
