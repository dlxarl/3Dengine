#include "Audio.h"
#include "GameAudio.h"
#include "ObjectFactory.h"
#include "TestSupport.h"

#include <cmath>
#include <cstdint>
#include <fstream>

namespace fs = std::filesystem;

namespace {
    void put16(std::ofstream& file, uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); }
    void put32(std::ofstream& file, uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); }

    // A WAV file of a constant sample value, so that what the mixer does to it is easy to read.
    void writeWav(const fs::path& path, int channels, int rate, int bits, double value, int frames, bool floatFormat = false) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        const int bytes = bits / 8;
        file.write("RIFF", 4);
        put32(file, uint32_t(36 + frames * channels * bytes));
        file.write("WAVEfmt ", 8);
        put32(file, 16);
        put16(file, floatFormat ? 3 : 1);
        put16(file, uint16_t(channels));
        put32(file, uint32_t(rate));
        put32(file, uint32_t(rate * channels * bytes));
        put16(file, uint16_t(channels * bytes));
        put16(file, uint16_t(bits));
        file.write("data", 4);
        put32(file, uint32_t(frames * channels * bytes));
        for (int i = 0; i < frames * channels; i++) {
            if (floatFormat) {
                const float sample = float(value);
                file.write(reinterpret_cast<const char*>(&sample), 4);
            } else if (bits == 8) {
                const uint8_t sample = uint8_t(128 + value * 127);
                file.write(reinterpret_cast<const char*>(&sample), 1);
            } else if (bits == 16) {
                put16(file, uint16_t(int16_t(value * 32767)));
            } else {
                const int32_t sample = int32_t(value * 8388607.0);
                file.write(reinterpret_cast<const char*>(&sample), 3);
            }
        }
    }

    struct Mix {
        std::vector<float> samples;
        float left(int frame) const { return samples[size_t(frame) * 2]; }
        float right(int frame) const { return samples[size_t(frame) * 2 + 1]; }
    };

    Mix mix(AudioEngine& engine, int frames) {
        Mix result;
        result.samples.resize(size_t(frames) * 2);
        engine.render(result.samples.data(), frames);
        return result;
    }

    void testLoading(const fs::path& dir) {
        std::string error;

        writeWav(dir / "mono16.wav", 1, 22050, 16, 0.5, 100);
        auto clip = loadWav((dir / "mono16.wav").string(), &error);
        CHECK(clip && clip->channels == 1 && clip->sampleRate == 22050 && clip->frames() == 100);
        CHECK(clip && std::abs(clip->samples[0] - 16383) <= 1);

        writeWav(dir / "stereo8.wav", 2, 44100, 8, -0.5, 50);
        clip = loadWav((dir / "stereo8.wav").string(), &error);
        CHECK(clip && clip->channels == 2 && clip->frames() == 50 && clip->samples[0] < -10000);

        writeWav(dir / "mono24.wav", 1, 48000, 24, 0.25, 10);
        clip = loadWav((dir / "mono24.wav").string(), &error);
        CHECK(clip && std::abs(clip->samples[0] - 8191) <= 2);

        writeWav(dir / "mono32f.wav", 1, 48000, 32, -0.25, 10, true);
        clip = loadWav((dir / "mono32f.wav").string(), &error);
        CHECK(clip && std::abs(clip->samples[0] + 8191) <= 2);

        CHECK(!loadWav((dir / "missing.wav").string(), &error));
        CHECK(!error.empty());

        std::ofstream(dir / "text.wav") << "this is not a WAV file at all";
        CHECK(!loadWav((dir / "text.wav").string(), &error));
        CHECK(error == "not a WAV file");

        // A file cut short in the middle of its data is still played as far as it goes.
        writeWav(dir / "cut.wav", 1, 48000, 16, 0.5, 100);
        fs::resize_file(dir / "cut.wav", 44 + 60);
        clip = loadWav((dir / "cut.wav").string(), &error);
        CHECK(clip && clip->frames() == 30);
    }

    void testMixing(const fs::path& dir) {
        const std::string loud = (dir / "loud.wav").string();
        writeWav(loud, 1, AudioEngine::SAMPLE_RATE, 16, 0.5, 1000);

        {   // a one-shot plays through, once, then is gone
            AudioEngine engine;
            CHECK(!engine.isOutputOpen());
            SoundHandle sound = engine.play(loud);
            CHECK(sound != NoSound && engine.isPlaying(sound));
            Mix result = mix(engine, 600);
            CHECK(std::abs(result.left(10) - 0.5f) < 0.01f && std::abs(result.right(10) - 0.5f) < 0.01f);
            result = mix(engine, 600);
            CHECK(std::abs(result.left(10) - 0.5f) < 0.01f);
            CHECK(result.left(500) == 0.0f);   // it ended at frame 1000
            CHECK(!engine.isPlaying(sound) && engine.playingCount() == 0);
        }

        {   // a loop does not end; a missing file is not played; volume applies
            AudioEngine engine;
            SoundOptions options;
            options.loop = true;
            options.volume = 0.5f;
            SoundHandle sound = engine.play(loud, options);
            mix(engine, 5000);
            CHECK(engine.isPlaying(sound));
            const Mix result = mix(engine, 100);
            CHECK(std::abs(result.left(50) - 0.25f) < 0.01f);
            CHECK(engine.play((dir / "nothing.wav").string()) == NoSound);
            engine.setVolume(sound, 0.0f);
            mix(engine, 100);   // the change is eased in
            CHECK(std::abs(mix(engine, 10).left(5)) < 0.001f);
        }

        {   // pitch: an octave up is twice as fast, so a clip is over in half the time
            AudioEngine engine;
            SoundOptions options;
            options.pitch = 2.0f;
            SoundHandle sound = engine.play(loud, options);
            mix(engine, 520);
            CHECK(!engine.isPlaying(sound) || mix(engine, 1).left(0) == 0.0f);
            const Mix result = mix(engine, 100);
            CHECK(result.left(10) == 0.0f);
        }

        {   // placed in the world: to the right is louder in the right ear, far away is quiet or silent
            AudioEngine engine;
            engine.setListener(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
            SoundOptions options;
            options.loop = true;
            options.spatial = true;
            options.nearDistance = 2.0f;
            options.farDistance = 50.0f;

            options.position = glm::vec3(8.0f, 0.0f, 0.0f);
            SoundHandle right = engine.play(loud, options);
            Mix result = mix(engine, 100);
            CHECK(result.right(50) > result.left(50) * 3.0f);
            engine.stopSound(right);

            options.position = glm::vec3(-8.0f, 0.0f, 0.0f);
            engine.play(loud, options);
            result = mix(engine, 100);
            CHECK(result.left(50) > result.right(50) * 3.0f);
            engine.stopAll();

            options.position = glm::vec3(0.0f, 0.0f, -2.0f);
            engine.play(loud, options);
            const float near = mix(engine, 100).left(50);
            engine.stopAll();
            options.position = glm::vec3(0.0f, 0.0f, -30.0f);
            engine.play(loud, options);
            const float far = mix(engine, 100).left(50);
            engine.stopAll();
            options.position = glm::vec3(0.0f, 0.0f, -80.0f);
            engine.play(loud, options);
            const float beyond = mix(engine, 100).left(50);
            CHECK(near > far * 4.0f && far > 0.0f);
            CHECK(beyond == 0.0f);

            // Turning the listener turns the sound with it.
            engine.stopAll();
            options.position = glm::vec3(8.0f, 0.0f, 0.0f);
            engine.play(loud, options);
            engine.setListener(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));   // now facing the sound
            engine.stopAll();
            engine.play(loud, options);
            result = mix(engine, 100);
            CHECK(std::abs(result.left(50) - result.right(50)) < 0.05f);
        }

        {   // fades
            AudioEngine engine;
            SoundOptions options;
            options.loop = true;
            options.fadeIn = 0.01f;   // 480 frames
            SoundHandle sound = engine.play(loud, options);
            const Mix result = mix(engine, 960);
            CHECK(result.left(10) < 0.05f);
            CHECK(std::abs(result.left(900) - 0.5f) < 0.01f);
            engine.stopSound(sound, 0.01f);
            CHECK(!engine.isPlaying(sound));
            mix(engine, 1000);
            CHECK(engine.playingCount() == 0);
        }

        {   // many sounds together never exceed the range of the output
            AudioEngine engine;
            SoundOptions options;
            options.loop = true;
            for (int i = 0; i < 6; i++) engine.play(loud, options);
            const Mix result = mix(engine, 100);
            CHECK(result.left(50) <= 1.0f && result.left(50) > 0.9f);
        }

        {   // with no output nothing is collected, so the engine must not grow without bound
            AudioEngine engine;
            for (int i = 0; i < 500; i++) engine.play(loud);
            CHECK(engine.playingCount() <= 64);
        }
    }

    void testGameAudio(const fs::path& dir) {
        AudioEngine engine;
        GameAudio audio(engine);
        SceneGraph scene;

        audio.begin(dir / "no-such-folder", scene);
        CHECK(!audio.isActive());   // a game without sounds is simply silent

        GameSession game;
        audio.update(scene, game, 0.016f);   // and does nothing, safely
        CHECK(engine.playingCount() == 0);

        audio.begin(dir, scene);
        CHECK(audio.isActive());
        audio.end();
        CHECK(!audio.isActive());
    }

    // An Audio Source object plays its clip from the project, follows its settings, and goes quiet when muted.
    void testAudioSources(const fs::path& dir) {
        const fs::path root = dir / "game";
        writeWav(root / "Assets" / "Audio" / "beep.wav", 1, 48000, 16, 0.5, 48000);

        AudioEngine engine;
        GameAudio audio(engine);
        SceneGraph scene;
        auto* source = static_cast<AudioSourceObject*>(scene.add(ObjectFactory::create(ObjectType::AudioSource)));
        source->clip = "Assets/Audio/beep.wav";
        source->spatial = false;
        source->volume = 0.8f;
        auto* broken = static_cast<AudioSourceObject*>(scene.add(ObjectFactory::create(ObjectType::AudioSource)));
        broken->clip = "Assets/Audio/missing.wav";

        GameSession game;
        audio.begin(root / "Assets" / "Audio", scene);
        CHECK(audio.isActive());
        audio.update(scene, game, 0.016f);
        CHECK(engine.playingCount() == 1);   // the clip that exists, and not the one that does not
        const Mix playing = mix(engine, 100);
        CHECK(std::abs(playing.left(50) - 0.4f) < 0.02f);   // 0.5 of the file at volume 0.8, centred
        audio.update(scene, game, 0.016f);
        CHECK(engine.playingCount() == 1);   // it loops, and is not started twice

        source->muted = true;
        audio.update(scene, game, 0.016f);
        mix(engine, 20000);
        CHECK(engine.playingCount() == 0);

        source->muted = false;
        source->loop = false;
        audio.update(scene, game, 0.016f);
        CHECK(engine.playingCount() == 1);
        mix(engine, 60000);   // it plays through once...
        audio.update(scene, game, 0.016f);
        CHECK(engine.playingCount() == 0);   // ...and is not started again
        audio.end();
        CHECK(!audio.isActive());
    }
}

int main() {
    TempDir temp;
    testLoading(temp.path);
    testMixing(temp.path);
    testGameAudio(temp.path);
    testAudioSources(temp.path);
    return testSummary("audio_tests");
}
