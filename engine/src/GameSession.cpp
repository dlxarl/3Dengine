#include "GameSession.h"
#include "Player.h"

#include <algorithm>
#include <cmath>

namespace {
    const float RUN_SPEED = 6.5f;
    const float FALL_LIMIT = 14.0f;       // how far below the start counts as lost
    const float PICKUP_SPIN = 140.0f;     // degrees per second
    const float CAMERA_DISTANCE = 11.0f;
    const float CAMERA_HEIGHT = 6.0f;
    const float CAMERA_LOOK_AHEAD = 1.0f;
    const float CAMERA_FOLLOW = 7.0f;     // higher catches up with the player sooner
    const float TWO_PI = 6.2831853f;
    const float MAX_LOOK_PITCH = 85.0f;
    const float EYE_FOLLOW = 20.0f;       // absorbs the small hops of ground contact

    // Where the camera sits for a given yaw, which is 0 looking towards -Z.
    glm::vec3 cameraForward(float yawDegrees) {
        const float yaw = glm::radians(yawDegrees);
        return glm::vec3(std::sin(yaw), 0.0f, -std::cos(yaw));
    }
}

void GameSession::begin(SceneGraph& scene) {
    status = GameStatus();
    patrols.clear();

    Player* player = scene.getPlayer();
    status.hasPlayer = player != nullptr;
    if (!player) return;

    spawnPoint = player->position;
    killHeight = spawnPoint.y - FALL_LIMIT;
    cameraYaw = 0.0f;
    cameraFocus = spawnPoint;
    player->runSpeed = RUN_SPEED;

    for (const auto& shape : scene.getShapes()) {
        if (shape->gameRole == GameRole::None) continue;

        // These are touched, not stood on, so they must not stop the player.
        shape->hasCollision = false;

        if (shape->gameRole == GameRole::Pickup) {
            status.total++;
        } else if (shape->gameRole == GameRole::Hazard && glm::length(shape->patrolOffset) > 0.0f) {
            patrols.push_back({shape->id, shape->position});
        }
    }

    CameraObject* camera = scene.getMainCamera();
    setFirstPerson(scene, camera && camera->firstPerson);
}

void GameSession::setFirstPerson(SceneGraph& scene, bool firstPerson) {
    status.firstPerson = firstPerson;
    cameraPitch = 0.0f;

    if (Player* player = scene.getPlayer()) {
        // The camera sits inside the body, which still throws a shadow to land by.
        player->shadowOnly = firstPerson;
        eyeHeight = player->getCameraPosition().y;
    }
    moveCamera(scene, 0.0f);
}

void GameSession::update(SceneGraph& scene, const GameInput& input, float deltaTime) {
    if (!status.hasPlayer || !scene.getPlayer()) return;

    animate(scene, deltaTime);

    if (!status.won) {
        status.seconds += deltaTime;
        movePlayer(scene, input, deltaTime);
        applyRules(scene);
    }

    cameraYaw += input.turnDegrees;
    cameraPitch = std::clamp(cameraPitch + input.lookUpDegrees, -MAX_LOOK_PITCH, MAX_LOOK_PITCH);
    moveCamera(scene, deltaTime);
}

void GameSession::animate(SceneGraph& scene, float deltaTime) {
    for (const auto& shape : scene.getShapes()) {
        if (shape->gameRole == GameRole::Pickup && shape->visible) {
            shape->rotate(PICKUP_SPIN * deltaTime, glm::vec3(0.0f, 1.0f, 0.0f));
        }
    }

    for (const Patrol& patrol : patrols) {
        if (Shape* shape = scene.findAs<Shape>(patrol.id)) {
            const float phase = status.seconds / std::max(shape->patrolSeconds, 0.1f);
            shape->setPosition(patrol.home + shape->patrolOffset * std::sin(phase * TWO_PI));
        }
    }
}

void GameSession::movePlayer(SceneGraph& scene, const GameInput& input, float deltaTime) {
    Player* player = scene.getPlayer();

    const glm::vec3 forward = cameraForward(cameraYaw);
    const glm::vec3 right(-forward.z, 0.0f, forward.x);

    glm::vec3 direction = forward * input.moveForward + right * input.moveRight;
    if (glm::length(direction) > 1.0f) {
        direction = glm::normalize(direction);
    }

    player->move(direction);
    if (input.jump) {
        player->jump();
    }
    player->update(deltaTime, scene.getShapes());
}

void GameSession::applyRules(SceneGraph& scene) {
    Player* player = scene.getPlayer();

    status.goalLocked = false;

    for (const auto& shape : scene.getShapes()) {
        if (shape->gameRole == GameRole::None || !shape->isVisibleInHierarchy()) continue;
        if (!player->checkCollision(*shape)) continue;

        switch (shape->gameRole) {
            case GameRole::Pickup:
                shape->visible = false;
                status.collected++;
                break;
            case GameRole::Goal:
                status.goalLocked = status.collected < status.total;
                status.won = !status.goalLocked;
                break;
            case GameRole::Hazard:
                respawn(scene);
                return;
            case GameRole::None:
                break;
        }
    }

    if (player->position.y < killHeight) {
        respawn(scene);
    }
}

void GameSession::respawn(SceneGraph& scene) {
    Player* player = scene.getPlayer();
    player->setPosition(spawnPoint);
    player->velocity = glm::vec3(0.0f);
    status.falls++;
}

void GameSession::moveCamera(SceneGraph& scene, float deltaTime) {
    Player* player = scene.getPlayer();
    CameraObject* camera = scene.getMainCamera();
    if (!player || !camera) return;

    if (status.firstPerson) {
        glm::vec3 eye = player->getCameraPosition();
        const float eyeBlend = deltaTime > 0.0f ? 1.0f - std::exp(-EYE_FOLLOW * deltaTime) : 1.0f;
        eyeHeight += (eye.y - eyeHeight) * eyeBlend;
        eye.y = eyeHeight;

        const float pitch = glm::radians(cameraPitch);
        const glm::vec3 look = cameraForward(cameraYaw) * std::cos(pitch) + glm::vec3(0.0f, std::sin(pitch), 0.0f);
        camera->setWorldMatrix(glm::inverse(glm::lookAt(eye, eye + look, glm::vec3(0.0f, 1.0f, 0.0f))));
        return;
    }

    // Ease towards the player: on the first call, deltaTime 0 means no easing is wanted.
    const float blend = deltaTime > 0.0f ? 1.0f - std::exp(-CAMERA_FOLLOW * deltaTime) : 1.0f;
    cameraFocus += (player->position - cameraFocus) * blend;

    const glm::vec3 forward = cameraForward(cameraYaw);
    const glm::vec3 eye = cameraFocus - forward * CAMERA_DISTANCE + glm::vec3(0.0f, CAMERA_HEIGHT, 0.0f);
    const glm::vec3 target = cameraFocus + forward * CAMERA_LOOK_AHEAD;

    camera->setWorldMatrix(glm::inverse(glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f))));
}
