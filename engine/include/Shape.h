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
enum class GameRole {
    None, Pickup, Goal, Hazard,
    Ground,            // walked on, wherever its mesh is
    Villager, Bandit,
    Horse,
    PlayerBody,        // what the player looks like; the Player object itself is only its collider
    Sword, Bow, Arrow,
    Item,              // picked up; gameParameter names it, as "item" or "item:count"
    Chest,             // opened once; gameParameter lists the contents, as "item:count, gold:25"
    Bed,               // sleeping passes the night and restores health
    Sign,              // gameParameter is what it says
    Water,             // the surface: deep enough below it, the player swims
    Deer, Wolf, Livestock,
    Limb,              // swings with its parent's walk, or its wingbeat
    NightGlow,         // lit up from dusk to dawn
    Boar,              // charges whoever comes too close
    Bear,              // slow to anger, hits hard
    Rabbit,            // darts away from the player
    Bird,              // pecks at the ground and takes to the air when disturbed
    Dragon,            // sleeps at its lair; once woken, flies, breathes fire and lands to fight
    Fireball,          // what dragons breathe; kept hidden until then
};

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
    bool emissive = false;     // drawn at full color, unaffected by light
    float waveHeight = 0.0f;   // swell of a water surface; needs a mesh with enough vertices
    float sway = 0.0f;         // bending in the wind, more the higher above its origin, as plants do
    float lightRange = 0.0f;   // while emissive, it lights what is this near, as a fire does
    glm::vec3 lightColor = glm::vec3(1.0f, 0.62f, 0.3f);

    // Gameplay
    GameRole gameRole = GameRole::None;
    glm::vec3 patrolOffset = glm::vec3(0.0f);  // a hazard swings this far either side of its position
    float patrolSeconds = 3.0f;                // for one full swing
    std::string gameParameter;                 // what the role needs; see GameRole
    float limbSwing = 0.0f;                    // a Limb's swing either way at full stride, in degrees
    float limbPhase = 0.0f;                    // how far ahead of the stride it swings, in degrees
    int limbAxis = 0;                          // 0 swings forward and back, 1 side to side, 2 up and down with wingbeats

    // A sphere around everything drawn, in world space, for culling.
    virtual void getBounds(glm::vec3& center, float& radius) const;

    bool checkCollision(Shape& other);

protected:
    unsigned int VAO, VBO;
    int vertexCount = 0;
    glm::vec3 color;
    std::vector<std::shared_ptr<Texture>> textures;

    // Uploads the model matrix and material state shared by every primitive.
    void bindMaterial(Shader& shader);
};
