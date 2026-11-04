#include "Frustum.h"

Frustum::Frustum(const glm::mat4& m) {
    // Rows of the matrix; glm stores columns.
    const glm::vec4 row0(m[0][0], m[1][0], m[2][0], m[3][0]);
    const glm::vec4 row1(m[0][1], m[1][1], m[2][1], m[3][1]);
    const glm::vec4 row2(m[0][2], m[1][2], m[2][2], m[3][2]);
    const glm::vec4 row3(m[0][3], m[1][3], m[2][3], m[3][3]);

    planes[0] = row3 + row0;
    planes[1] = row3 - row0;
    planes[2] = row3 + row1;
    planes[3] = row3 - row1;
    planes[4] = row3 + row2;
    planes[5] = row3 - row2;

    for (glm::vec4& plane : planes) {
        plane /= glm::length(glm::vec3(plane));
    }
}

bool Frustum::containsSphere(glm::vec3 center, float radius) const {
    for (const glm::vec4& plane : planes) {
        if (glm::dot(glm::vec3(plane), center) + plane.w < -radius) return false;
    }
    return true;
}
