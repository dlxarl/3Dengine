#pragma once
#include <glm/glm.hpp>

// The six planes of a view volume, for skipping what cannot be seen.
class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);

    bool containsSphere(glm::vec3 center, float radius) const;

private:
    glm::vec4 planes[6];
};
