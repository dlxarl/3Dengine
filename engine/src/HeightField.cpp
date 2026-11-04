#include "HeightField.h"

#include <algorithm>
#include <cmath>
#include <limits>

void HeightField::build(const std::vector<glm::vec3>& triangles, const glm::mat4& transform, float cellSize) {
    heights.clear();
    if (triangles.size() < 3) return;

    std::vector<glm::vec3> world;
    world.reserve(triangles.size());

    glm::vec2 low(std::numeric_limits<float>::max());
    glm::vec2 high(std::numeric_limits<float>::lowest());
    for (const glm::vec3& vertex : triangles) {
        const glm::vec3 point(transform * glm::vec4(vertex, 1.0f));
        world.push_back(point);
        low = glm::min(low, glm::vec2(point.x, point.z));
        high = glm::max(high, glm::vec2(point.x, point.z));
    }

    cell = cellSize;
    origin = low;
    columns = (int)std::ceil((high.x - low.x) / cell) + 1;
    rows = (int)std::ceil((high.y - low.y) / cell) + 1;

    const float missing = std::numeric_limits<float>::lowest();
    heights.assign((size_t)columns * rows, missing);

    for (size_t i = 0; i + 2 < world.size(); i += 3) {
        const glm::vec3& a = world[i];
        const glm::vec3& b = world[i + 1];
        const glm::vec3& c = world[i + 2];

        const float area = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
        if (std::abs(area) < 1e-6f) continue;

        const int minColumn = std::max(0, (int)std::floor((std::min({a.x, b.x, c.x}) - origin.x) / cell));
        const int maxColumn = std::min(columns - 1, (int)std::ceil((std::max({a.x, b.x, c.x}) - origin.x) / cell));
        const int minRow = std::max(0, (int)std::floor((std::min({a.z, b.z, c.z}) - origin.y) / cell));
        const int maxRow = std::min(rows - 1, (int)std::ceil((std::max({a.z, b.z, c.z}) - origin.y) / cell));

        for (int row = minRow; row <= maxRow; row++) {
            for (int column = minColumn; column <= maxColumn; column++) {
                const float x = origin.x + column * cell;
                const float z = origin.y + row * cell;

                const float u = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / area;
                const float v = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / area;
                const float w = 1.0f - u - v;

                const float tolerance = -0.001f;
                if (u < tolerance || v < tolerance || w < tolerance) continue;

                float& height = heights[(size_t)row * columns + column];
                height = std::max(height, u * a.y + v * b.y + w * c.y);
            }
        }
    }

    for (float& height : heights) {
        if (height == missing) height = 0.0f;
    }
}

float HeightField::sample(int column, int row) const {
    column = std::clamp(column, 0, columns - 1);
    row = std::clamp(row, 0, rows - 1);
    return heights[(size_t)row * columns + column];
}

float HeightField::heightAt(float x, float z) const {
    if (heights.empty()) return std::numeric_limits<float>::lowest();

    const float gridX = (x - origin.x) / cell;
    const float gridZ = (z - origin.y) / cell;
    const int column = (int)std::floor(gridX);
    const int row = (int)std::floor(gridZ);
    const float fx = gridX - column;
    const float fz = gridZ - row;

    const float near0 = sample(column, row) * (1.0f - fx) + sample(column + 1, row) * fx;
    const float near1 = sample(column, row + 1) * (1.0f - fx) + sample(column + 1, row + 1) * fx;
    return near0 * (1.0f - fz) + near1 * fz;
}

glm::vec3 HeightField::clampInside(glm::vec3 position, float margin) const {
    if (heights.empty()) return position;
    position.x = std::clamp(position.x, origin.x + margin, origin.x + (columns - 1) * cell - margin);
    position.z = std::clamp(position.z, origin.y + margin, origin.y + (rows - 1) * cell - margin);
    return position;
}
