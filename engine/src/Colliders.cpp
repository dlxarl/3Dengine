#include "Colliders.h"

#include <limits>

namespace {
    const float SEPARATION = 0.001f;
    const int MAX_CELLS_PER_SIDE = 512;
}

SolidBox SolidBox::of(Shape& shape) {
    SolidBox box;
    box.shape = &shape;

    // Primitives are unit cubes around their position, so the world matrix's axes are the box.
    const glm::mat4 world = shape.getWorldMatrix();
    const glm::vec3 x(world[0]);
    const glm::vec3 y(world[1]);
    const glm::vec3 z(world[2]);
    box.center = glm::vec3(world[3]);

    const float lengthX = glm::length(x);
    const float lengthY = glm::length(y);
    const float lengthZ = glm::length(z);
    const float tilt = 1e-3f;
    const bool upright = std::abs(x.y) <= tilt * lengthX && std::abs(z.y) <= tilt * lengthZ &&
                         std::abs(y.x) <= tilt * lengthY && std::abs(y.z) <= tilt * lengthY;

    if (upright && lengthX > 0.0f && lengthZ > 0.0f) {
        box.half = 0.5f * glm::vec3(lengthX, lengthY, lengthZ);
        box.axisX = glm::normalize(glm::vec2(x.x, x.z));
        box.axisZ = glm::normalize(glm::vec2(z.x, z.z));
    } else {
        box.half = 0.5f * (glm::abs(x) + glm::abs(y) + glm::abs(z));
    }
    return box;
}

glm::vec2 SolidBox::reach() const {
    return glm::vec2(std::abs(axisX.x) * half.x + std::abs(axisZ.x) * half.z,
                     std::abs(axisX.y) * half.x + std::abs(axisZ.y) * half.z);
}

bool SolidBox::pushOut(glm::vec2 point, float radius, glm::vec2& push) const {
    const glm::vec2 offset = point - glm::vec2(center.x, center.z);
    const float localX = glm::dot(offset, axisX);
    const float localZ = glm::dot(offset, axisZ);
    const float nearX = std::clamp(localX, -half.x, half.x);
    const float nearZ = std::clamp(localZ, -half.z, half.z);
    const glm::vec2 gap(localX - nearX, localZ - nearZ);
    const float distance = glm::length(gap);

    glm::vec2 local;
    if (distance > 0.0f) {
        if (distance >= radius) return false;
        local = gap / distance * (radius - distance + SEPARATION);
    } else {
        // The centre is inside: out through the nearest side.
        const float outX = half.x - std::abs(localX);
        const float outZ = half.z - std::abs(localZ);
        if (outX < outZ) {
            local = glm::vec2((localX < 0.0f ? -1.0f : 1.0f) * (outX + radius + SEPARATION), 0.0f);
        } else {
            local = glm::vec2(0.0f, (localZ < 0.0f ? -1.0f : 1.0f) * (outZ + radius + SEPARATION));
        }
    }
    push = axisX * local.x + axisZ * local.y;
    return true;
}

bool SolidBox::touches(glm::vec2 point, float radius) const {
    const glm::vec2 offset = point - glm::vec2(center.x, center.z);
    const float localX = glm::dot(offset, axisX);
    const float localZ = glm::dot(offset, axisZ);
    const glm::vec2 gap(localX - std::clamp(localX, -half.x, half.x), localZ - std::clamp(localZ, -half.z, half.z));
    return glm::dot(gap, gap) < radius * radius;
}

bool SolidBox::contains(glm::vec3 point) const {
    if (point.y < bottom() || point.y > top()) return false;
    const glm::vec2 offset = glm::vec2(point.x, point.z) - glm::vec2(center.x, center.z);
    return std::abs(glm::dot(offset, axisX)) <= half.x && std::abs(glm::dot(offset, axisZ)) <= half.z;
}

