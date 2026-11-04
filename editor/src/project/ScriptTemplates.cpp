#include "ScriptBuilder.h"

namespace {
    const char* const PLAYER_CONTROL = R"cpp(#include "CameraObject.h"
#include "Player.h"
#include "Script.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

// Walks and jumps the Player this script is on, and keeps the scene's camera behind it.
// W A S D move, Shift runs, Space jumps, Q and E turn the view.
//
// Put it on a Player object: select the Player and choose PlayerControl under Script.
class PlayerControl : public Script {
public:
    float walkSpeed = 4.5f;       // metres per second
    float runSpeed = 7.5f;
    float cameraDistance = 6.0f;  // metres behind the player
    float cameraHeight = 1.8f;    // metres above the player's centre

    void onStart() override {
        player = dynamic_cast<Player*>(self);
        yaw = 0.0f;
    }

    void onUpdate(float deltaTime, const GameInput& input) override {
        if (!player) return;

        yaw += input.turnDegrees;
        const float radians = glm::radians(yaw);
        const glm::vec3 forward(std::sin(radians), 0.0f, -std::cos(radians));
        const glm::vec3 right(-forward.z, 0.0f, forward.x);

        glm::vec3 direction = forward * input.moveForward + right * input.moveRight;
        if (glm::length(direction) > 1.0f) direction = glm::normalize(direction);

        player->runSpeed = input.sprint ? runSpeed : walkSpeed;
        player->move(direction);
        if (input.jump) player->jump();
        player->update(deltaTime, scene->getShapes());

        followWithCamera(forward);
    }

private:
    Player* player = nullptr;
    float yaw = 0.0f;

    void followWithCamera(const glm::vec3& forward) {
        CameraObject* camera = scene->getMainCamera();
        if (!camera) return;

        const glm::vec3 up(0.0f, 1.0f, 0.0f);
        const glm::vec3 eye = player->position - forward * cameraDistance + up * cameraHeight;
        camera->setWorldMatrix(glm::inverse(glm::lookAt(eye, player->position + up * 0.5f, up)));
    }
};

AUREON_SCRIPT(PlayerControl)
)cpp";

    const char* const SPINNER = R"cpp(#include "Script.h"

// Turns its object around the vertical axis, all the time the scene plays.
class Spinner : public Script {
public:
    float degreesPerSecond = 45.0f;

    void onUpdate(float deltaTime, const GameInput& input) override {
        self->rotate(degreesPerSecond * deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));
    }
};

AUREON_SCRIPT(Spinner)
)cpp";

    const char* const EMPTY_SCRIPT = R"cpp(#include "Script.h"

// self is the object this script is on, and scene is the scene it plays in.
class EmptyScript : public Script {
public:
    void onStart() override {
        // Runs once, when the scene starts playing.
    }

    void onUpdate(float deltaTime, const GameInput& input) override {
        // Runs every frame. input holds the keys: moveForward, moveRight, jump, sprint, attack...
    }
};

AUREON_SCRIPT(EmptyScript)
)cpp";
}

const std::vector<ScriptBuilder::Template>& ScriptBuilder::templates() {
    static const std::vector<Template> all = {
        {"PlayerControl", "Walk, run and jump, with a camera behind the player.", PLAYER_CONTROL},
        {"Spinner", "Turns an object around all the time.", SPINNER},
        {"EmptyScript", "A script that does nothing yet.", EMPTY_SCRIPT},
    };
    return all;
}
