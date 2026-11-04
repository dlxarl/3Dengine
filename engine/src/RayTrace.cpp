#include "RayTrace.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
    struct Box {
        glm::vec3 low = glm::vec3(1e30f);
        glm::vec3 high = glm::vec3(-1e30f);
        void grow(glm::vec3 p) {
            low = glm::min(low, p);
            high = glm::max(high, p);
        }
        void grow(const Box& other) {
            low = glm::min(low, other.low);
            high = glm::max(high, other.high);
        }
    };

    struct Builder {
        const std::vector<glm::vec3>& points;
        std::vector<int> order;                // triangle indices, rearranged into leaves
        std::vector<Box> boxes;
        std::vector<glm::vec3> centers;
        RayTraceData& out;

        Builder(const std::vector<glm::vec3>& p, RayTraceData& result) : points(p), out(result) {}

        // Returns the index of the node it made.
        int build(int first, int count) {
            Box box;
            Box centerBox;
            for (int i = first; i < first + count; i++) {
                box.grow(boxes[(size_t)order[(size_t)i]]);
                centerBox.grow(centers[(size_t)order[(size_t)i]]);
            }

            const int index = (int)out.nodes.size() / 2;
            out.nodes.push_back(glm::vec4(box.low, 0.0f));
            out.nodes.push_back(glm::vec4(box.high, 0.0f));

            const glm::vec3 extent = centerBox.high - centerBox.low;
            const int axis = extent.x > extent.y && extent.x > extent.z ? 0 : (extent.y > extent.z ? 1 : 2);
            if (count <= 4 || extent[axis] <= 1e-6f) {
                out.nodes[(size_t)index * 2].w = (float)first;
                out.nodes[(size_t)index * 2 + 1].w = (float)count;
                return index;
            }

            // Split at the middle triangle along the longest axis of the centers.
            const int middle = first + count / 2;
            std::nth_element(order.begin() + first, order.begin() + middle, order.begin() + first + count,
                             [&](int a, int b) { return centers[(size_t)a][axis] < centers[(size_t)b][axis]; });
            build(first, middle - first);
            const int right = build(middle, first + count - middle);
            out.nodes[(size_t)index * 2].w = (float)right;
            return index;
        }
    };

    // Möller-Trumbore, for a triangle stored as a corner and two edges.
    bool hitsTriangle(glm::vec3 origin, glm::vec3 direction, glm::vec3 corner, glm::vec3 e1, glm::vec3 e2, float maxDistance) {
        const glm::vec3 p = glm::cross(direction, e2);
        const float det = glm::dot(e1, p);
        if (std::abs(det) < 1e-9f) return false;
        const float inverse = 1.0f / det;
        const glm::vec3 s = origin - corner;
        const float u = glm::dot(s, p) * inverse;
        if (u < 0.0f || u > 1.0f) return false;
        const glm::vec3 q = glm::cross(s, e1);
        const float v = glm::dot(direction, q) * inverse;
        if (v < 0.0f || u + v > 1.0f) return false;
        const float t = glm::dot(e2, q) * inverse;
        return t > 1e-4f && t < maxDistance;
    }
}

std::shared_ptr<RayTraceData> buildRayTraceData(const std::vector<glm::vec3>& points) {
    auto data = std::make_shared<RayTraceData>();
    const int count = (int)points.size() / 3;
    if (count == 0) return data;

    Builder builder(points, *data);
    builder.boxes.resize((size_t)count);
    builder.centers.resize((size_t)count);
    for (int t = 0; t < count; t++) {
        Box box;
        for (int c = 0; c < 3; c++) box.grow(points[(size_t)t * 3 + c]);
        builder.boxes[(size_t)t] = box;
        builder.centers[(size_t)t] = (box.low + box.high) * 0.5f;
    }
    builder.order.resize((size_t)count);
    std::iota(builder.order.begin(), builder.order.end(), 0);
    data->nodes.reserve((size_t)count);
    builder.build(0, count);

    data->triangles.reserve((size_t)count * 3);
    for (int index : builder.order) {
        const glm::vec3 a = points[(size_t)index * 3], b = points[(size_t)index * 3 + 1], c = points[(size_t)index * 3 + 2];
        data->triangles.push_back(glm::vec4(a, 0.0f));
        data->triangles.push_back(glm::vec4(b - a, 0.0f));
        data->triangles.push_back(glm::vec4(c - a, 0.0f));
    }
    return data;
}

bool rayHitsAny(const RayTraceData& data, glm::vec3 origin, glm::vec3 direction, float maxDistance) {
    if (data.empty()) return false;
    const glm::vec3 safe(std::abs(direction.x) < 1e-8f ? 1e-8f : direction.x, std::abs(direction.y) < 1e-8f ? 1e-8f : direction.y,
                         std::abs(direction.z) < 1e-8f ? 1e-8f : direction.z);
    const glm::vec3 inverse = 1.0f / safe;

    int stack[64];
    int top = 0;
    int node = 0;
    while (true) {
        const glm::vec4 a = data.nodes[(size_t)node * 2], b = data.nodes[(size_t)node * 2 + 1];
        const glm::vec3 t0 = (glm::vec3(a) - origin) * inverse, t1 = (glm::vec3(b) - origin) * inverse;
        const glm::vec3 near = glm::min(t0, t1), far = glm::max(t0, t1);
        const float enter = std::max(std::max(near.x, near.y), near.z);
        const float leave = std::min(std::min(far.x, far.y), far.z);
        if (leave >= std::max(enter, 0.0f) && enter < maxDistance) {
            if (b.w > 0.5f) {
                const int first = (int)a.w, count = (int)b.w;
                for (int i = 0; i < count; i++) {
                    const size_t base = (size_t)(first + i) * 3;
                    if (hitsTriangle(origin, direction, glm::vec3(data.triangles[base]), glm::vec3(data.triangles[base + 1]),
                                     glm::vec3(data.triangles[base + 2]), maxDistance)) {
                        return true;
                    }
                }
            } else {
                if (top < 64) stack[top++] = (int)a.w;
                node += 1;
                continue;
            }
        }
        if (top == 0) break;
        node = stack[--top];
    }
    return false;
}
