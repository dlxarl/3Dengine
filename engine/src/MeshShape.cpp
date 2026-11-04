#include "MeshShape.h"

GpuMesh::GpuMesh(const MeshData& data) {
    vertexCount = data.vertexCount();

    triangles.reserve(vertexCount);
    for (int i = 0; i < vertexCount; i++) {
        triangles.emplace_back(data.vertices[i * 8], data.vertices[i * 8 + 1], data.vertices[i * 8 + 2]);
    }
    if (!triangles.empty()) {
        boundsMin = boundsMax = triangles.front();
        for (const glm::vec3& point : triangles) {
            boundsMin = glm::min(boundsMin, point);
            boundsMax = glm::max(boundsMax, point);
        }
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, data.vertices.size() * sizeof(float), data.vertices.data(), GL_STATIC_DRAW);

    // layout: pos(3), normal(3), uv(2)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

GpuMesh::~GpuMesh() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void GpuMesh::draw() const {
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void MeshShape::setMesh(std::shared_ptr<GpuMesh> newMesh, const std::string& newAssetPath) {
    mesh = std::move(newMesh);
    assetPath = newAssetPath;
}

void MeshShape::getBounds(glm::vec3& center, float& radius) const {
    if (!mesh) {
        Shape::getBounds(center, radius);
        return;
    }
    const glm::vec3 low = mesh->getBoundsMin();
    const glm::vec3 high = mesh->getBoundsMax();
    const glm::vec3 size = getWorldScale();
    center = glm::vec3(model * glm::vec4((low + high) * 0.5f, 1.0f));
    radius = 0.5f * glm::length(high - low) * std::max(size.x, std::max(size.y, size.z));
}

void MeshShape::draw(Shader& shader) {
    if (!mesh) return;

    bindMaterial(shader);
    mesh->draw();
}
