#include "PlayController.h"
#include "Log.h"

#include <algorithm>

PlayController::PlayController(SceneDocument& sceneDocument) : document(sceneDocument) {}

void PlayController::startAudio() {
    gameAudio.begin(audioDirectory, document.getScene());
    if (gameAudio.isActive()) audio.start();
    audio.setMasterVolume(1.0f);
}

void PlayController::startScripts() {
    scripts.end();
    scriptHost.unload();
    if (scriptLibrary.empty()) return;

    std::string error;
    if (!scriptHost.load(scriptLibrary, &error)) {
        Log::error("Script", "Cannot load the scripts: " + error);
        return;
    }
    scripts.begin(document.getScene(), scriptHost);
}

void PlayController::play() {
    if (state != State::Stopped) return;

    snapshot = document.snapshot();
    document.setTrackingChanges(false);
    game.begin(document.getScene());
    startAudio();
    startScripts();
    state = State::Playing;
    Log::info("Editor", "Entered Play mode");
}

void PlayController::togglePause() {
    if (state == State::Playing) {
        state = State::Paused;
        audio.setMasterVolume(0.0f);
    } else if (state == State::Paused) {
        state = State::Playing;
        audio.setMasterVolume(1.0f);
    }
}

void PlayController::stop() {
    if (state == State::Stopped) return;

    gameAudio.end();
    audio.stop();
    scripts.end();   // before the scene is replaced under them

    std::string error;
    if (!document.restore(snapshot, &error)) {
        Log::error("Editor", "Could not restore the scene after Play mode: " + error);
    }
    snapshot.clear();
    snapshot.shrink_to_fit();
    document.setTrackingChanges(true);
    state = State::Stopped;
    Log::info("Editor", "Left Play mode; the scene is back to how it was");
}

void PlayController::restart() {
    if (state == State::Stopped) return;

    scripts.end();
    std::string error;
    if (!document.restore(snapshot, &error)) {
        Log::error("Editor", "Could not restart the scene: " + error);
        return;
    }
    game.begin(document.getScene());
    gameAudio.end();
    startAudio();
    startScripts();
    state = State::Playing;
}

void PlayController::toggleFirstPerson() {
    if (state == State::Stopped) return;
    game.setFirstPerson(document.getScene(), !game.getStatus().firstPerson);
}

void PlayController::update(float deltaTime, const GameInput& input) {
    if (state != State::Playing) return;

    // The same clamp as the engine's runtime loop, so a stall cannot tunnel objects.
    const float step = std::clamp(deltaTime, 0.0f, 0.05f);
    game.update(document.getScene(), input, step);
    scripts.update(step, input);
    document.getScene().update(step);
    gameAudio.update(document.getScene(), game, step);
}
