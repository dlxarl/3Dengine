#pragma once
#include "Audio.h"
#include "GameSession.h"

#include <filesystem>
#include <random>
#include <string>
#include <vector>

// The sound of a game: turns what a GameSession reports, and what the scene is made of, into
// calls on an AudioEngine.
//
// Sounds are found by file name inside the project's Audio folder, so a game with none of the
// files, or no Audio folder, is simply silent:
//
//   player/    <surface>-step1/2.wav for grass, stone, wood, sand and snow, and pick-sword-inventory
//   weapon/    sword1/2, metal-hit1/2, arrow
//   animals/   attacking-wolf, attacking-wolf2/3, crow, crow2/3, deer, deer-snort, sheep, pigeons
//   horse/     horse-snort, horse-snort2, horse-galloping
//   outdoor/   river, lake, fire
//   town/      town-ambience, crowd, ambience-music, church-bell
//   sfx/       string-and-stomp, string-and-stomp2
//
// What is heard where follows from names in the scene: objects named River..., Lake... or ending
// in Fire carry their sound with them, houses, cottages, stalls and wells make a town, a Chapel
// holds the bell, and a Minstrel, Bard or Musician plays the music, for as long as they live.
// The countryside has no bed of noise of its own: only what is really there is heard.
class GameAudio {
public:
    explicit GameAudio(AudioEngine& engine);

    // audioDirectory is the project's Assets/Audio. Without it only the scene's Audio Sources are
    // heard (their clips are asset paths from the project folder above Assets).
    void begin(const std::filesystem::path& audioDirectory, const SceneGraph& scene);
    void update(const SceneGraph& scene, const GameSession& game, float deltaTime);
    void end();

    bool isActive() const { return active; }

private:
    // A sound that plays for as long as it should, fading to its level.
    struct Layer {
        std::string file;
        SoundHandle handle = NoSound;
        float level = 0.0f;
    };

    // A sound that comes from the nearest of a set of places.
    struct Emitter {
        std::string file;
        std::vector<glm::vec3> places;
        float nearDistance = 3.0f;
        float farDistance = 40.0f;
        float dayVolume = 1.0f;
        float nightVolume = 1.0f;
        SoundHandle handle = NoSound;
        glm::vec3 nearest = glm::vec3(0.0f);
        float distance = 1e9f;
    };

    AudioEngine& engine;
    std::filesystem::path directory;
    bool active = false;

    Layer hooves;
    Emitter river, lake, fire, townNoise, crowd, music;
    std::vector<glm::vec3> town;
    std::vector<glm::vec3> chapels;   // where the bell is
    std::vector<ObjectId> musicians;

    std::vector<ObjectId> riverBodies;   // Water objects that carry a river's sound
    std::vector<ObjectId> lakeBodies;
    std::vector<ObjectId> sheep;
    std::vector<ObjectId> horses;

    // An Audio Source object that is playing, or waiting to.
    struct SourceState {
        ObjectId id = InvalidObjectId;
        SoundHandle handle = NoSound;
        float timer = 0.0f;      // until it starts, or until its next repeat
        bool started = false;
        bool done = false;       // a sound that plays once has played
        std::string failed;      // a clip that could not be read, so that it is not tried every frame
    };
    std::vector<SourceState> sources;
    std::filesystem::path projectRoot;

    std::mt19937 random{4242};
    float scanTimer = 0.0f;
    float townness = 0.0f;
    float nightness = 0.0f;
    int lastBellHour = -1;
    float bleatTimer = 14.0f;
    float snortTimer = 18.0f;
    float pigeonTimer = 12.0f;
    int lastStep = 0;

    std::string path(const std::string& relative) const;
    float range(float low, float high);
    int pick(int count);
    void rescan(const SceneGraph& scene, const GameSession& game, glm::vec3 listener);
    void drive(Layer& layer, float target, float deltaTime, float fadeSeconds = 2.0f);
    void driveEmitter(Emitter& emitter, float volume);
    void playAt(const std::string& relative, glm::vec3 position, float volume, float pitch = 1.0f, float nearDistance = 4.0f,
                float farDistance = 70.0f);
    void playHere(const std::string& relative, float volume, float pitch = 1.0f);
    void playCue(const SoundCue& cue, const GameStatus& status);
    void playCreature(const ActorSound& sound);
    void playRandomEvents(const SceneGraph& scene, const GameStatus& status, float deltaTime);
    void updateSources(const SceneGraph& scene, float deltaTime);
    std::string sourcePath(const std::string& clip) const;
    void stopLayer(Layer& layer);
    void stopEmitter(Emitter& emitter);
};
