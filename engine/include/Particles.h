#pragma once
#include <glm/glm.hpp>

#include <random>
#include <vector>

// How a stream of particles is born and lives. Particles are soft camera-facing discs that
// fade through three colors over their lifetime.
struct ParticleSettings {
    int maxParticles = 300;
    float rate = 40.0f;              // born per second
    float lifetimeMin = 1.0f;
    float lifetimeMax = 2.0f;
    float speedMin = 1.0f;
    float speedMax = 2.0f;
    float coneDegrees = 25.0f;       // spread around the emitter's up (+Y) direction
    float radius = 0.1f;             // size of the disc they are born on
    glm::vec3 gravity = glm::vec3(0.0f, -2.0f, 0.0f);  // acceleration, in the world; up makes smoke rise
    float drag = 0.0f;
    float turbulence = 0.0f;         // sideways wander
    float sizeStart = 0.3f;
    float sizeEnd = 0.1f;
    float spin = 0.0f;               // radians per second, up to
    glm::vec4 colorStart = glm::vec4(1.0f, 0.8f, 0.3f, 1.0f);
    glm::vec4 colorMid = glm::vec4(1.0f, 0.5f, 0.1f, 0.6f);
    glm::vec4 colorEnd = glm::vec4(0.8f, 0.1f, 0.0f, 0.0f);
    bool additive = true;            // glows and adds up, instead of covering what is behind
    float softness = 0.5f;           // 0 is a hard disc, 1 fades from the middle
    float flicker = 0.0f;            // how much each particle's edge is eaten by noise, for flames
};

// The simulation and the data the renderer draws from. It lives in the world: a particle that
// is born stays where it was when its emitter moves.
class ParticleSystem {
public:
    struct Instance {
        glm::vec4 positionSize;   // world position, size
        glm::vec4 color;
        glm::vec2 rotationSeed;
    };

    ParticleSettings settings;
    bool playing = true;

    // Advances time. emitter is the emitter's world transform: where, and which way is up.
    void update(float deltaTime, const glm::mat4& emitter);

    // Starts count particles at once, whatever the rate.
    void burst(int count, const glm::mat4& emitter);

    void clear();
    int count() const { return int(particles.size()); }

    // The particles as the renderer draws them. Alpha-blended ones are sorted far to near.
    void gather(std::vector<Instance>& out, glm::vec3 eye) const;

private:
    struct Particle {
        glm::vec3 position;
        glm::vec3 velocity;
        float age;
        float life;
        float rotation;
        float spin;
        float seed;
    };

    std::vector<Particle> particles;
    float owed = 0.0f;   // fraction of a particle left over from the last frame
    std::mt19937 random{9001};

    float range(float low, float high);
    void spawn(const glm::mat4& emitter);
};
