#pragma once
#include "Cube.h"
#include <GLFW/glfw3.h>
#include <vector>
#include <memory>

class Player : public Cube {
public:
    Player(glm::vec3 startPos);

    void update(float deltaTime, const std::vector<std::shared_ptr<Shape>>& worldObjects);
    void move(glm::vec3 direction);
    void jump();

    void setCrouch(bool crouching);
    void setGrounded(bool grounded);

    glm::vec3 getCameraPosition() const;

    float runSpeed = 5.0f;

private:
    float walkSpeed = 2.5f;
    float crouchSpeed = 1.0f;
    float jumpForce = 5.0f;

    bool isGrounded;
    bool isCrouching;

    float standHeight = 2.0f;
    float crouchHeight = 1.0f;
    float playerWidth = 0.8f;

    void applyGravity(float deltaTime);
    void checkCollisions(const std::vector<std::shared_ptr<Shape>>& worldObjects, glm::vec3 velocityStep);
};