#pragma once
#include "Shape.h"

#include <algorithm>
#include <cmath>
#include <vector>

// A solid's box in the world. It may be turned about the vertical axis, as a building set at
// an angle is; anything tilted further is held by the upright box around it.
struct SolidBox {
    Shape* shape = nullptr;
    glm::vec3 center = glm::vec3(0.0f);
    glm::vec3 half = glm::vec3(0.0f);      // along its own axes
    glm::vec2 axisX = glm::vec2(1.0f, 0.0f);  // its own x and z axes, in the ground plane
    glm::vec2 axisZ = glm::vec2(0.0f, 1.0f);

    static SolidBox of(Shape& shape);

    float bottom() const { return center.y - half.y; }
    float top() const { return center.y + half.y; }

    // Half the size of the upright, unturned box around it, in the ground plane.
    glm::vec2 reach() const;

    // How a circle in the ground plane gets out of it, when they overlap. The push is in
    // world space and leaves the circle just touching.
    bool pushOut(glm::vec2 point, float radius, glm::vec2& push) const;

    // Whether a circle overlaps its footprint, ignoring height.
    bool touches(glm::vec2 point, float radius) const;

    bool contains(glm::vec3 point) const;

    // Where a ray first enters the box grown by margin; false when it misses or starts inside.
    bool rayEnter(glm::vec3 from, glm::vec3 direction, float margin, float& distance) const;
};

// The solids of a world, sorted into a grid of cells so that a question about one place only
// looks at what is near it. Solids that move are kept apart and read again on refresh().
class ColliderGrid {
public:
    void build(const std::vector<Shape*>& solids, float cellSize = 8.0f);
    void clear();
    bool isBuilt() const { return built; }

    // Reads the boxes of the moving solids again, after they moved.
    void refresh();

    // Calls visit(const SolidBox&) once for each box whose footprint meets the rectangle.
    template <typename Visit>
    void query(glm::vec2 low, glm::vec2 high, Visit&& visit) const {
        if (!built) return;
        stamp++;
        if (stamp == 0) {
            std::fill(stamps.begin(), stamps.end(), 0u);
            stamp = 1;
        }
        const int x0 = std::max(0, (int)std::floor((low.x - origin.x) / cell));
        const int z0 = std::max(0, (int)std::floor((low.y - origin.y) / cell));
        const int x1 = std::min(columns - 1, (int)std::floor((high.x - origin.x) / cell));
        const int z1 = std::min(rows - 1, (int)std::floor((high.y - origin.y) / cell));
        for (int z = z0; z <= z1; z++) {
            for (int x = x0; x <= x1; x++) {
                for (int index : cells[(size_t)z * columns + x]) {
                    if (stamps[index] == stamp) continue;
                    stamps[index] = stamp;
                    visit(boxes[index]);
                }
            }
        }
        for (int index : moving) {
            const SolidBox& box = boxes[index];
            const glm::vec2 r = box.reach();
            if (box.center.x + r.x < low.x || box.center.x - r.x > high.x ||
                box.center.z + r.y < low.y || box.center.z - r.y > high.y) continue;
            visit(box);
        }
    }

    // Moves a standing body out of every solid it overlaps, so that it slides along walls.
    // Solids no higher than climbable above its feet are left to be walked over. Returns
    // whether anything was in the way, and in which direction it pushed back, on average.
    bool resolve(glm::vec3& feet, float radius, float height, float climbable, const Shape* ignore,
                 glm::vec2* pushed = nullptr) const;

    // A solid the point is inside, as an arrow on its way; null when none.
    const SolidBox* firstContaining(glm::vec3 point, const Shape* ignore) const;

private:
    std::vector<SolidBox> boxes;
    std::vector<int> moving;
    std::vector<std::vector<int>> cells;
    mutable std::vector<unsigned> stamps;
    mutable unsigned stamp = 0;
    glm::vec2 origin = glm::vec2(0.0f);
    int columns = 0;
    int rows = 0;
    float cell = 8.0f;
    bool built = false;
};
