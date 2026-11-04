#pragma once
#include <glm/glm.hpp>

// How the world looks to a camera that is under the water.
struct UnderwaterView {
    bool active = false;
    glm::vec3 tint = glm::vec3(0.04f, 0.22f, 0.28f);   // the water's color at a distance
    float density = 1.0f;                               // 1 is the usual murk
    float depth = 0.0f;                                 // below the surface
};
