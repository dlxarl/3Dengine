#pragma once
#include "Audio.h"

#include <utility>
#include <vector>

// Edits on a clip of sound. Ranges are in frames, from the first to one past the last; every
// function leaves the clip valid, whatever the range.
namespace AudioEdit {
    struct Range {
        size_t begin = 0;
        size_t end = 0;
        size_t length() const { return end > begin ? end - begin : 0; }
    };

    // The range, brought inside the clip.
    Range clamp(const AudioClip& clip, Range range);

    void trim(AudioClip& clip, Range range);                       // keeps only the range
    void cut(AudioClip& clip, Range range);                        // removes the range
    void silence(AudioClip& clip, Range range);
    void gain(AudioClip& clip, Range range, float decibels);
    void fadeIn(AudioClip& clip, Range range);
    void fadeOut(AudioClip& clip, Range range);
    void reverse(AudioClip& clip, Range range);

    // Scales so that the loudest sample of the range reaches the level, 1 being full scale.
    void normalize(AudioClip& clip, Range range, float level = 0.95f);

    // Speed 2 is an octave up and half as long. Linear interpolation.
    void resample(AudioClip& clip, float speed);
    void toMono(AudioClip& clip);

    // The loudest sample of a range, 0 to 1.
    float peak(const AudioClip& clip, Range range);

    // The lowest and highest sample, 0 to 1 each side, of a stretch of frames. For drawing.
    std::pair<float, float> extent(const AudioClip& clip, size_t firstFrame, size_t lastFrame);
}
