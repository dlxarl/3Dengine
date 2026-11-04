#pragma once
#include "CharacterController.h"
#include "HeightField.h"
#include "SceneGraph.h"

#include <random>
#include <string>
#include <unordered_set>
#include <vector>

// Swings the Limb children of an object: those on the walking axes in time with a stride,
// those that beat up and down (wings) in time with a wingbeat. amount 0 stands still, 1 is a
// full swing.
void animateLimbs(SceneObject& root, float stride, float amount, float wingbeat = 0.0f, float wingAmount = 0.0f);

// Something the session should know about, such as a death that a quest may be counting.
struct ActorEvent {
    GameRole role;
    std::string name;
    glm::vec3 position;
};

// Something worth hearing: a creature attacking, being hurt, dying or taking fright.
struct ActorSound {
    enum class Kind { Attack, Hurt, Death, Alarm };
    Kind kind;
    GameRole role;
    glm::vec3 position;
};

// The living things in a game, the arrows in flight and the fire dragons breathe.
//
//   Villagers wander near home, go home at night and run from danger.
//   Bandits, wolves, boars and bears wander until the player comes close, then chase and
//   strike; a wolf that gives chase brings its pack, and wolves see further at night.
//   Deer and rabbits bolt when the player comes near. Livestock only runs when hurt.
//   Birds peck about and take to the air when disturbed, circling before they settle.
//   A dragon sleeps at its lair until the player comes near. Then it flies: it circles,
//   breathes fire and swoops to bite, and once badly hurt it lands to fight on the ground.
//
// Everyone walks round walls and deep water rather than through them, keeps out of each
// other's way, and steps aside for the player.
class GameActors {
public:
    // stationary: names of characters who keep to their place, such as merchants.
    void begin(SceneGraph& scene, const std::unordered_set<std::string>& stationary);

    // Moves everyone and returns the damage dealt to the player this frame.
    float update(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet, bool night, float deltaTime);

    // Hits everyone, and pushes everything movable, inside a cone in front of origin.
    int strike(SceneGraph& scene, glm::vec3 origin, glm::vec3 forward, float range, float damage);

    // Sends the next free arrow. Returns false when all of them are in flight.
    bool fireArrow(SceneGraph& scene, glm::vec3 origin, glm::vec3 velocity, float damage);

    // Keeps someone still and facing a point for a while, for a conversation.
    void holdStill(ObjectId id, glm::vec3 lookAt, float seconds);

    bool isAlive(ObjectId id) const;
    bool hostileNear(const SceneGraph& scene, glm::vec3 position, float range) const;

    int banditsTotal() const { return totalBandits; }
    int banditsAlive() const;

    // Filled during update; the caller takes them.
    std::vector<ActorEvent> events;

    // Likewise filled during update, and by strike; the caller takes them.
    std::vector<ActorSound> sounds;

    // How hard the player was knocked this frame, as a change of velocity; the caller takes it.
    glm::vec3 playerPush = glm::vec3(0.0f);

private:
    enum class Kind { Villager, Bandit, Deer, Wolf, Livestock, Boar, Bear, Rabbit, Bird, Dragon };
    enum class State { Idle, Walk, Chase, Flee, Dead };
    enum class Flight { Circle, Swoop, Climb, Land, Return };

    struct Npc {
        ObjectId id;
        Kind kind;
        State state;
        glm::vec3 home;
        glm::vec3 target;
        glm::vec3 knock;      // velocity from being hit, fading out
        glm::vec3 lookAt;
        glm::vec3 velocity;   // in flight
        glm::vec3 detour;     // the way round something in the way
        float detourTime;
        float yaw;
        float health;
        float wanderRadius;
        float timer;          // until the next wander decision
        float attackTimer;
        float panic;          // seconds of running away left
        float flash;          // seconds of showing a hit
        float stride;         // walking animation phase
        float wingbeat;       // flying animation phase
        float hold;           // seconds of standing still for a conversation
        float fallen;         // 0 to 1, how far a body has fallen over
        float breath;         // until a dragon breathes fire again
        float flightTime;     // left in this part of a flight
        float orbit;          // where on its circle a flier is, in radians
        float bank;           // how far a flier leans into its turn, in radians
        bool airborne;
        Flight flight;
    };

    struct Arrow {
        ObjectId id;
        glm::vec3 velocity;
        float damage;
        float life;           // seconds until it disappears; 0 means free
        bool flying;
    };

    struct Fire {
        ObjectId id;
        glm::vec3 velocity;
        glm::vec3 scale;
        float life;           // seconds left; 0 means free
        float burst;          // seconds left of its burst, after it hit
    };

    std::vector<Npc> npcs;
    std::vector<Arrow> arrows;
    std::vector<Fire> fires;
    std::vector<Shape*> props;
    const Shape* player = nullptr;
    std::mt19937 random{1234};
    int totalBandits = 0;
    glm::vec3 lastPlayerFeet = glm::vec3(0.0f);
    glm::vec3 playerVelocity = glm::vec3(0.0f);

    bool walk(const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 direction, float speed, float deltaTime,
              bool turn = true);
    void wander(Npc& npc, float deltaTime);
    void flyTowards(const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 point, float speed, float deltaTime);
    float updateDragon(SceneGraph& scene, const CharacterSurroundings& world, Npc& npc, Shape& body, glm::vec3 playerFeet,
                       float deltaTime);
    void updateBird(const CharacterSurroundings& world, Npc& npc, Shape& body, float distance, float deltaTime);
    void updateFallen(const CharacterSurroundings& world, Npc& npc, Shape& body, float deltaTime);
    void separate(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet);
    void hurt(SceneGraph& scene, Npc& npc, float damage, Shape& body, glm::vec3 from);
    void sound(ActorSound::Kind kind, const Npc& npc, const Shape& body);
    void breatheFire(SceneGraph& scene, glm::vec3 origin, glm::vec3 target);
    void updateArrows(SceneGraph& scene, const CharacterSurroundings& world, float deltaTime);
    float updateFires(SceneGraph& scene, const CharacterSurroundings& world, glm::vec3 playerFeet, float deltaTime);
    float randomRange(float low, float high);
    float groundAt(const CharacterSurroundings& world, glm::vec3 point) const;
};
