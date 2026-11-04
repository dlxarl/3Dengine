#pragma once
#include "Shape.h"
#include <memory>
#include <string>
#include <vector>

// Geometry in the engine's vertex layout: position(3), normal(3), uv(2), one triple per triangle.
struct MeshData {
    std::vector<float> vertices;

    int vertexCount() const { return (int)(vertices.size() / 8); }
};

// A mesh uploaded to the GPU, shared by every object that uses the same asset.
class GpuMesh {
public:
    explicit GpuMesh(const MeshData& data);
    ~GpuMesh();

    GpuMesh(const GpuMesh&) = delete;
    GpuMesh& operator=(const GpuMesh&) = delete;

    void draw() const;

    // Kept on the CPU for what needs the shape itself, such as building a height field.
    const std::vector<glm::vec3>& getTriangles() const { return triangles; }

    // In object space.
    glm::vec3 getBoundsMin() const { return boundsMin; }
    glm::vec3 getBoundsMax() const { return boundsMax; }

private:
    std::vector<glm::vec3> triangles;
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    int vertexCount = 0;
};

// A scene object drawn from an imported model.
class MeshShape : public Shape {
public:
    ObjectType getType() const override { return ObjectType::Mesh; }
    void draw(Shader& shader) override;

    void setMesh(std::shared_ptr<GpuMesh> newMesh, const std::string& newAssetPath);
    const std::string& getAssetPath() const { return assetPath; }
    const std::shared_ptr<GpuMesh>& getMesh() const { return mesh; }

    void getBounds(glm::vec3& center, float& radius) const override;

private:
    std::shared_ptr<GpuMesh> mesh;
    std::string assetPath;
};
