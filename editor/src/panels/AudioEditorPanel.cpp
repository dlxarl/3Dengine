#include "AudioEditorPanel.h"
#include "Theme.h"
#include "Widgets.h"

#include <algorithm>
#include <cmath>
#include <system_error>

namespace fs = std::filesystem;

bool AudioEditorPanel::load(EditorContext& ctx, const fs::path& file) {
    stop();
    std::string error;
    std::shared_ptr<AudioClip> read = loadWav(file.string(), &error);
    if (!read || read->samples.empty()) {
        ctx.dialogs.error("Cannot open the sound", file.filename().u8string() + ": " + (error.empty() ? "it is empty" : error) +
                                                       ". The Audio Editor opens WAV files.");
        return false;
    }
    path = file;
    clip = *read;
    savedClip = clip;
    loaded = true;
    undoStack.clear();
    redoStack.clear();
    zoom = 1.0f;
    viewStart = 0.0f;
    selection = {};
    return true;
}

AudioEdit::Range AudioEditorPanel::target() const {
    if (selection.length() > 0) return AudioEdit::clamp(clip, selection);
    return {0, clip.frames()};
}

void AudioEditorPanel::edit(const std::function<void(AudioClip&)>& change) {
    stop();
    undoStack.push_back(clip);
    if (undoStack.size() > 20) undoStack.erase(undoStack.begin());
    redoStack.clear();
    change(clip);
    selection = AudioEdit::clamp(clip, selection);
    zoom = std::max(1.0f, zoom);
}

void AudioEditorPanel::undo() {
    if (undoStack.empty()) return;
    stop();
    redoStack.push_back(clip);
    clip = std::move(undoStack.back());
    undoStack.pop_back();
    selection = AudioEdit::clamp(clip, selection);
}

void AudioEditorPanel::redo() {
    if (redoStack.empty()) return;
    stop();
    undoStack.push_back(clip);
    clip = std::move(redoStack.back());
    redoStack.pop_back();
    selection = AudioEdit::clamp(clip, selection);
}

void AudioEditorPanel::stop() {
    if (playing != NoSound) engine.stopSound(playing);
    playing = NoSound;
}

void AudioEditorPanel::play(EditorContext& ctx, bool selectionOnly) {
    stop();
    const AudioEdit::Range range = selectionOnly ? target() : AudioEdit::Range{0, clip.frames()};
    AudioClip part = clip;
    AudioEdit::trim(part, range);

    // The engine plays files, so the part is written where it can read it.
    std::error_code ec;
    const fs::path temp = ctx.project.libraryDir() / "Temp";
    fs::create_directories(temp, ec);
    const fs::path file = temp / "audio_preview.wav";
    std::string error;
    if (!saveWav(part, file.string(), &error)) {
        ctx.setStatus(error);
        return;
    }
    // Written again under the same name, so the cached copy of the last preview must go.
    engine.stopAll();
    if (!engine.isOutputOpen()) engine.start();

    SoundOptions options;
    options.loop = loop;
    playing = engine.play(file.string(), options);
    playStarted = std::chrono::steady_clock::now();
    playFrom = range.begin;
    playTo = range.end;
}

void AudioEditorPanel::saveTo(EditorContext& ctx, const fs::path& file) {
    std::string error;
    if (!saveWav(clip, file.string(), &error)) {
        ctx.dialogs.error("Cannot save the sound", error);
        return;
    }
    ctx.assets.invalidate(ctx.assets.toAssetPath(file));
    if (file == path) savedClip = clip;
    ctx.setStatus("Saved " + file.filename().u8string());
}

void AudioEditorPanel::drawToolbar(EditorContext& ctx) {
    const bool isPlaying = playing != NoSound && engine.isPlaying(playing);

    if (ImGui::Button(isPlaying ? "Stop" : "Play")) {
        if (isPlaying) stop();
        else play(ctx, selection.length() > 0);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &loop);
    ImGui::SameLine();
    ImGui::BeginDisabled(undoStack.empty());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(redoStack.empty());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!dirty());
    if (ImGui::Button("Save")) saveTo(ctx, path);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Save as")) {
        const fs::path folder = path.parent_path();
        ctx.dialogs.promptText("Save sound as", "File name", path.stem().u8string() + " edited", "Save", [this, &ctx, folder](const std::string& name) {
            if (name.empty() || name.find_first_of("/\\:") != std::string::npos) return std::string("A file name cannot contain / \\ or :.");
            const fs::path target = folder / fs::u8path(name + ".wav");
            std::error_code ec;
            if (fs::exists(target, ec)) return std::string("A file with this name already exists.");
            saveTo(ctx, target);
            return std::string();
        });
    }
}

