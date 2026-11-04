#include "Physics.h"
#include "Colliders.h"
#include "HeightField.h"

#include <cmath>

namespace {
    const float GRAVITY = -19.6f;
    const float SEPARATION = 0.001f;
    const float GROUND_FRICTION = 6.0f;   // per second, while resting on something
    const float RESTITUTION = 0.15f;      // how much of a landing bounces back
    const float SETTLE_SPEED = 0.05f;

    // Moves the object to world position, through its parent if it has one.
    void placeAt(Shape& object, glm::vec3 world) {
        if (SceneObject* parent = object.getParent()) {
            object.setPosition(glm::vec3(glm::inverse(parent->getWorldMatrix()) * glm::vec4(world, 1.0f)));
        } else {
            object.setPosition(world);
        }
    }

    bool alignedWithAxes(const SolidBox& box) {
        return std::abs(box.axisX.x) > 0.9999f || std::abs(box.axisX.y) > 0.9999f;
    }

    // How a body's upright box gets out of a solid in the ground plane; false when apart.
    // Boxes square to the world are pushed out exactly; against a turned one, the body is a circle.
    bool pushOut(const SolidBox& solid, glm::vec2 at, glm::vec2 half, glm::vec2& push) {
        if (!alignedWithAxes(solid)) return solid.pushOut(at, std::max(half.x, half.y), push);

        const glm::vec2 offset = at - glm::vec2(solid.center.x, solid.center.z);
        const glm::vec2 overlap = half + solid.reach() - glm::abs(offset);
        if (overlap.x <= 0.0f || overlap.y <= 0.0f) return false;
        if (overlap.x < overlap.y) {
            push = glm::vec2((offset.x < 0.0f ? -1.0f : 1.0f) * (overlap.x + SEPARATION), 0.0f);
        } else {
            push = glm::vec2(0.0f, (offset.y < 0.0f ? -1.0f : 1.0f) * (overlap.y + SEPARATION));
        }
        return true;
    }

    bool under(const SolidBox& solid, glm::vec2 at, glm::vec2 half) {
        if (!alignedWithAxes(solid)) return solid.touches(at, std::max(half.x, half.y) - 0.002f);
        const glm::vec2 offset = glm::abs(at - glm::vec2(solid.center.x, solid.center.z));
        const glm::vec2 limit = half + solid.reach();
        return offset.x < limit.x && offset.y < limit.y;
    }
}

void Physics::simulate(const std::vector<std::shared_ptr<Shape>>& shapes, float deltaTime, const HeightField* ground,
                       ColliderGrid* colliders) {
    ColliderGrid gathered;
    if (!colliders || !colliders->isBuilt()) {
        std::vector<Shape*> solids;
        bool anyMoving = false;
        for (const auto& shape : shapes) {
            if (!shape->hasCollision) continue;
            solids.push_back(shape.get());
            anyMoving = anyMoving || (!shape->isStatic && shape->getType() != ObjectType::Player);
        }
        if (!anyMoving) return;
        gathered.build(solids);
        colliders = &gathered;
    }
    colliders->refresh();

    for (const auto& body : shapes) {
        if (body->isStatic || !body->hasCollision || body->getType() == ObjectType::Player) continue;
        if (!body->useGravity && glm::length(body->velocity) < SETTLE_SPEED) continue;

        const glm::vec3 half = 0.5f * body->getWorldScale();
        const glm::vec2 flatHalf(half.x, half.z);
        glm::vec3 position = body->getWorldPosition();
        glm::vec3& velocity = body->velocity;
        bool resting = false;

        if (body->useGravity) velocity.y += GRAVITY * deltaTime;

        std::vector<const SolidBox*> nearby;
        auto gather = [&]() {
            nearby.clear();
            const glm::vec2 at(position.x, position.z);
            const glm::vec2 reach = flatHalf + glm::vec2(1.0f);
            colliders->query(at - reach, at + reach, [&](const SolidBox& solid) {
                if (solid.shape != body.get() && solid.shape->hasCollision) nearby.push_back(&solid);
            });
        };

        // Across first, then down, each resolved against what it then touches.
        position.x += velocity.x * deltaTime;
        position.z += velocity.z * deltaTime;
        gather();
        for (int pass = 0; pass < 3; pass++) {
            bool moved = false;
            for (const SolidBox* solid : nearby) {
                if (solid->top() <= position.y - half.y + SEPARATION || solid->bottom() >= position.y + half.y) continue;
                glm::vec2 push;
                if (!pushOut(*solid, glm::vec2(position.x, position.z), flatHalf, push)) continue;
                position.x += push.x;
                position.z += push.y;
                const glm::vec2 normal = glm::normalize(push);
                const float into = glm::dot(glm::vec2(velocity.x, velocity.z), normal);
                if (into < 0.0f) {
                    velocity.x -= normal.x * into;
                    velocity.z -= normal.y * into;
                }
                moved = true;
            }
            if (!moved) break;
        }

        const float fall = velocity.y * deltaTime;
        position.y += fall;
        if (fall != 0.0f) {
            gather();
            for (const SolidBox* solid : nearby) {
                if (solid->top() <= position.y - half.y || solid->bottom() >= position.y + half.y) continue;
                if (!under(*solid, glm::vec2(position.x, position.z), flatHalf)) continue;
                if (fall < 0.0f) {
                    position.y = solid->top() + half.y + SEPARATION;
                    resting = true;
                    velocity.y = -velocity.y * RESTITUTION;
                    if (std::abs(velocity.y) < 1.0f) velocity.y = 0.0f;
                } else {
                    position.y = solid->bottom() - half.y - SEPARATION;
                    velocity.y = 0.0f;
                }
            }
        }

        if (ground && ground->isValid()) {
            position = ground->clampInside(position, half.x);
            const float floor = ground->heightAt(position.x, position.z) + half.y;
            if (position.y <= floor) {
                position.y = floor;
                velocity.y = velocity.y < -1.0f ? -velocity.y * RESTITUTION : 0.0f;
                resting = true;
            }
        }

        if (resting) {
            const float keep = std::exp(-GROUND_FRICTION * deltaTime);
            velocity.x *= keep;
            velocity.z *= keep;
            if (glm::length(glm::vec2(velocity.x, velocity.z)) < SETTLE_SPEED) {
                velocity.x = 0.0f;
                velocity.z = 0.0f;
            }
        }

        if (position.y < -30.0f) {
            position = glm::vec3(0.0f, 10.0f, 0.0f);
            velocity = glm::vec3(0.0f);
        }

        placeAt(*body, position);
    }
    colliders->refresh();
}

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
