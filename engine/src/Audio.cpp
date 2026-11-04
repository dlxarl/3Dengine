#include "Audio.h"
#include "Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace {
    constexpr float PAN_CENTER_BOOST = 1.4142135f;
    constexpr size_t MAX_SILENT_VOICES = 64;

    uint16_t readU16(const unsigned char* p) { return uint16_t(p[0] | (p[1] << 8)); }
    uint32_t readU32(const unsigned char* p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

    int16_t toPcm16(float value) {
        return int16_t(std::lround(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
    }
}

bool saveWav(const AudioClip& clip, const std::string& path, std::string* error) {
    if (clip.channels < 1 || clip.channels > 2 || clip.samples.empty()) {
        if (error) *error = "There is no sound to save.";
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        if (error) *error = "Cannot write " + path;
        return false;
    }

    auto put32 = [&out](uint32_t v) { out.write(reinterpret_cast<const char*>(&v), 4); };
    auto put16 = [&out](uint16_t v) { out.write(reinterpret_cast<const char*>(&v), 2); };
    const uint32_t dataBytes = (uint32_t)(clip.samples.size() * 2);

    out.write("RIFF", 4);
    put32(36 + dataBytes);
    out.write("WAVEfmt ", 8);
    put32(16);
    put16(1);                                      // PCM
    put16((uint16_t)clip.channels);
    put32((uint32_t)clip.sampleRate);
    put32((uint32_t)(clip.sampleRate * clip.channels * 2));
    put16((uint16_t)(clip.channels * 2));
    put16(16);
    out.write("data", 4);
    put32(dataBytes);
    out.write(reinterpret_cast<const char*>(clip.samples.data()), dataBytes);
    if (!out) {
        if (error) *error = "Writing " + path + " failed.";
        return false;
    }
    return true;
}

std::shared_ptr<AudioClip> loadWav(const std::string& path, std::string* error) {
    auto fail = [&](const std::string& why) -> std::shared_ptr<AudioClip> {
        if (error) *error = why;
        return nullptr;
    };

    std::ifstream file(path, std::ios::binary);
    if (!file) return fail("cannot open the file");
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 || std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
        return fail("not a WAV file");
    }

    uint16_t format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const unsigned char* data = nullptr;
    size_t dataSize = 0;

    size_t at = 12;
    while (at + 8 <= bytes.size()) {
        const unsigned char* chunk = bytes.data() + at;
        size_t size = readU32(chunk + 4);
        const size_t body = at + 8;
        size = std::min(size, bytes.size() - body);  // a data chunk cut short is still playable

        if (std::memcmp(chunk, "fmt ", 4) == 0 && size >= 16) {
            format = readU16(chunk + 8);
            channels = readU16(chunk + 10);
            rate = readU32(chunk + 12);
            bits = readU16(chunk + 22);
            if (format == 0xFFFE && size >= 26) format = readU16(chunk + 32);  // the real tag of an extensible file
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data = bytes.data() + body;
            dataSize = size;
            break;
        }
        at = body + size + (size & 1);
    }

    if (!data || rate == 0) return fail("no audio data");
    if (channels < 1 || channels > 2) return fail("only mono and stereo are supported");
    const bool isFloat = format == 3;
    if (!(format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) && !(isFloat && bits == 32)) {
        return fail("unsupported sample format");
    }

    auto clip = std::make_shared<AudioClip>();
    clip->channels = channels;
    clip->sampleRate = int(rate);
    const size_t bytesPerSample = bits / 8;
    const size_t count = dataSize / bytesPerSample;
    clip->samples.resize(count - count % channels);

    for (size_t i = 0; i < clip->samples.size(); i++) {
        const unsigned char* p = data + i * bytesPerSample;
        switch (bits) {
            case 8:
                clip->samples[i] = int16_t((int(p[0]) - 128) * 256);
                break;
            case 16:
                clip->samples[i] = int16_t(readU16(p));
                break;
            case 24:
                clip->samples[i] = int16_t(readU16(p + 1));  // the top two of the three bytes
                break;
            default:
                if (isFloat) {
                    float value;
                    std::memcpy(&value, p, sizeof(value));
                    clip->samples[i] = toPcm16(value);
                } else {
                    clip->samples[i] = int16_t(readU16(p + 2));
                }
                break;
        }
    }
    if (clip->samples.empty()) return fail("no audio data");
    return clip;
}

#ifdef __APPLE__
struct AudioEngine::Backend {
    AudioComponentInstance unit = nullptr;

    static OSStatus pull(void* owner, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32 frames,
                         AudioBufferList* buffers) {
        float* out = static_cast<float*>(buffers->mBuffers[0].mData);
        static_cast<AudioEngine*>(owner)->render(out, int(frames));
        return noErr;
    }

    bool open(AudioEngine* engine) {
        AudioComponentDescription description = {};
        description.componentType = kAudioUnitType_Output;
        description.componentSubType = kAudioUnitSubType_DefaultOutput;
        description.componentManufacturer = kAudioUnitManufacturer_Apple;
        AudioComponent component = AudioComponentFindNext(nullptr, &description);
        if (!component || AudioComponentInstanceNew(component, &unit) != noErr) return false;

        AudioStreamBasicDescription format = {};
        format.mSampleRate = SAMPLE_RATE;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mChannelsPerFrame = 2;
        format.mBitsPerChannel = 32;
        format.mBytesPerFrame = 8;
        format.mFramesPerPacket = 1;
        format.mBytesPerPacket = 8;

        AURenderCallbackStruct callback = {pull, engine};
        const bool ok =
            AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format)) == noErr &&
            AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback)) == noErr &&
            AudioUnitInitialize(unit) == noErr && AudioOutputUnitStart(unit) == noErr;
        if (!ok) close();
        return ok;
    }

    void close() {
        if (!unit) return;
        AudioOutputUnitStop(unit);
        AudioUnitUninitialize(unit);
        AudioComponentInstanceDispose(unit);
        unit = nullptr;
    }
};
#else
struct AudioEngine::Backend {
    bool open(AudioEngine*) { return false; }
    void close() {}
};
#endif

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start() {
    if (outputOpen) return true;
    backend = std::make_unique<Backend>();
    outputOpen = backend->open(this);
    if (!outputOpen) {
        backend.reset();
        Log::warn("Audio", "No audio output is available; the game will be silent");
    }
    return outputOpen;
}

