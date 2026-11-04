#pragma once
#include "Particles.h"
#include "SceneObject.h"

// Things that are not geometry but make the world look alive. They are objects in the scene
// like any other: placed, named, parented, saved, and edited in the Inspector.

// A stream of particles; every setting is the user's.
class ParticleEmitter : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Particles; }

    ParticleSystem system;

    void update(float deltaTime) { system.update(deltaTime, model); }
};

// A fire: flames, smoke and sparks, and a flickering light that falls on what is near. It is
// described by a few numbers, and its particles follow from them.
class FireObject : public SceneObject {
public:
    FireObject();
    ObjectType getType() const override { return ObjectType::Fire; }

    float size = 1.0f;         // roughly the height of the flames in units
    float intensity = 1.0f;    // how many flames there are
    float smoke = 0.6f;        // 0 is none
    float sparks = 0.5f;       // 0 is none
    float lightRange = 14.0f;  // 0 lights nothing
    glm::vec3 lightColor = glm::vec3(1.0f, 0.55f, 0.2f);
    float flicker = 0.35f;     // how unsteadily the light burns
    bool burning = true;
    float damage = 8.0f;       // hurt per second to anyone standing in the flames; 0 is harmless

    ParticleSystem flames, smokeCloud, sparkShower;

    // Rebuilds the three streams from size, intensity, smoke and sparks. Called by update, so
    // a change is seen at once; particles already alive are left to finish.
    void configure();
    void update(float deltaTime);

    // How bright the light is right now, around 1, wavering with time.
    float lightLevel(float time) const;

private:
    float lastSize = -1, lastIntensity = -1, lastSmoke = -1, lastSparks = -1;
};

// A sound placed in the world: it plays a WAV file from where the object stands, and the player
// hears it louder, quieter and to one side or the other as they move around it. During Play and
// in a built game it starts with the scene.
class AudioSourceObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::AudioSource; }

    std::string clip;               // asset path of a WAV file, e.g. Assets/Audio/outdoor/river.wav
    float volume = 1.0f;            // 0 is silent, 1 the file as it is
    float pitch = 1.0f;             // 2 is an octave up and twice as fast
    float pitchVariation = 0.0f;    // each repeat is this much higher or lower, at random (0 to 0.5)
    bool loop = true;
    bool spatial = true;            // heard from this place; otherwise it plays everywhere equally
    float nearDistance = 3.0f;      // as loud as it gets within this
    float farDistance = 40.0f;      // silent beyond this
    float fadeIn = 0.0f;            // seconds to reach its volume when it starts
    float startDelay = 0.0f;        // seconds after the scene starts
    float repeatMin = 0.0f;         // for a sound that does not loop: seconds between plays, at random
    float repeatMax = 0.0f;         // between the two; 0 for both plays it once
    bool muted = false;
};

// A body of water: a surface that rolls with waves, reflects the sky, and is something the
// player can swim and dive in. It is as large as size says, centered on the object, and its
// height is the object's height.
class WaterObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Water; }

    glm::vec2 size = glm::vec2(20.0f);   // along x and z
    bool round = false;                  // an ellipse inside that rectangle, for a pond
    float waveHeight = 0.12f;
    float waveLength = 5.0f;
    float waveSpeed = 1.0f;
    glm::vec2 flow = glm::vec2(0.0f);    // a current, in units per second; ripples drift with it
    glm::vec3 shallowColor = glm::vec3(0.18f, 0.45f, 0.5f);
    glm::vec3 deepColor = glm::vec3(0.03f, 0.14f, 0.2f);
    float opacity = 0.82f;

    float getLevel() const { return getWorldPosition().y; }

    // Whether a point over the surface is inside the water's outline.
    bool contains(glm::vec2 point) const;
};