void AudioEditorPanel::drawWaveform(EditorContext& ctx) {
    const size_t frames = clip.frames();
    const float height = 150.0f;
    const ImVec2 size(ImGui::GetContentRegionAvail().x, height);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##wave", size, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), Theme::Sunken, 3.0f);
    draw->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

    const float visibleFrames = std::max(16.0f, (float)frames / zoom);
    viewStart = std::clamp(viewStart, 0.0f, std::max(0.0f, (float)frames - visibleFrames));
    const float framesPerPixel = visibleFrames / size.x;
    auto frameAt = [&](float x) { return (size_t)std::clamp(viewStart + (x - origin.x) * framesPerPixel, 0.0f, (float)frames); };
    auto xAt = [&](size_t frame) { return origin.x + ((float)frame - viewStart) / framesPerPixel; };

    // Zoom with the wheel around the mouse, scroll sideways with shift.
    if (hovered) {
        const float wheel = ImGui::GetIO().MouseWheel;
        const float wheelX = ImGui::GetIO().MouseWheelH;
        if (wheel != 0.0f && !ImGui::GetIO().KeyShift) {
            const float anchor = viewStart + (ImGui::GetIO().MousePos.x - origin.x) * framesPerPixel;
            zoom = std::clamp(zoom * std::pow(1.15f, wheel), 1.0f, std::max(1.0f, (float)frames / 16.0f));
            const float newVisible = std::max(16.0f, (float)frames / zoom);
            viewStart = anchor - (ImGui::GetIO().MousePos.x - origin.x) / size.x * newVisible;
        } else if (wheel != 0.0f || wheelX != 0.0f) {
            viewStart -= (ImGui::GetIO().KeyShift ? wheel : wheelX) * visibleFrames * 0.1f;
        }
    }

    // Dragging selects a stretch; a click without moving clears it.
    if (ImGui::IsItemActivated()) {
        dragging = true;
        dragFrom = frameAt(ImGui::GetIO().MousePos.x);
        selection = {};
    }
    if (dragging && ImGui::IsItemActive()) {
        const size_t now = frameAt(ImGui::GetIO().MousePos.x);
        selection = {std::min(dragFrom, now), std::max(dragFrom, now)};
        if (selection.length() < 2) selection = {};
    }
    if (ImGui::IsItemDeactivated()) dragging = false;

    if (selection.length() > 0) {
        draw->AddRectFilled(ImVec2(xAt(selection.begin), origin.y), ImVec2(xAt(selection.end), origin.y + size.y), Theme::rgb(0x6FBFAE, 50));
    }

    const float middle = origin.y + size.y * 0.5f;
    draw->AddLine(ImVec2(origin.x, middle), ImVec2(origin.x + size.x, middle), Theme::rgb(0xFFFFFF, 40));
    const int columns = (int)size.x;
    for (int column = 0; column < columns; column++) {
        const size_t first = (size_t)(viewStart + column * framesPerPixel);
        const size_t last = std::max(first + 1, (size_t)(viewStart + (column + 1) * framesPerPixel));
        if (first >= frames) break;
        const auto [low, high] = AudioEdit::extent(clip, first, last);
        const float x = origin.x + column + 0.5f;
        draw->AddLine(ImVec2(x, middle - high * size.y * 0.5f), ImVec2(x, middle - low * size.y * 0.5f - 1.0f), Theme::Accent);
    }

    // The playhead.
    if (playing != NoSound && engine.isPlaying(playing)) {
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - playStarted).count();
        const size_t length = std::max<size_t>(1, playTo - playFrom);
        const size_t frame = playFrom + (size_t)std::fmod(elapsed * clip.sampleRate, (double)length);
        const float x = xAt(frame);
        draw->AddLine(ImVec2(x, origin.y), ImVec2(x, origin.y + size.y), Theme::Text, 1.5f);
    }
    draw->PopClipRect();

    // Where the view is, for a clip zoomed in.
    ImGui::BeginDisabled(zoom <= 1.0f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##scroll", &viewStart, 0.0f, std::max(1.0f, (float)frames - visibleFrames), "");
    ImGui::EndDisabled();

    char info[160];
    const float rate = (float)clip.sampleRate;
    if (selection.length() > 0) {
        std::snprintf(info, sizeof(info), "Selection %.3f to %.3f s (%.3f s)", selection.begin / rate, selection.end / rate, selection.length() / rate);
    } else {
        std::snprintf(info, sizeof(info), "Drag on the waveform to select. Wheel zooms, Shift+wheel scrolls.");
    }
    Widgets::dimText(info);
    (void)ctx;
}