void AudioEngine::stop() {
    if (backend) backend->close();  // after this the output thread no longer calls render()
    backend.reset();
    outputOpen = false;
    stopAll();
}

void AudioEngine::setMasterVolume(float volume) {
    std::lock_guard<std::mutex> guard(lock);
    master = std::clamp(volume, 0.0f, 2.0f);
}

void AudioEngine::setListener(glm::vec3 position, glm::vec3 forward) {
    const glm::vec2 flat(forward.x, forward.z);
    const float length = glm::length(flat);
    std::lock_guard<std::mutex> guard(lock);
    listenerPosition = position;
    if (length > 1e-4f) listenerRight = glm::vec3(-flat.y / length, 0.0f, flat.x / length);
}

std::shared_ptr<AudioClip> AudioEngine::clipFor(const std::string& path) {
    auto cached = clips.find(path);
    if (cached != clips.end()) return cached->second;

    std::string error;
    std::shared_ptr<AudioClip> clip = loadWav(path, &error);
    if (!clip) Log::warn("Audio", "Cannot play " + path + ": " + error);
    clips[path] = clip;  // a file that failed is remembered, so that it is not retried every frame
    return clip;
}

bool AudioEngine::preload(const std::string& path) {
    return clipFor(path) != nullptr;
}

AudioEngine::Voice* AudioEngine::find(SoundHandle handle) {
    for (Voice& voice : voices) {
        if (voice.handle == handle) return &voice;
    }
    return nullptr;
}

SoundHandle AudioEngine::play(const std::string& path, const SoundOptions& options) {
    std::shared_ptr<AudioClip> clip = clipFor(path);
    if (!clip) return NoSound;

    Voice voice;
    voice.clip = clip;
    voice.step = double(clip->sampleRate) / SAMPLE_RATE * std::max(0.05f, options.pitch);
    voice.loop = options.loop;
    voice.spatial = options.spatial;
    voice.position = options.position;
    voice.nearDistance = std::max(0.1f, options.nearDistance);
    voice.farDistance = std::max(voice.nearDistance + 0.1f, options.farDistance);
    voice.volume = std::max(0.0f, options.volume);
    if (options.fadeIn > 0.0f) {
        voice.fade = 0.0f;
        voice.fadeRate = 1.0f / (options.fadeIn * SAMPLE_RATE);
    }

    std::lock_guard<std::mutex> guard(lock);

    // Without an output nothing plays the sounds out, so they must not pile up.
    if (!outputOpen && voices.size() >= MAX_SILENT_VOICES) {
        auto spent = std::find_if(voices.begin(), voices.end(), [](const Voice& v) { return v.stopping || !v.loop; });
        voices.erase(spent != voices.end() ? spent : voices.begin());
    }
    voice.handle = nextHandle++;
    voices.push_back(std::move(voice));
    return voices.back().handle;
}

void AudioEngine::setVolume(SoundHandle sound, float volume) {
    std::lock_guard<std::mutex> guard(lock);
    if (Voice* voice = find(sound)) voice->volume = std::max(0.0f, volume);
}

void AudioEngine::setPosition(SoundHandle sound, glm::vec3 position) {
    std::lock_guard<std::mutex> guard(lock);
    if (Voice* voice = find(sound)) voice->position = position;
}

void AudioEngine::stopSound(SoundHandle sound, float fadeOut) {
    std::lock_guard<std::mutex> guard(lock);
    if (Voice* voice = find(sound)) {
        voice->stopping = true;
        voice->fadeTarget = 0.0f;
        voice->fadeRate = fadeOut > 0.0f ? 1.0f / (fadeOut * SAMPLE_RATE) : 0.0f;
    }
}

