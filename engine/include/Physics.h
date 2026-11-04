#pragma once
#include "Shape.h"
#include <memory>
#include <vector>

class ColliderGrid;
class HeightField;

// The engine's lightweight physics: gravity, velocity integration and box collisions.
namespace Physics {
    // The original response, kept for the demo scenes: an object that touches anything goes
    // back to where it was and stops.
    void step(const std::vector<std::shared_ptr<Shape>>& shapes, float deltaTime);

    // Pushes an object out of what it runs into by the shortest way, so that it slides along
    // walls (turned ones too), lands flush on top of things and comes to rest on the ground.
    // Objects that are not static and move or fall take part; everything with collision is
    // something to collide with. colliders, when built, is where those are looked up;
    // without it they are gathered from shapes for this step.
    void simulate(const std::vector<std::shared_ptr<Shape>>& shapes, float deltaTime, const HeightField* ground = nullptr,
                  ColliderGrid* colliders = nullptr);
}
