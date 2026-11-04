#include "Shape.h"

#include <algorithm>

Shape::Shape() {
    color = glm::vec3(1.0f);

    velocity = glm::vec3(0.0f);

    useGravity = false;
    isStatic = false;
    hasCollision = true;

    VAO = 0;
    VBO = 0;
}

Shape::~Shape() {
    if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    if (VBO != 0) glDeleteBuffers(1, &VBO);
}

void Shape::setColor(glm::vec3 newColor) {
    color = newColor;
}

glm::vec3 Shape::getColor() const {
    return color;
}

void Shape::addTexture(std::shared_ptr<Texture> tex) {
    textures.push_back(tex);
}

void Shape::setTexture(const std::string& type, std::shared_ptr<Texture> tex) {
    textures.erase(std::remove_if(textures.begin(), textures.end(),
                                  [&](const std::shared_ptr<Texture>& t) { return t->type == type; }),
                   textures.end());
    if (tex) {
        textures.push_back(tex);
    }
}

std::shared_ptr<Texture> Shape::getTexture(const std::string& type) const {
    for (const auto& tex : textures) {
        if (tex->type == type) return tex;
    }
    return nullptr;
}

void Shape::bindMaterial(Shader& shader) {
    shader.setMat4("model", model);

    shader.setVec3("objectColor", color);
    shader.setBool("material.tintAlbedo", tintTexture);
    shader.setBool("material.emissive", emissive);
    shader.setFloat("waveHeight", waveHeight);
    shader.setFloat("sway", sway);
    shader.setBool("material.hasAlbedo", false);
    shader.setBool("material.hasNormal", false);
    shader.setBool("material.hasMetallic", false);
    shader.setBool("material.hasRoughness", false);
    shader.setBool("material.hasAO", false);

    // Sprawdzamy jakie tekstury sa przypisane do obiektu
    for (unsigned int i = 0; i < textures.size(); i++) {
        const std::string& name = textures[i]->type;

        if (name == "texture_albedo") {
            shader.setInt("material.albedoMap", i);
            shader.setBool("material.hasAlbedo", true);
        } else if (name == "texture_normal") {
            shader.setInt("material.normalMap", i);
            shader.setBool("material.hasNormal", true);
        } else if (name == "texture_metallic") {
            shader.setInt("material.metallicMap", i);
            shader.setBool("material.hasMetallic", true);
        } else if (name == "texture_roughness") {
            shader.setInt("material.roughnessMap", i);
            shader.setBool("material.hasRoughness", true);
        } else if (name == "texture_ao") {
            shader.setInt("material.aoMap", i);
            shader.setBool("material.hasAO", true);
        }

        textures[i]->bind(i);
    }
}

void Shape::getBounds(glm::vec3& center, float& radius) const {
    // Every primitive fits in a unit cube around its position.
    const glm::vec3 size = getWorldScale();
    center = getWorldPosition();
    radius = 0.87f * std::max(size.x, std::max(size.y, size.z));
}

bool Shape::checkCollision(Shape& other) {
    const glm::vec3 worldPos = getWorldPosition();
    const glm::vec3 otherPos = other.getWorldPosition();
    const glm::vec3 half = 0.5f * getWorldScale();
    const glm::vec3 otherHalf = 0.5f * other.getWorldScale();

    bool collisionX = worldPos.x + half.x >= otherPos.x - otherHalf.x &&
                      otherPos.x + otherHalf.x >= worldPos.x - half.x;

    bool collisionY = worldPos.y + half.y >= otherPos.y - otherHalf.y &&
                      otherPos.y + otherHalf.y >= worldPos.y - half.y;

    bool collisionZ = worldPos.z + half.z >= otherPos.z - otherHalf.z &&
                      otherPos.z + otherHalf.z >= worldPos.z - half.z;

    return collisionX && collisionY && collisionZ;
}
