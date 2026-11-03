#pragma once
#include "SceneGraph.h"

#include <glm/glm.hpp>
#include <vector>

// One frame of player intent. Whoever owns the keyboard fills it in.
struct GameInput {
    float moveForward = 0.0f;  // -1 to 1, relative to the camera
    float moveRight = 0.0f;    // -1 to 1, relative to the camera
    bool jump = false;
    float turnDegrees = 0.0f;  // how far to swing the camera around the player this frame
    float lookUpDegrees = 0.0f;  // first person only
};

struct GameStatus {
    bool hasPlayer = false;
    bool firstPerson = false;
    bool won = false;
    bool goalLocked = false;  // the player reached the goal with pickups still missing
    int collected = 0;
    int total = 0;
    int falls = 0;
    float seconds = 0.0f;
};

// The rules of a game built from scene data. Any scene with a Player object is playable:
// pickups are collected by touching them, the goal opens once all of them are in, and
// hazards and long falls send the player back to the start.
//
// Everything here changes the scene in place, so the caller is expected to restore it
// afterwards, which is what the Editor's Play mode does.
class GameSession {
public:
    // Reads the roles of the scene's objects. Call again after replacing the scene.
    void begin(SceneGraph& scene);

    void update(SceneGraph& scene, const GameInput& input, float deltaTime);

    void setFirstPerson(SceneGraph& scene, bool firstPerson);

    const GameStatus& getStatus() const { return status; }

private:
    struct Patrol {
        ObjectId id;
        glm::vec3 home;
    };

    GameStatus status;
    std::vector<Patrol> patrols;
    glm::vec3 spawnPoint = glm::vec3(0.0f);
    float killHeight = 0.0f;
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    float eyeHeight = 0.0f;
    glm::vec3 cameraFocus = glm::vec3(0.0f);

    void animate(SceneGraph& scene, float deltaTime);
    void movePlayer(SceneGraph& scene, const GameInput& input, float deltaTime);
    void applyRules(SceneGraph& scene);
    void respawn(SceneGraph& scene);
    void moveCamera(SceneGraph& scene, float deltaTime);
};
