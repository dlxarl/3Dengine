#pragma once
#include "SceneDocument.h"

#include <string>

// Play mode. The scene is snapshotted on entry and simulated in place, and Stop restores
// the snapshot, so that nothing that happens during play can reach the saved scene.
class PlayController {
public:
    enum class State { Stopped, Playing, Paused };

    explicit PlayController(SceneDocument& document);

    State getState() const { return state; }
    bool isStopped() const { return state == State::Stopped; }
    bool isPlaying() const { return state == State::Playing; }
    bool isPaused() const { return state == State::Paused; }

    void play();
    void togglePause();
    void stop();

    void update(float deltaTime);

private:
    SceneDocument& document;
    State state = State::Stopped;
    std::string snapshot;
};
