#include "CharacterController.h"

#include <algorithm>
#include <cmath>

namespace {
    const float SEPARATION = 0.001f;
    const float SWIM_SPEED_FACTOR = 0.55f;
    const float SWIM_ACCELERATION = 14.0f;
    const float BUOYANCY = 10.0f;
    const float WATER_DRAG = 4.0f;
    const float DIVE_SPEED = 3.2f;
    const float DIVE_RESPONSE = 4.0f;
    const int MAX_SUBSTEPS = 8;

    // The solids near a point, gathered first so that checking one never starts another query.
    std::vector<const SolidBox*> near(const ColliderGrid* colliders, glm::vec3 position, float reach, const Shape& body) {
        std::vector<const SolidBox*> found;
        if (!colliders) return found;
        const glm::vec2 at(position.x, position.z);
        colliders->query(at - glm::vec2(reach), at + glm::vec2(reach), [&](const SolidBox& box) {
            if (box.shape != &body && box.shape->hasCollision) found.push_back(&box);
        });
        return found;
    }

    // Whether a body standing at position, as a cylinder of radius, overlaps any of them.
    bool blocked(const std::vector<const SolidBox*>& solids, glm::vec3 position, float radius, glm::vec3 half) {
        for (const SolidBox* box : solids) {
            if (box->top() > position.y - half.y && box->bottom() < position.y + half.y &&
                box->touches(glm::vec2(position.x, position.z), radius - 0.002f)) {
                return true;
            }
        }
        return false;
    }
}

const WaterArea* CharacterSurroundings::waterAt(glm::vec2 point) const {
    // Where pieces of a river meet, the piece the point is most inside of.
    const WaterArea* best = nullptr;
    float nearest = 0.0f;
    for (const WaterArea& water : waters) {
        if (point.x <= water.area.x || point.x >= water.area.z || point.y <= water.area.y || point.y >= water.area.w) continue;
        const glm::vec2 center(0.5f * (water.area.x + water.area.z), 0.5f * (water.area.y + water.area.w));
        if (water.round) {
            const glm::vec2 half(0.5f * (water.area.z - water.area.x), 0.5f * (water.area.w - water.area.y));
            const glm::vec2 offset = (point - center) / half;
            if (glm::dot(offset, offset) >= 1.0f) continue;
        }
        const float distance = glm::length(point - center);
        if (!best || distance < nearest) {
            best = &water;
            nearest = distance;
        }
    }
    return best;
}

