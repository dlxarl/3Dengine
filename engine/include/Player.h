#pragma once
#include "Cube.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

class Player : public Cube {
public:
    Player(glm::vec3 startPos);

    ObjectType getType() const override { return ObjectType::Player; }

    void update(float deltaTime, const std::vector<std::shared_ptr<Shape>>& worldObjects);
    void move(glm::vec3 direction);
    void jump();

    void setCrouch(bool crouching);
    void setGrounded(bool grounded);

    glm::vec3 getCameraPosition() const;

    // Whether the engine's game rules drive this player: movement, camera, combat, the HUD.
    // Off, the player is an ordinary object that scripts move.
    bool gameControls = true;

    float runSpeed = 5.0f;

private:
    float walkSpeed = 2.5f;
    float crouchSpeed = 1.0f;
    float jumpForce = 5.0f;

    bool isGrounded;
    bool isCrouching;

    // Time left to jump after last touching the ground. Contact with the ground is only
    // detected on some frames, so without this a quick press of the key can do nothing.
    float jumpGrace = 0.0f;

    float standHeight = 2.0f;
    float crouchHeight = 1.0f;
    float playerWidth = 0.8f;

    void applyGravity(float deltaTime);
    void checkCollisions(const std::vector<std::shared_ptr<Shape>>& worldObjects, glm::vec3 velocityStep);
};