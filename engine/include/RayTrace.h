#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <vector>

// The scene's still geometry as a bounding volume hierarchy of triangles, packed so that a
// fragment shader can walk it: it is uploaded as two float textures and traced there, one ray
// per pixel per light, to find shadows.
//
//   nodes      two vec4 per node. The first: the box's minimum, and for a leaf the first
//              triangle, for an inner node the index of its right child (the left child is the
//              next node). The second: the box's maximum, and the triangle count, 0 for an inner node.
//   triangles  three vec4 per triangle: a corner and the two edges from it.
struct RayTraceData {
    std::vector<glm::vec4> nodes;
    std::vector<glm::vec4> triangles;

    int nodeCount() const { return (int)nodes.size() / 2; }
    int triangleCount() const { return (int)triangles.size() / 3; }
    bool empty() const { return triangles.empty(); }
};

// points holds three corners per triangle, in world space.
std::shared_ptr<RayTraceData> buildRayTraceData(const std::vector<glm::vec3>& points);

// Whether anything lies on the ray between its start and maxDistance. The same walk the shader does.
bool rayHitsAny(const RayTraceData& data, glm::vec3 origin, glm::vec3 direction, float maxDistance);
