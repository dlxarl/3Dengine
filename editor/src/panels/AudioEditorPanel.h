#pragma once
#include "AudioEdit.h"
#include "EditorContext.h"

#include <chrono>
#include <functional>
#include <filesystem>
#include <string>
#include <vector>

// Looks at a WAV file as a waveform and edits it: select a stretch to play, trim, cut, fade,
// change its level or speed, and save it back or as a new file.
class AudioEditorPanel {
public:
    AudioEditorPanel() = default;

    void draw(EditorContext& ctx, bool* open);
    bool load(EditorContext& ctx, const std::filesystem::path& file);

private:
    std::filesystem::path path;
    AudioClip clip;
    AudioClip savedClip;
    bool loaded = false;

    std::vector<AudioClip> undoStack;
    std::vector<AudioClip> redoStack;

    // The view: how many frames per pixel, and the first frame on screen.
    float zoom = 1.0f;            // 1 shows the whole clip
    float viewStart = 0.0f;       // in frames
    AudioEdit::Range selection;
    bool dragging = false;
    size_t dragFrom = 0;

    float gainDecibels = 0.0f;
    float speed = 1.0f;
    bool loop = false;

    AudioEngine engine;
    SoundHandle playing = NoSound;
    std::chrono::steady_clock::time_point playStarted;
    size_t playFrom = 0;
    size_t playTo = 0;

    bool dirty() const { return loaded && clip.samples != savedClip.samples; }
    AudioEdit::Range target() const;     // the selection, or the whole clip
    void edit(const std::function<void(AudioClip&)>& change);
    void undo();
    void redo();
    void play(EditorContext& ctx, bool selectionOnly);
    void stop();
    void saveTo(EditorContext& ctx, const std::filesystem::path& file);

    void drawToolbar(EditorContext& ctx);
    void drawWaveform(EditorContext& ctx);
    void drawEffects(EditorContext& ctx);
};
