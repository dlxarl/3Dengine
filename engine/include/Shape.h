#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <memory>
#include "SceneObject.h"
#include "Shader.h"
#include "Texture.h"

// What an object means to the game rules; see GameSession.
enum class GameRole { None, Pickup, Goal, Hazard };

class Shape : public SceneObject {
public:
    Shape();
    ~Shape() override;
    virtual void draw(Shader& shader) = 0;

    void setColor(glm::vec3 newColor);
    glm::vec3 getColor() const;
    void addTexture(std::shared_ptr<Texture> tex);

    // Replaces any texture of the same type; a null texture just removes it.
    void setTexture(const std::string& type, std::shared_ptr<Texture> tex);
    std::shared_ptr<Texture> getTexture(const std::string& type) const;
    const std::vector<std::shared_ptr<Texture>>& getTextures() const { return textures; }

    glm::vec3 velocity;

    bool useGravity;
    bool isStatic;
    bool hasCollision;

    // Rendering properties
    bool castShadow = true;
    bool shadowOnly = false;   // left out of the camera's image, for a body the camera is inside
    bool tintTexture = false;  // multiply the albedo texture by the object color

    // Gameplay
    GameRole gameRole = GameRole::None;
    glm::vec3 patrolOffset = glm::vec3(0.0f);  // a hazard swings this far either side of its position
    float patrolSeconds = 3.0f;                // for one full swing

    bool checkCollision(Shape& other);

protected:
    unsigned int VAO, VBO;
    int vertexCount = 0;
    glm::vec3 color;
    std::vector<std::shared_ptr<Texture>> textures;

    // Uploads the model matrix and material state shared by every primitive.
    void bindMaterial(Shader& shader);
};
