#pragma once
#include <glm/glm.hpp>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// A sound read from a WAV file (8, 16, 24 or 32 bit PCM, or 32 bit float; any rate; mono or
// stereo). Kept as 16 bit samples, which is what keeps a few minutes of ambience affordable.
struct AudioClip {
    std::vector<int16_t> samples;  // interleaved
    int channels = 1;
    int sampleRate = 44100;

    size_t frames() const { return channels > 0 ? samples.size() / channels : 0; }
    float seconds() const { return sampleRate > 0 ? float(frames()) / float(sampleRate) : 0.0f; }
};

// Returns an empty clip, and says why in error, when the file cannot be read as WAV.
std::shared_ptr<AudioClip> loadWav(const std::string& path, std::string* error = nullptr);

// Writes the clip as a 16 bit PCM WAV file. False, with the reason in error, when it cannot.
bool saveWav(const AudioClip& clip, const std::string& path, std::string* error = nullptr);

struct SoundOptions {
    float volume = 1.0f;
    float pitch = 1.0f;       // 2 is an octave up and twice as fast
    bool loop = false;
    float fadeIn = 0.0f;      // seconds
    bool spatial = false;     // placed in the world: panned and softened by distance
    glm::vec3 position = glm::vec3(0.0f);
    float nearDistance = 3.0f;   // as loud as it gets within this
    float farDistance = 60.0f;   // silent beyond this
};

using SoundHandle = uint32_t;
constexpr SoundHandle NoSound = 0;

// Mixes sounds to stereo and hands them to the system's output. Everything the game thread
// calls is cheap; the output runs on its own thread and only ever waits for the short moment
// the game thread spends changing the list of playing sounds.
//
// Without an output device (a headless machine, a platform without a backend) the engine
// still accepts every call and keeps track of what is playing. Nothing is heard, and render()
// can still be called to get the mix, which is how the tests listen.
class AudioEngine {
public:
    static constexpr int SAMPLE_RATE = 48000;

    AudioEngine();
    ~AudioEngine();
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Opens the system output. False when there is none; the engine stays usable.
    bool start();
    void stop();
    bool isOutputOpen() const { return outputOpen; }

    void setMasterVolume(float volume);
    void setListener(glm::vec3 position, glm::vec3 forward);

    // Reads the clip into the cache so that playing it later does not stall a frame.
    bool preload(const std::string& path);

    // NoSound when the file cannot be read.
    SoundHandle play(const std::string& path, const SoundOptions& options = SoundOptions());

    // For looping sounds, which keep playing until told to stop.
    void setVolume(SoundHandle sound, float volume);
    void setPosition(SoundHandle sound, glm::vec3 position);
    void stopSound(SoundHandle sound, float fadeOut = 0.0f);
    void stopAll(float fadeOut = 0.0f);
    bool isPlaying(SoundHandle sound) const;
    int playingCount() const;

    // Mixes frames of interleaved stereo into out. Called by the output thread.
    void render(float* out, int frames);

private:
    struct Voice {
        SoundHandle handle = NoSound;
        std::shared_ptr<AudioClip> clip;
        double cursor = 0.0;   // in clip frames
        double step = 1.0;     // clip frames per output frame
        bool loop = false;
        bool spatial = false;
        glm::vec3 position = glm::vec3(0.0f);
        float nearDistance = 3.0f;
        float farDistance = 60.0f;
        float volume = 1.0f;
        float fade = 1.0f;      // 0 to 1, moving towards fadeTarget
        float fadeTarget = 1.0f;
        float fadeRate = 0.0f;  // per output frame; 0 jumps
        bool stopping = false;
        float gainLeft = 0.0f;  // what the last buffer ended on, so that changes do not click
        float gainRight = 0.0f;
        bool fresh = true;
    };

    mutable std::mutex lock;
    std::vector<Voice> voices;
    std::unordered_map<std::string, std::shared_ptr<AudioClip>> clips;
    SoundHandle nextHandle = 1;
    float master = 1.0f;
    glm::vec3 listenerPosition = glm::vec3(0.0f);
    glm::vec3 listenerRight = glm::vec3(1.0f, 0.0f, 0.0f);
    bool outputOpen = false;

    struct Backend;
    std::unique_ptr<Backend> backend;

    std::shared_ptr<AudioClip> clipFor(const std::string& path);
    void gains(const Voice& voice, float& left, float& right) const;
    Voice* find(SoundHandle handle);
};
