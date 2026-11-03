#include "ObjLoader.h"

#include <glm/glm.hpp>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
    struct FaceVertex {
        int position = -1;
        int uv = -1;
        int normal = -1;
    };

    // OBJ indices are 1-based, and negative ones count back from the end of the list.
    int resolveIndex(long index, size_t count) {
        if (index > 0) return (int)index - 1;
        if (index < 0) return (int)count + (int)index;
        return -1;
    }

    bool parseFaceVertex(const char*& cursor, size_t positions, size_t uvs, size_t normals, FaceVertex& out) {
        char* end = nullptr;
        long index = std::strtol(cursor, &end, 10);
        if (end == cursor) return false;
        out.position = resolveIndex(index, positions);
        cursor = end;

        if (*cursor == '/') {
            cursor++;
            if (*cursor != '/') {
                index = std::strtol(cursor, &end, 10);
                if (end != cursor) {
                    out.uv = resolveIndex(index, uvs);
                    cursor = end;
                }
            }
            if (*cursor == '/') {
                cursor++;
                index = std::strtol(cursor, &end, 10);
                if (end != cursor) {
                    out.normal = resolveIndex(index, normals);
                    cursor = end;
                }
            }
        }
        return true;
    }

    int parseFloats(const char* text, float* values, int maxCount) {
        int count = 0;
        char* end = nullptr;
        while (count < maxCount) {
            float value = std::strtof(text, &end);
            if (end == text) break;
            values[count++] = value;
            text = end;
        }
        return count;
    }
}

bool ObjLoader::load(const std::filesystem::path& path, MeshData& mesh, std::string* error) {
    std::ifstream file(path);
    if (!file.is_open()) {
        if (error) *error = "Cannot open " + path.string();
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> uvs;
    std::vector<FaceVertex> face;
    std::vector<float> vertices;

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        lineNumber++;
        const char* text = line.c_str();
        while (*text == ' ' || *text == '\t') text++;

        if (std::strncmp(text, "v ", 2) == 0) {
            float v[3] = {0.0f, 0.0f, 0.0f};
            parseFloats(text + 2, v, 3);
            positions.emplace_back(v[0], v[1], v[2]);
        } else if (std::strncmp(text, "vt ", 3) == 0) {
            float v[2] = {0.0f, 0.0f};
            parseFloats(text + 3, v, 2);
            uvs.emplace_back(v[0], v[1]);
        } else if (std::strncmp(text, "vn ", 3) == 0) {
            float v[3] = {0.0f, 0.0f, 0.0f};
            parseFloats(text + 3, v, 3);
            normals.emplace_back(v[0], v[1], v[2]);
        } else if (std::strncmp(text, "f ", 2) == 0) {
            face.clear();
            const char* cursor = text + 2;
            while (*cursor) {
                while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r') cursor++;
                if (!*cursor) break;

                FaceVertex vertex;
                if (!parseFaceVertex(cursor, positions.size(), uvs.size(), normals.size(), vertex)) {
                    if (error) *error = "Malformed face at line " + std::to_string(lineNumber);
                    return false;
                }
                if (vertex.position < 0 || vertex.position >= (int)positions.size()) {
                    if (error) *error = "Vertex index out of range at line " + std::to_string(lineNumber);
                    return false;
                }
                face.push_back(vertex);
            }

            // Fan-triangulate polygons.
            for (size_t i = 2; i < face.size(); i++) {
                const FaceVertex corners[3] = {face[0], face[i - 1], face[i]};
                const glm::vec3 a = positions[corners[0].position];
                const glm::vec3 b = positions[corners[1].position];
                const glm::vec3 c = positions[corners[2].position];

                glm::vec3 flatNormal = glm::cross(b - a, c - a);
                const float length = glm::length(flatNormal);
                flatNormal = length > 0.0f ? flatNormal / length : glm::vec3(0.0f, 1.0f, 0.0f);

                for (const FaceVertex& corner : corners) {
                    const glm::vec3 p = positions[corner.position];
                    const bool hasNormal = corner.normal >= 0 && corner.normal < (int)normals.size();
                    const bool hasUv = corner.uv >= 0 && corner.uv < (int)uvs.size();
                    const glm::vec3 n = hasNormal ? normals[corner.normal] : flatNormal;
                    const glm::vec2 uv = hasUv ? uvs[corner.uv] : glm::vec2(0.0f);
                    vertices.insert(vertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
                }
            }
        }
    }

    if (vertices.empty()) {
        if (error) *error = "No faces found in " + path.string();
        return false;
    }

    mesh.vertices = std::move(vertices);
    return true;
}