bool SolidBox::rayEnter(glm::vec3 from, glm::vec3 direction, float margin, float& distance) const {
    const glm::vec3 offset = from - center;
    const glm::vec3 start(glm::dot(glm::vec2(offset.x, offset.z), axisX), offset.y,
                          glm::dot(glm::vec2(offset.x, offset.z), axisZ));
    const glm::vec3 heading(glm::dot(glm::vec2(direction.x, direction.z), axisX), direction.y,
                            glm::dot(glm::vec2(direction.x, direction.z), axisZ));
    const glm::vec3 size = half + glm::vec3(margin);

    float enter = 0.0f;
    float leave = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; axis++) {
        if (std::abs(heading[axis]) < 1e-6f) {
            if (std::abs(start[axis]) > size[axis]) return false;
            continue;
        }
        float t0 = (-size[axis] - start[axis]) / heading[axis];
        float t1 = (size[axis] - start[axis]) / heading[axis];
        if (t0 > t1) std::swap(t0, t1);
        enter = std::max(enter, t0);
        leave = std::min(leave, t1);
        if (enter > leave) return false;
    }
    if (enter <= 0.0f) return false;
    distance = enter;
    return true;
}

void ColliderGrid::clear() {
    boxes.clear();
    moving.clear();
    cells.clear();
    stamps.clear();
    built = false;
}

void ColliderGrid::build(const std::vector<Shape*>& solids, float cellSize) {
    clear();

    glm::vec2 low(std::numeric_limits<float>::max());
    glm::vec2 high(std::numeric_limits<float>::lowest());
    for (Shape* solid : solids) {
        boxes.push_back(SolidBox::of(*solid));
        if (!solid->isStatic) {
            moving.push_back((int)boxes.size() - 1);
            continue;
        }
        const SolidBox& box = boxes.back();
        const glm::vec2 r = box.reach();
        low = glm::min(low, glm::vec2(box.center.x, box.center.z) - r);
        high = glm::max(high, glm::vec2(box.center.x, box.center.z) + r);
    }
    stamps.assign(boxes.size(), 0u);
    built = true;

    if (low.x > high.x) {
        origin = glm::vec2(0.0f);
        columns = rows = 1;
        cell = cellSize;
        cells.assign(1, {});
        return;
    }

    cell = std::max({cellSize, (high.x - low.x) / MAX_CELLS_PER_SIDE, (high.y - low.y) / MAX_CELLS_PER_SIDE});
    origin = low;
    columns = (int)std::floor((high.x - low.x) / cell) + 1;
    rows = (int)std::floor((high.y - low.y) / cell) + 1;
    cells.assign((size_t)columns * rows, {});

    for (int index = 0; index < (int)boxes.size(); index++) {
        const SolidBox& box = boxes[index];
        if (!box.shape->isStatic) continue;
        const glm::vec2 r = box.reach();
        const int x0 = (int)std::floor((box.center.x - r.x - origin.x) / cell);
        const int x1 = std::min(columns - 1, (int)std::floor((box.center.x + r.x - origin.x) / cell));
        const int z0 = (int)std::floor((box.center.z - r.y - origin.y) / cell);
        const int z1 = std::min(rows - 1, (int)std::floor((box.center.z + r.y - origin.y) / cell));
        for (int z = std::max(0, z0); z <= z1; z++) {
            for (int x = std::max(0, x0); x <= x1; x++) cells[(size_t)z * columns + x].push_back(index);
        }
    }
}

void ColliderGrid::refresh() {
    for (int index : moving) {
        boxes[index] = SolidBox::of(*boxes[index].shape);
    }
}

bool ColliderGrid::resolve(glm::vec3& feet, float radius, float height, float climbable, const Shape* ignore,
                           glm::vec2* pushed) const {
    bool blocked = false;
    glm::vec2 total(0.0f);

    // A few passes settle a corner, where getting out of one wall pushes into the other.
    for (int pass = 0; pass < 4; pass++) {
        bool moved = false;
        const glm::vec2 point(feet.x, feet.z);
        query(point - glm::vec2(radius), point + glm::vec2(radius), [&](const SolidBox& box) {
            if (box.shape == ignore || !box.shape->hasCollision) return;
            if (box.top() <= feet.y + climbable || box.bottom() >= feet.y + height) return;
            glm::vec2 push;
            if (box.pushOut(glm::vec2(feet.x, feet.z), radius, push)) {
                feet.x += push.x;
                feet.z += push.y;
                total += push;
                moved = true;
            }
        });
        if (!moved) break;
        blocked = true;
    }
    if (pushed) *pushed = total;
    return blocked;
}

const SolidBox* ColliderGrid::firstContaining(glm::vec3 point, const Shape* ignore) const {
    const SolidBox* found = nullptr;
    const glm::vec2 at(point.x, point.z);
    query(at, at, [&](const SolidBox& box) {
        if (!found && box.shape != ignore && box.shape->hasCollision && box.contains(point)) found = &box;
    });
    return found;
}
