#pragma once
#include "Shape.h"
#include <memory>
#include <vector>

// The engine's lightweight physics: gravity, velocity integration and AABB overlap response.
namespace Physics {
    void step(const std::vector<std::shared_ptr<Shape>>& shapes, float deltaTime);
}
