#pragma once
#include "GameSession.h"
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

    // Starts over from the state Play was entered with, without leaving Play mode.
    void restart();

    void update(float deltaTime, const GameInput& input);

    void toggleFirstPerson();

    const GameStatus& getGameStatus() const { return game.getStatus(); }

private:
    SceneDocument& document;
    State state = State::Stopped;
    std::string snapshot;
    GameSession game;
};
