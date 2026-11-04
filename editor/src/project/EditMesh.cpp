#include "EditMesh.h"
#include "ObjLoader.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

namespace {
    EditFace makeFace(std::initializer_list<int> corners, std::initializer_list<glm::vec2> uvs) {
        EditFace face;
        face.corners = corners;
        face.uvs = uvs;
        return face;
    }

    const glm::vec2 QUAD_UVS[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    bool rayTriangle(glm::vec3 origin, glm::vec3 direction, glm::vec3 a, glm::vec3 b, glm::vec3 c, float& t) {
        const glm::vec3 e1 = b - a, e2 = c - a;
        const glm::vec3 p = glm::cross(direction, e2);
        const float det = glm::dot(e1, p);
        if (std::abs(det) < 1e-9f) return false;
        const float inv = 1.0f / det;
        const glm::vec3 s = origin - a;
        const float u = glm::dot(s, p) * inv;
        if (u < 0.0f || u > 1.0f) return false;
        const glm::vec3 q = glm::cross(s, e1);
        const float v = glm::dot(direction, q) * inv;
        if (v < 0.0f || u + v > 1.0f) return false;
        t = glm::dot(e2, q) * inv;
        return t > 0.0f;
    }
}

EditMesh EditMesh::cube(float size) {
    EditMesh mesh;
    const float h = size * 0.5f;
    mesh.positions = {{-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {-h, -h, -h}, {h, -h, -h}, {h, h, -h}, {-h, h, -h}};
    const int sides[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
    for (const auto& side : sides) {
        mesh.faces.push_back(makeFace({side[0], side[1], side[2], side[3]}, {QUAD_UVS[0], QUAD_UVS[1], QUAD_UVS[2], QUAD_UVS[3]}));
    }
    return mesh;
}

EditMesh EditMesh::plane(float size, int divisions) {
    EditMesh mesh;
    divisions = std::clamp(divisions, 1, 64);
    for (int z = 0; z <= divisions; z++) {
        for (int x = 0; x <= divisions; x++) {
            mesh.positions.push_back({(float(x) / divisions - 0.5f) * size, 0.0f, (float(z) / divisions - 0.5f) * size});
        }
    }
    for (int z = 0; z < divisions; z++) {
        for (int x = 0; x < divisions; x++) {
            const int a = z * (divisions + 1) + x, b = a + 1, c = a + divisions + 2, d = a + divisions + 1;
            const glm::vec2 u0(float(x) / divisions, float(z) / divisions), u1((x + 1.0f) / divisions, (z + 1.0f) / divisions);
            // Facing up: counter-clockwise seen from above.
            mesh.faces.push_back(makeFace({d, c, b, a}, {{u0.x, u1.y}, {u1.x, u1.y}, {u1.x, u0.y}, {u0.x, u0.y}}));
        }
    }
    return mesh;
}

EditMesh EditMesh::cylinder(int sides, float radius, float height) {
    EditMesh mesh;
    sides = std::clamp(sides, 3, 128);
    for (int i = 0; i < sides; i++) {
        const float a = float(i) / sides * 6.2831853f;
        mesh.positions.push_back({std::cos(a) * radius, -height * 0.5f, std::sin(a) * radius});
    }
    for (int i = 0; i < sides; i++) {
        const float a = float(i) / sides * 6.2831853f;
        mesh.positions.push_back({std::cos(a) * radius, height * 0.5f, std::sin(a) * radius});
    }
    for (int i = 0; i < sides; i++) {
        const int j = (i + 1) % sides;
        const float u0 = float(i) / sides, u1 = float(i + 1) / sides;
        mesh.faces.push_back(makeFace({j, i, sides + i, sides + j}, {{u1, 0}, {u0, 0}, {u0, 1}, {u1, 1}}));
    }
    EditFace top, bottom;
    for (int i = 0; i < sides; i++) {
        const float a = float(i) / sides * 6.2831853f;
        const glm::vec2 uv(std::cos(a) * 0.5f + 0.5f, std::sin(a) * 0.5f + 0.5f);
        top.corners.push_back(sides + sides - 1 - i);
        top.uvs.push_back({1.0f - uv.x, uv.y});
        bottom.corners.push_back(i);
        bottom.uvs.push_back(uv);
    }
    // Seen from above, increasing angle runs clockwise: the top goes the other way, the bottom this way.
    mesh.faces.push_back(top);
    mesh.faces.push_back(bottom);
    return mesh;
}

EditMesh EditMesh::cone(int sides, float radius, float height) {
    EditMesh mesh;
    sides = std::clamp(sides, 3, 128);
    for (int i = 0; i < sides; i++) {
        const float a = float(i) / sides * 6.2831853f;
        mesh.positions.push_back({std::cos(a) * radius, -height * 0.5f, std::sin(a) * radius});
    }
    mesh.positions.push_back({0.0f, height * 0.5f, 0.0f});
    for (int i = 0; i < sides; i++) {
        const int j = (i + 1) % sides;
        mesh.faces.push_back(makeFace({j, i, sides}, {{float(i + 1) / sides, 0}, {float(i) / sides, 0}, {(i + 0.5f) / sides, 1}}));
    }
    EditFace bottom;
    for (int i = 0; i < sides; i++) {
        const float a = float(i) / sides * 6.2831853f;
        bottom.corners.push_back(i);
        bottom.uvs.push_back({std::cos(a) * 0.5f + 0.5f, std::sin(a) * 0.5f + 0.5f});
    }
    mesh.faces.push_back(bottom);
    return mesh;
}

EditMesh EditMesh::sphere(int segments, int rings, float radius) {
    EditMesh mesh;
    segments = std::clamp(segments, 3, 128);
    rings = std::clamp(rings, 2, 64);
    mesh.positions.push_back({0.0f, radius, 0.0f});
    for (int r = 1; r < rings; r++) {
        const float phi = float(r) / rings * 3.14159265f;
        for (int s = 0; s < segments; s++) {
            const float theta = float(s) / segments * 6.2831853f;
            mesh.positions.push_back({std::sin(phi) * std::cos(theta) * radius, std::cos(phi) * radius, std::sin(phi) * std::sin(theta) * radius});
        }
    }
    mesh.positions.push_back({0.0f, -radius, 0.0f});
    const int bottom = (int)mesh.positions.size() - 1;
    auto ring = [&](int r, int s) { return 1 + (r - 1) * segments + (s % segments); };
    for (int s = 0; s < segments; s++) {
        const float u0 = float(s) / segments, u1 = float(s + 1) / segments;
        mesh.faces.push_back(makeFace({0, ring(1, s + 1), ring(1, s)}, {{(u0 + u1) * 0.5f, 1}, {u1, 1.0f - 1.0f / rings}, {u0, 1.0f - 1.0f / rings}}));
        for (int r = 1; r < rings - 1; r++) {
            const float v0 = 1.0f - float(r) / rings, v1 = 1.0f - float(r + 1) / rings;
            mesh.faces.push_back(makeFace({ring(r, s), ring(r, s + 1), ring(r + 1, s + 1), ring(r + 1, s)}, {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}}));
        }
        mesh.faces.push_back(makeFace({bottom, ring(rings - 1, s), ring(rings - 1, s + 1)}, {{(u0 + u1) * 0.5f, 0}, {u0, 1.0f / rings}, {u1, 1.0f / rings}}));
    }
    return mesh;
}

EditMesh EditMesh::fromMeshData(const MeshData& data) {
    EditMesh mesh;
    std::map<std::tuple<long, long, long>, int> joined;
    auto indexOf = [&](glm::vec3 p) {
        const auto key = std::make_tuple(std::lround(p.x * 10000.0f), std::lround(p.y * 10000.0f), std::lround(p.z * 10000.0f));
        auto found = joined.find(key);
        if (found != joined.end()) return found->second;
        mesh.positions.push_back(p);
        return joined[key] = (int)mesh.positions.size() - 1;
    };
    for (int t = 0; t + 2 < data.vertexCount(); t += 3) {
        EditFace face;
        for (int c = 0; c < 3; c++) {
            const float* v = &data.vertices[(size_t)(t + c) * 8];
            face.corners.push_back(indexOf({v[0], v[1], v[2]}));
            face.uvs.push_back({v[6], v[7]});
        }
        // Triangles that collapsed to a line or point cannot be edited or seen.
        if (face.corners[0] != face.corners[1] && face.corners[1] != face.corners[2] && face.corners[0] != face.corners[2]) {
            mesh.faces.push_back(std::move(face));
        }
    }
    return mesh;
}

bool EditMesh::loadObj(const fs::path& file, EditMesh& mesh, std::string* error) {
    MeshData data;
    if (!ObjLoader::load(file, data, error)) return false;
    mesh = fromMeshData(data);
    return true;
}

bool EditMesh::saveObj(const fs::path& file, std::string* error) const {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "Cannot write " + file.filename().string();
        return false;
    }
    out << "# Written by the Aureon Model Editor\n";
    for (const glm::vec3& p : positions) out << "v " << p.x << ' ' << p.y << ' ' << p.z << '\n';

    std::vector<glm::vec3> vertexNormals;
    if (smooth) {
        vertexNormals.assign(positions.size(), glm::vec3(0.0f));
        for (size_t f = 0; f < faces.size(); f++) {
            const glm::vec3 n = faceNormal((int)f);
            for (int c : faces[f].corners) vertexNormals[c] += n;
        }
        for (glm::vec3& n : vertexNormals) n = glm::length(n) > 0.0f ? glm::normalize(n) : glm::vec3(0, 1, 0);
        for (const glm::vec3& n : vertexNormals) out << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
    }

    int uvCount = 0, normalBase = smooth ? 0 : 0;
    for (const EditFace& face : faces) {
        for (const glm::vec2& uv : face.uvs) out << "vt " << uv.x << ' ' << uv.y << '\n';
    }
    if (!smooth) {
        for (size_t f = 0; f < faces.size(); f++) {
            const glm::vec3 n = faceNormal((int)f);
            out << "vn " << n.x << ' ' << n.y << ' ' << n.z << '\n';
        }
    }
    for (size_t f = 0; f < faces.size(); f++) {
        out << "f";
        for (size_t c = 0; c < faces[f].corners.size(); c++) {
            const int normal = smooth ? faces[f].corners[c] + 1 : (int)f + 1;
            out << ' ' << faces[f].corners[c] + 1 << '/' << ++uvCount << '/' << normal + normalBase;
        }
        out << '\n';
    }
    if (!out) {
        if (error) *error = "Writing " + file.filename().string() + " failed.";
        return false;
    }
    return true;
}

glm::vec3 EditMesh::faceNormal(int face) const {
    // Newell's method, which holds for any polygon, however it is bent.
    const EditFace& f = faces[(size_t)face];
    glm::vec3 n(0.0f);
    for (size_t i = 0; i < f.corners.size(); i++) {
        const glm::vec3 a = positions[(size_t)f.corners[i]], b = positions[(size_t)f.corners[(i + 1) % f.corners.size()]];
        n += glm::vec3((a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y));
    }
    return glm::length(n) > 1e-12f ? glm::normalize(n) : glm::vec3(0.0f, 1.0f, 0.0f);
}

glm::vec3 EditMesh::faceCenter(int face) const {
    glm::vec3 sum(0.0f);
    const EditFace& f = faces[(size_t)face];
    for (int c : f.corners) sum += positions[(size_t)c];
    return sum / float(std::max<size_t>(1, f.corners.size()));
}

glm::vec3 EditMesh::center() const {
    if (positions.empty()) return glm::vec3(0.0f);
    glm::vec3 low = positions[0], high = positions[0];
    for (const glm::vec3& p : positions) {
        low = glm::min(low, p);
        high = glm::max(high, p);
    }
    return (low + high) * 0.5f;
}

MeshData EditMesh::toMeshData() const {
    MeshData data;
    std::vector<glm::vec3> vertexNormals;
    if (smooth) {
        vertexNormals.assign(positions.size(), glm::vec3(0.0f));
        for (size_t f = 0; f < faces.size(); f++) {
            const glm::vec3 n = faceNormal((int)f);
            for (int c : faces[f].corners) vertexNormals[(size_t)c] += n;
        }
        for (glm::vec3& n : vertexNormals) n = glm::length(n) > 0.0f ? glm::normalize(n) : glm::vec3(0, 1, 0);
    }

    for (size_t f = 0; f < faces.size(); f++) {
        const EditFace& face = faces[f];
        const glm::vec3 flat = faceNormal((int)f);
        auto push = [&](size_t corner) {
            const glm::vec3& p = positions[(size_t)face.corners[corner]];
            const glm::vec3 n = smooth ? vertexNormals[(size_t)face.corners[corner]] : flat;
            const glm::vec2 uv = corner < face.uvs.size() ? face.uvs[corner] : glm::vec2(0.0f);
            const float values[8] = {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y};
            data.vertices.insert(data.vertices.end(), values, values + 8);
        };
        for (size_t c = 1; c + 1 < face.corners.size(); c++) {
            push(0);
            push(c);
            push(c + 1);
        }
    }
    return data;
}

int EditMesh::pick(glm::vec3 origin, glm::vec3 direction, float& distance) const {
    int best = -1;
    float nearest = 1e30f;
    for (size_t f = 0; f < faces.size(); f++) {
        const EditFace& face = faces[f];
        for (size_t c = 1; c + 1 < face.corners.size(); c++) {
            float t;
            if (rayTriangle(origin, direction, positions[(size_t)face.corners[0]], positions[(size_t)face.corners[c]], positions[(size_t)face.corners[c + 1]], t) && t < nearest) {
                nearest = t;
                best = (int)f;
            }
        }
    }
    distance = nearest;
    return best;
}

std::set<int> EditMesh::verticesOf(const std::set<int>& faceSet) const {
    std::set<int> result;
    for (int f : faceSet) {
        if (f >= 0 && f < (int)faces.size()) result.insert(faces[(size_t)f].corners.begin(), faces[(size_t)f].corners.end());
    }
    return result;
}

std::set<int> EditMesh::facesUsing(const std::set<int>& vertexSet) const {
    std::set<int> result;
    for (size_t f = 0; f < faces.size(); f++) {
        bool all = !faces[f].corners.empty();
        for (int c : faces[f].corners) all &= vertexSet.count(c) > 0;
        if (all) result.insert((int)f);
    }
    return result;
}

std::set<int> EditMesh::extrude(const std::set<int>& faceSet, float amount) {
    if (faceSet.empty()) return {};

    // Each vertex of the selection gets a twin, which the selected faces move onto.
    std::map<int, int> twin;
    std::map<int, glm::vec3> push;
    for (int f : faceSet) {
        const glm::vec3 n = faceNormal(f);
        for (int c : faces[(size_t)f].corners) push[c] += n;
    }
    for (const auto& entry : push) {
        twin[entry.first] = (int)positions.size();
        const glm::vec3 direction = glm::length(entry.second) > 1e-6f ? glm::normalize(entry.second) : glm::vec3(0.0f);
        positions.push_back(positions[(size_t)entry.first] + direction * amount);
    }

    // An edge on the border of the selection is one whose other side is not selected.
    std::set<std::pair<int, int>> directed;
    for (int f : faceSet) {
        const auto& corners = faces[(size_t)f].corners;
        for (size_t i = 0; i < corners.size(); i++) directed.insert({corners[i], corners[(i + 1) % corners.size()]});
    }
    std::vector<EditFace> walls;
    for (int f : faceSet) {
        const EditFace& face = faces[(size_t)f];
        for (size_t i = 0; i < face.corners.size(); i++) {
            const int a = face.corners[i], b = face.corners[(i + 1) % face.corners.size()];
            if (directed.count({b, a})) continue;
            // Facing away from the extruded faces, along the edge they ran in.
            walls.push_back(makeFace({b, twin[b], twin[a], a}, {QUAD_UVS[0], QUAD_UVS[1], QUAD_UVS[2], QUAD_UVS[3]}));
        }
    }

    for (int f : faceSet) {
        for (int& c : faces[(size_t)f].corners) c = twin[c];
    }
    for (EditFace& wall : walls) faces.push_back(std::move(wall));
    return faceSet;
}

void EditMesh::deleteFaces(const std::set<int>& faceSet) {
    std::vector<EditFace> kept;
    for (size_t f = 0; f < faces.size(); f++) {
        if (!faceSet.count((int)f)) kept.push_back(std::move(faces[f]));
    }
    faces = std::move(kept);
    removeUnusedVertices();
}

std::set<int> EditMesh::subdivide(const std::set<int>& faceSet) {
    std::map<std::pair<int, int>, int> midpoints;
    auto middle = [&](int a, int b) {
        const std::pair<int, int> key{std::min(a, b), std::max(a, b)};
        auto found = midpoints.find(key);
        if (found != midpoints.end()) return found->second;
        positions.push_back((positions[(size_t)a] + positions[(size_t)b]) * 0.5f);
        return midpoints[key] = (int)positions.size() - 1;
    };

    std::vector<EditFace> result;
    std::set<int> created;
    std::vector<EditFace> old = std::move(faces);
    for (size_t f = 0; f < old.size(); f++) {
        const EditFace& face = old[f];
        if (!faceSet.count((int)f) || face.corners.size() < 3) {
            result.push_back(face);
            continue;
        }

        // Every edge is cut in half and the face is joined to its own middle.
        const size_t n = face.corners.size();
        positions.push_back(glm::vec3(0.0f));
        const int centerIndex = (int)positions.size() - 1;
        glm::vec3 sum(0.0f);
        glm::vec2 uvSum(0.0f);
        for (size_t i = 0; i < n; i++) {
            sum += positions[(size_t)face.corners[i]];
            uvSum += face.uvs[i];
        }
        positions[(size_t)centerIndex] = sum / float(n);
        const glm::vec2 uvCenter = uvSum / float(n);

        for (size_t i = 0; i < n; i++) {
            const size_t next = (i + 1) % n, previous = (i + n - 1) % n;
            const int mNext = middle(face.corners[i], face.corners[next]);
            const int mPrevious = middle(face.corners[previous], face.corners[i]);
            EditFace piece = makeFace({face.corners[i], mNext, centerIndex, mPrevious},
                                      {face.uvs[i], (face.uvs[i] + face.uvs[next]) * 0.5f, uvCenter, (face.uvs[previous] + face.uvs[i]) * 0.5f});
            created.insert((int)result.size());
            result.push_back(std::move(piece));
        }
    }
    faces = std::move(result);
    return created;
}

void EditMesh::flip(const std::set<int>& faceSet) {
    for (int f : faceSet) {
        std::reverse(faces[(size_t)f].corners.begin(), faces[(size_t)f].corners.end());
        std::reverse(faces[(size_t)f].uvs.begin(), faces[(size_t)f].uvs.end());
    }
}

void EditMesh::translate(const std::set<int>& vertexSet, glm::vec3 delta) {
    for (int v : vertexSet) positions[(size_t)v] += delta;
}

void EditMesh::transform(const std::set<int>& vertexSet, const glm::mat4& matrix) {
    for (int v : vertexSet) positions[(size_t)v] = glm::vec3(matrix * glm::vec4(positions[(size_t)v], 1.0f));
}

int EditMesh::mergeByDistance(float distance) {
    std::vector<int> target(positions.size());
    int merged = 0;
    for (size_t i = 0; i < positions.size(); i++) {
        target[i] = (int)i;
        for (size_t j = 0; j < i; j++) {
            if (target[j] == (int)j && glm::distance(positions[i], positions[j]) <= distance) {
                target[i] = (int)j;
                merged++;
                break;
            }
        }
    }
    std::vector<EditFace> kept;
    for (EditFace& face : faces) {
        EditFace clean;
        for (size_t c = 0; c < face.corners.size(); c++) {
            const int t = target[(size_t)face.corners[c]];
            // Two corners that became one leave a polygon with one fewer.
            if (!clean.corners.empty() && clean.corners.back() == t) continue;
            clean.corners.push_back(t);
            clean.uvs.push_back(face.uvs[c]);
        }
        if (clean.corners.size() > 1 && clean.corners.front() == clean.corners.back()) {
            clean.corners.pop_back();
            clean.uvs.pop_back();
        }
        if (clean.corners.size() >= 3) kept.push_back(std::move(clean));
    }
    faces = std::move(kept);
    removeUnusedVertices();
    return merged;
}

void EditMesh::removeUnusedVertices() {
    std::vector<int> remap(positions.size(), -1);
    for (const EditFace& face : faces) {
        for (int c : face.corners) remap[(size_t)c] = 0;
    }
    std::vector<glm::vec3> kept;
    for (size_t i = 0; i < positions.size(); i++) {
        if (remap[i] == 0) {
            remap[i] = (int)kept.size();
            kept.push_back(positions[i]);
        }
    }
    for (EditFace& face : faces) {
        for (int& c : face.corners) c = remap[(size_t)c];
    }
    positions = std::move(kept);
}

void EditMesh::boxProjectUVs(float scale) {
    scale = std::max(scale, 1e-4f);
    for (size_t f = 0; f < faces.size(); f++) {
        const glm::vec3 n = glm::abs(faceNormal((int)f));
        for (size_t c = 0; c < faces[f].corners.size(); c++) {
            const glm::vec3 p = positions[(size_t)faces[f].corners[c]];
            glm::vec2 uv;
            if (n.y >= n.x && n.y >= n.z) uv = {p.x, p.z};
            else if (n.x >= n.z) uv = {p.z, p.y};
            else uv = {p.x, p.y};
            faces[f].uvs[c] = uv / scale;
        }
    }
}

bool EditMesh::valid() const {
    for (const EditFace& face : faces) {
        if (face.corners.size() < 3 || face.uvs.size() != face.corners.size()) return false;
        for (int c : face.corners) {
            if (c < 0 || c >= (int)positions.size()) return false;
        }
    }
    return true;
}
