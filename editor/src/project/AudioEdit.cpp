#include "AudioEdit.h"

#include <algorithm>
#include <cmath>

namespace AudioEdit {

Range clamp(const AudioClip& clip, Range range) {
    const size_t frames = clip.frames();
    range.end = std::min(range.end, frames);
    range.begin = std::min(range.begin, range.end);
    return range;
}

namespace {
    int16_t toSample(float value) {
        return (int16_t)std::lround(std::clamp(value, -32768.0f, 32767.0f));
    }
}

void trim(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    if (range.length() == 0) return;
    const size_t channels = (size_t)clip.channels;
    std::vector<int16_t> kept(clip.samples.begin() + range.begin * channels, clip.samples.begin() + range.end * channels);
    clip.samples = std::move(kept);
}

void cut(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    clip.samples.erase(clip.samples.begin() + range.begin * channels, clip.samples.begin() + range.end * channels);
}

void silence(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    std::fill(clip.samples.begin() + range.begin * channels, clip.samples.begin() + range.end * channels, (int16_t)0);
}

void gain(AudioClip& clip, Range range, float decibels) {
    range = clamp(clip, range);
    const float factor = std::pow(10.0f, decibels / 20.0f);
    const size_t channels = (size_t)clip.channels;
    for (size_t i = range.begin * channels; i < range.end * channels; i++) clip.samples[i] = toSample(clip.samples[i] * factor);
}

void fadeIn(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    for (size_t frame = range.begin; frame < range.end; frame++) {
        const float t = float(frame - range.begin) / float(std::max<size_t>(1, range.length() - 1));
        for (size_t c = 0; c < channels; c++) clip.samples[frame * channels + c] = toSample(clip.samples[frame * channels + c] * t);
    }
}

void fadeOut(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    for (size_t frame = range.begin; frame < range.end; frame++) {
        const float t = 1.0f - float(frame - range.begin) / float(std::max<size_t>(1, range.length() - 1));
        for (size_t c = 0; c < channels; c++) clip.samples[frame * channels + c] = toSample(clip.samples[frame * channels + c] * t);
    }
}

void reverse(AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    if (range.length() < 2) return;
    for (size_t a = range.begin, b = range.end - 1; a < b; a++, b--) {
        for (size_t c = 0; c < channels; c++) std::swap(clip.samples[a * channels + c], clip.samples[b * channels + c]);
    }
}

float peak(const AudioClip& clip, Range range) {
    range = clamp(clip, range);
    const size_t channels = (size_t)clip.channels;
    int loudest = 0;
    for (size_t i = range.begin * channels; i < range.end * channels; i++) loudest = std::max(loudest, std::abs((int)clip.samples[i]));
    return loudest / 32768.0f;
}

void normalize(AudioClip& clip, Range range, float level) {
    const float current = peak(clip, range);
    if (current <= 0.0f) return;
    gain(clip, range, 20.0f * std::log10(level / current));
}

void resample(AudioClip& clip, float speed) {
    if (speed <= 0.0f || clip.frames() < 2) return;
    const size_t channels = (size_t)clip.channels;
    const size_t frames = clip.frames();
    const size_t outFrames = std::max<size_t>(1, (size_t)std::floor((frames - 1) / speed) + 1);
    std::vector<int16_t> out(outFrames * channels);
    for (size_t frame = 0; frame < outFrames; frame++) {
        const float at = frame * speed;
        const size_t a = std::min((size_t)at, frames - 1);
        const size_t b = std::min(a + 1, frames - 1);
        const float t = at - (float)a;
        for (size_t c = 0; c < channels; c++) {
            out[frame * channels + c] = toSample(clip.samples[a * channels + c] * (1.0f - t) + clip.samples[b * channels + c] * t);
        }
    }
    clip.samples = std::move(out);
}

void toMono(AudioClip& clip) {
    if (clip.channels != 2) return;
    std::vector<int16_t> out(clip.frames());
    for (size_t i = 0; i < out.size(); i++) out[i] = (int16_t)((clip.samples[i * 2] + clip.samples[i * 2 + 1]) / 2);
    clip.samples = std::move(out);
    clip.channels = 1;
}

std::pair<float, float> extent(const AudioClip& clip, size_t firstFrame, size_t lastFrame) {
    const size_t channels = (size_t)clip.channels;
    lastFrame = std::min(lastFrame, clip.frames());
    int low = 0, high = 0;
    for (size_t frame = firstFrame; frame < lastFrame; frame++) {
        for (size_t c = 0; c < channels; c++) {
            const int s = clip.samples[frame * channels + c];
            low = std::min(low, s);
            high = std::max(high, s);
        }
    }
    return {low / 32768.0f, high / 32768.0f};
}

}  // namespace AudioEdit
