#pragma once
#include "MeshShape.h"

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// A polygon mesh that can be edited: shared vertices, faces of any size from 3 up, and a UV
// for each corner of each face. Everything here is plain data and needs no OpenGL.
struct EditFace {
    std::vector<int> corners;        // vertex indices, counter-clockwise seen from the front
    std::vector<glm::vec2> uvs;      // one per corner
};

class EditMesh {
public:
    std::vector<glm::vec3> positions;
    std::vector<EditFace> faces;

    // Smooth shading averages the normals of the faces around each vertex; flat gives every face its own.
    bool smooth = false;

    static EditMesh cube(float size = 1.0f);
    static EditMesh plane(float size = 1.0f, int divisions = 1);
    static EditMesh cylinder(int sides = 16, float radius = 0.5f, float height = 1.0f);
    static EditMesh cone(int sides = 16, float radius = 0.5f, float height = 1.0f);
    static EditMesh sphere(int segments = 16, int rings = 8, float radius = 0.5f);

    // Triangles from an imported model, with vertices at the same place joined.
    static EditMesh fromMeshData(const MeshData& data);
    static bool loadObj(const std::filesystem::path& file, EditMesh& mesh, std::string* error = nullptr);
    bool saveObj(const std::filesystem::path& file, std::string* error = nullptr) const;

    // In the engine's vertex layout, with every polygon cut into triangles.
    MeshData toMeshData() const;

    glm::vec3 faceNormal(int face) const;
    glm::vec3 faceCenter(int face) const;
    glm::vec3 center() const;

    // The nearest face a ray meets and how far along it; -1 for none.
    int pick(glm::vec3 origin, glm::vec3 direction, float& distance) const;

    std::set<int> verticesOf(const std::set<int>& faceSet) const;
    std::set<int> facesUsing(const std::set<int>& vertexSet) const;   // faces all of whose corners are in the set

    // The faces that replace the selected ones are returned, in the same order, so that they stay selected.
    std::set<int> extrude(const std::set<int>& faceSet, float amount);
    void deleteFaces(const std::set<int>& faceSet);
    std::set<int> subdivide(const std::set<int>& faceSet);
    void flip(const std::set<int>& faceSet);
    void translate(const std::set<int>& vertexSet, glm::vec3 delta);
    void transform(const std::set<int>& vertexSet, const glm::mat4& matrix);

    // Joins vertices closer than the distance; faces that collapse are dropped.
    int mergeByDistance(float distance);
    void removeUnusedVertices();

    // UVs from the side each face looks at, one metre of the model being scale units of the texture.
    void boxProjectUVs(float scale = 1.0f);

    bool valid() const;   // every index is in range, and every face has its UVs
};
