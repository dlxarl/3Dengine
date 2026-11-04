#pragma once
#include <glm/glm.hpp>

// A body of water: its surface height, over a rectangle of min x, min z, max x, max z, or the
// ellipse inside it.
struct WaterArea {
    float level = 0.0f;
    glm::vec4 area = glm::vec4(0.0f);
    bool round = false;
};