void AudioEngine::stopAll(float fadeOut) {
    std::lock_guard<std::mutex> guard(lock);
    for (Voice& voice : voices) {
        voice.stopping = true;
        voice.fadeTarget = 0.0f;
        voice.fadeRate = fadeOut > 0.0f ? 1.0f / (fadeOut * SAMPLE_RATE) : 0.0f;
    }
    // Without an output nobody calls render() to collect them.
    if (fadeOut <= 0.0f && !outputOpen) voices.clear();
}

bool AudioEngine::isPlaying(SoundHandle sound) const {
    std::lock_guard<std::mutex> guard(lock);
    for (const Voice& voice : voices) {
        if (voice.handle == sound && !voice.stopping) return true;
    }
    return false;
}

int AudioEngine::playingCount() const {
    std::lock_guard<std::mutex> guard(lock);
    int count = 0;
    for (const Voice& voice : voices) {
        if (!voice.stopping) count++;
    }
    return count;
}

void AudioEngine::gains(const Voice& voice, float& left, float& right) const {
    float level = voice.volume * master;
    float pan = 0.0f;  // -1 left to 1 right

    if (voice.spatial) {
        const glm::vec3 offset = voice.position - listenerPosition;
        const float distance = glm::length(offset);
        if (distance >= voice.farDistance) {
            left = right = 0.0f;
            return;
        }
        if (distance > voice.nearDistance) {
            const float away = (distance - voice.nearDistance) / (voice.farDistance - voice.nearDistance);
            level *= (voice.nearDistance / distance) * (1.0f - away);
        }
        // Straight above or below, or on top of the listener, is heard in the middle.
        if (distance > 0.5f) pan = std::clamp(glm::dot(offset / distance, listenerRight), -1.0f, 1.0f);
    }

    const float angle = (pan * 0.5f + 0.5f) * 1.5707963f;
    left = level * std::min(1.0f, PAN_CENTER_BOOST * std::cos(angle));
    right = level * std::min(1.0f, PAN_CENTER_BOOST * std::sin(angle));
}

void AudioEngine::render(float* out, int frames) {
    std::fill(out, out + size_t(frames) * 2, 0.0f);

    std::lock_guard<std::mutex> guard(lock);
    for (Voice& voice : voices) {
        const AudioClip& clip = *voice.clip;
        const size_t length = clip.frames();
        const bool stereo = clip.channels == 2;
        const bool mixDown = stereo && voice.spatial;

        float targetLeft, targetRight;
        gains(voice, targetLeft, targetRight);
        if (voice.fresh) {
            voice.gainLeft = targetLeft;
            voice.gainRight = targetRight;
            voice.fresh = false;
        }
        const float stepLeft = (targetLeft - voice.gainLeft) / float(frames);
        const float stepRight = (targetRight - voice.gainRight) / float(frames);
        float gainLeft = voice.gainLeft;
        float gainRight = voice.gainRight;

        bool finished = false;
        for (int i = 0; i < frames; i++) {
            if (voice.fade != voice.fadeTarget) {
                if (voice.fadeRate <= 0.0f) {
                    voice.fade = voice.fadeTarget;
                } else if (voice.fade < voice.fadeTarget) {
                    voice.fade = std::min(voice.fadeTarget, voice.fade + voice.fadeRate);
                } else {
                    voice.fade = std::max(voice.fadeTarget, voice.fade - voice.fadeRate);
                }
            }
            if (voice.stopping && voice.fade <= 0.0f) {
                finished = true;
                break;
            }

            size_t first = size_t(voice.cursor);
            if (first >= length) {
                finished = true;
                break;
            }
            size_t second = first + 1;
            if (second >= length) second = voice.loop ? 0 : first;
            const float blend = float(voice.cursor - double(first));

            float sampleLeft, sampleRight;
            if (stereo) {
                sampleLeft = (float(clip.samples[first * 2]) * (1.0f - blend) + float(clip.samples[second * 2]) * blend) / 32768.0f;
                sampleRight = (float(clip.samples[first * 2 + 1]) * (1.0f - blend) + float(clip.samples[second * 2 + 1]) * blend) / 32768.0f;
                if (mixDown) sampleLeft = sampleRight = 0.5f * (sampleLeft + sampleRight);
            } else {
                sampleLeft = sampleRight = (float(clip.samples[first]) * (1.0f - blend) + float(clip.samples[second]) * blend) / 32768.0f;
            }

            gainLeft += stepLeft;
            gainRight += stepRight;
            out[i * 2] += sampleLeft * gainLeft * voice.fade;
            out[i * 2 + 1] += sampleRight * gainRight * voice.fade;

            voice.cursor += voice.step;
            if (voice.cursor >= double(length) && voice.loop) voice.cursor = std::fmod(voice.cursor, double(length));
        }
        voice.gainLeft = targetLeft;
        voice.gainRight = targetRight;
        if (finished) voice.handle = NoSound;
    }

    voices.erase(std::remove_if(voices.begin(), voices.end(), [](const Voice& voice) { return voice.handle == NoSound; }),
                 voices.end());

    for (int i = 0; i < frames * 2; i++) out[i] = std::clamp(out[i], -1.0f, 1.0f);
}