void AudioEditorPanel::drawEffects(EditorContext& ctx) {
    (void)ctx;
    const bool hasSelection = selection.length() > 0;
    const char* scope = hasSelection ? "selection" : "whole sound";

    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("Trim to selection")) {
        const AudioEdit::Range range = target();
        edit([range](AudioClip& c) { AudioEdit::trim(c, range); });
        selection = {};
    }
    ImGui::SameLine();
    if (ImGui::Button("Cut selection")) {
        const AudioEdit::Range range = target();
        edit([range](AudioClip& c) { AudioEdit::cut(c, range); });
        selection = {};
    }
    ImGui::SameLine();
    if (ImGui::Button("Silence")) {
        const AudioEdit::Range range = target();
        edit([range](AudioClip& c) { AudioEdit::silence(c, range); });
    }
    ImGui::EndDisabled();

    const AudioEdit::Range range = target();
    if (ImGui::Button("Fade in")) edit([range](AudioClip& c) { AudioEdit::fadeIn(c, range); });
    ImGui::SameLine();
    if (ImGui::Button("Fade out")) edit([range](AudioClip& c) { AudioEdit::fadeOut(c, range); });
    ImGui::SameLine();
    if (ImGui::Button("Reverse")) edit([range](AudioClip& c) { AudioEdit::reverse(c, range); });
    ImGui::SameLine();
    if (ImGui::Button("Normalize")) edit([range](AudioClip& c) { AudioEdit::normalize(c, range); });
    ImGui::SameLine();
    ImGui::BeginDisabled(clip.channels != 2);
    if (ImGui::Button("To mono")) edit([](AudioClip& c) { AudioEdit::toMono(c); });
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("##gain", &gainDecibels, -24.0f, 24.0f, "%.1f dB");
    ImGui::SameLine();
    if (ImGui::Button("Apply gain")) {
        const float decibels = gainDecibels;
        edit([range, decibels](AudioClip& c) { AudioEdit::gain(c, range, decibels); });
        gainDecibels = 0.0f;
    }
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat("##speed", &speed, 0.25f, 4.0f, "speed x%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    if (ImGui::Button("Apply speed")) {
        const float factor = speed;
        edit([factor](AudioClip& c) { AudioEdit::resample(c, factor); });
        selection = {};
        speed = 1.0f;
    }
    Widgets::dimText((std::string("Effects work on the ") + scope + ". Speed always changes the whole sound, and pitch with it.").c_str());
}

void AudioEditorPanel::draw(EditorContext& ctx, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(680.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Audio Editor", open)) {
        ImGui::End();
        return;
    }

    if (!ctx.toolRequests.audio.empty()) {
        load(ctx, ctx.toolRequests.audio);
        ctx.toolRequests.audio.clear();
    }

    if (!loaded) {
        Widgets::dimText("Double-click a .wav sound in the Project panel to open it here.");
        ImGui::End();
        return;
    }

    char title[256];
    std::snprintf(title, sizeof(title), "%s%s    %.2f s, %d Hz, %s, peak %.0f%%", path.filename().u8string().c_str(), dirty() ? " *" : "",
                  clip.seconds(), clip.sampleRate, clip.channels == 1 ? "mono" : "stereo", AudioEdit::peak(clip, {0, clip.frames()}) * 100.0f);
    ImGui::TextUnformatted(title);

    drawToolbar(ctx);
    drawWaveform(ctx);
    drawEffects(ctx);

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        ctx.textEditorFocused = true;   // Cmd+S saves the sound, not the scene
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false) && !ImGui::GetIO().WantTextInput) {
            if (playing != NoSound && engine.isPlaying(playing)) stop();
            else play(ctx, selection.length() > 0);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S) && dirty()) saveTo(ctx, path);
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z)) undo();
    }

    ImGui::End();
}
