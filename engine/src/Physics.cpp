#include "Physics.h"

void Physics::step(const std::vector<std::shared_ptr<Shape>>& shapes, float deltaTime) {
    float gravity = -19.6f;

    for (auto& object : shapes) {
        if (object->isStatic) continue;

        // A Player moves itself, through Player::update().
        if (object->getType() == ObjectType::Player) continue;

        if (object->useGravity)
            object->velocity.y += gravity * deltaTime;

        glm::vec3 oldPosition = object->position;

        // Velocity is in world space; a child's position is relative to its parent.
        glm::vec3 worldStep = object->velocity * deltaTime;
        if (SceneObject* parent = object->getParent()) {
            worldStep = glm::vec3(glm::inverse(parent->getWorldMatrix()) * glm::vec4(worldStep, 0.0f));
        }
        object->setPosition(object->position + worldStep);

        if (object->hasCollision) {
            for (auto& other : shapes) {
                if (object.get() == other.get()) continue;
                if (!other->hasCollision) continue;

                if (object->checkCollision(*other)) {
                    object->setPosition(oldPosition);
                    object->velocity = glm::vec3(0.0f);
                }
            }
        }

        if (object->getWorldPosition().y < -30.0f) {
            object->setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
            object->velocity = glm::vec3(0.0f);
        }
    }
}