CharacterMove CharacterController::move(Shape& body, CharacterState& state, glm::vec3 wish, float speed, bool jump,
                                        const CharacterSurroundings& world, float deltaTime, float swimUp) const {
    CharacterMove result;
    const glm::vec3 half = 0.5f * body.getWorldScale();
    const float radius = std::max(half.x, half.z);
    glm::vec3 position = body.position;
    glm::vec3& velocity = body.velocity;
    const bool wasGrounded = state.grounded;
    const bool hasGround = world.ground && world.ground->isValid();

    const WaterArea* water = world.waterAt(glm::vec2(position.x, position.z));
    state.swimming = water && position.y - half.y < water->level - swimDepth;
    state.submerged = water && position.y + half.y * 0.8f < water->level;
    if (!state.swimming) state.diving = false;
    if (state.swimming && swimUp < 0.0f) state.diving = true;

    // Steering towards the wished velocity rather than snapping to it.
    const glm::vec2 target = glm::vec2(wish.x, wish.z) * speed * (state.swimming ? SWIM_SPEED_FACTOR : 1.0f);
    const float rate = state.swimming ? SWIM_ACCELERATION : (state.grounded ? acceleration : airAcceleration);
    glm::vec2 horizontal(velocity.x, velocity.z);
    glm::vec2 change = target - horizontal;
    const float most = rate * deltaTime;
    if (glm::length(change) > most) change *= most / glm::length(change);
    horizontal += change;
    velocity.x = horizontal.x;
    velocity.z = horizontal.y;

    if (state.swimming) {
        // Floats with the head above the surface; jumping swims upwards.
        const float floatHeight = water->level - swimDepth - 0.25f + half.y;
        if (state.diving) {
            // Under the surface one goes where one looks, and stays put when not asked to move.
            const float target = glm::clamp(wish.y, -1.0f, 1.0f) * speed * SWIM_SPEED_FACTOR + swimUp * DIVE_SPEED;
            velocity.y += (target - velocity.y) * std::min(1.0f, DIVE_RESPONSE * deltaTime);
            // Reaching the surface ends the dive, and the water holds one there.
            if (position.y >= floatHeight && target >= 0.0f) {
                state.diving = false;
                velocity.y = std::min(velocity.y, 1.0f);
            }
        } else {
            velocity.y += (floatHeight - position.y) * BUOYANCY * deltaTime;
            velocity.y *= std::exp(-WATER_DRAG * deltaTime);
            if (jump) velocity.y = std::max(velocity.y, 3.0f);
        }
    } else {
        // Gravity applies even on the ground, so that contact is found again every frame.
        velocity.y = std::max(velocity.y - gravity * deltaTime, -50.0f);
        if (jump && (state.grounded || state.jumpGrace > 0.0f)) {
            velocity.y = jumpSpeed;
            state.grounded = false;
            state.jumpGrace = 0.0f;
        }
    }

    // Across the ground in steps no longer than half the body, so nothing thin is passed through.
    const float travel = glm::length(glm::vec2(velocity.x, velocity.z)) * deltaTime;
    const int steps = std::clamp((int)std::ceil(travel / (radius * 0.5f)), 1, MAX_SUBSTEPS);
    const float slice = deltaTime / steps;

    for (int s = 0; s < steps; s++) {
        glm::vec2 step(velocity.x * slice, velocity.z * slice);
        if (step == glm::vec2(0.0f)) break;

        if (hasGround && wasGrounded && !state.swimming) {
            // Each way on its own, so that a cliff stops the climb but not the walk along it.
            for (int axis = 0; axis < 2; axis++) {
                glm::vec3 next = position;
                next[axis * 2] += step[axis];
                const float rise = world.ground->heightAt(next.x, next.z) - world.ground->heightAt(position.x, position.z);
                if (rise > maxSlope * std::abs(step[axis]) + 0.002f) {
                    step[axis] = 0.0f;
                    velocity[axis * 2] = 0.0f;
                }
            }
        }
        position.x += step.x;
        position.z += step.y;

        const std::vector<const SolidBox*> solids = near(world.colliders, position, radius + 1.0f, body);
        for (int pass = 0; pass < 4; pass++) {
            bool moved = false;
            for (const SolidBox* box : solids) {
                const float feet = position.y - half.y;
                if (box->top() <= feet + SEPARATION || box->bottom() >= position.y + half.y) continue;
                glm::vec2 push;
                if (!box->pushOut(glm::vec2(position.x, position.z), radius, push)) continue;

                const float climb = box->top() - feet;
                glm::vec3 stepped = position;
                stepped.y = box->top() + half.y + SEPARATION;
                if (wasGrounded && climb <= stepHeight && !blocked(solids, stepped, radius, half)) {
                    position = stepped;
                } else {
                    position.x += push.x;
                    position.z += push.y;
                    // Only the part of the velocity into the wall is lost, so the rest slides.
                    const glm::vec2 normal = glm::normalize(push);
                    const float into = glm::dot(glm::vec2(velocity.x, velocity.z), normal);
                    if (into < 0.0f) {
                        velocity.x -= normal.x * into;
                        velocity.z -= normal.y * into;
                    }
                }
                moved = true;
            }
            if (!moved) break;
        }
    }

    bool grounded = false;
    const float fallSpeed = -velocity.y;
    position.y += velocity.y * deltaTime;

    const std::vector<const SolidBox*> solids = near(world.colliders, position, radius + 1.0f, body);
    for (int i = 0; i < 4; i++) {
        const SolidBox* hit = nullptr;
        for (const SolidBox* box : solids) {
            if (box->top() > position.y - half.y && box->bottom() < position.y + half.y &&
                box->touches(glm::vec2(position.x, position.z), radius - 0.002f)) {
                hit = box;
                break;
            }
        }
        if (!hit) break;
        if (velocity.y <= 0.0f) {
            position.y = hit->top() + half.y + SEPARATION;
            grounded = true;
        } else {
            position.y = hit->bottom() - half.y - SEPARATION;
        }
        velocity.y = 0.0f;
    }

    if (hasGround) {
        position = world.ground->clampInside(position, radius + 0.5f);
        const float floor = world.ground->heightAt(position.x, position.z) + half.y;
        // Within a step of the ground after walking, it keeps its feet down the slope, unless
        // it is standing on something solid instead.
        const bool snap = !grounded && wasGrounded && velocity.y <= 0.0f && position.y < floor + stepHeight;
        if (position.y <= floor || snap) {
            position.y = floor;
            velocity.y = 0.0f;
            grounded = true;
        }
    }

    if (grounded && !wasGrounded) {
        result.landingSpeed = std::max(0.0f, fallSpeed);
    }

    state.grounded = grounded;
    state.jumpGrace = grounded ? jumpGraceSeconds : std::max(0.0f, state.jumpGrace - deltaTime);

    body.setPosition(position);
    return result;
}
