#include "PlayController.h"
#include "Log.h"

#include <algorithm>

PlayController::PlayController(SceneDocument& sceneDocument) : document(sceneDocument) {}

void PlayController::play() {
    if (state != State::Stopped) return;

    snapshot = document.snapshot();
    document.setTrackingChanges(false);
    state = State::Playing;
    Log::info("Editor", "Entered Play mode");
}

void PlayController::togglePause() {
    if (state == State::Playing) {
        state = State::Paused;
    } else if (state == State::Paused) {
        state = State::Playing;
    }
}

void PlayController::stop() {
    if (state == State::Stopped) return;

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

void PlayController::update(float deltaTime) {
    if (state != State::Playing) return;

    // The same clamp as the engine's runtime loop, so a stall cannot tunnel objects.
    document.getScene().update(std::clamp(deltaTime, 0.0f, 0.05f));
}
