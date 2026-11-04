#pragma once
#include "GameAudio.h"
#include "GameSession.h"
#include "ScriptHost.h"
#include "SceneDocument.h"

#include <filesystem>
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

    // Where the game's sounds are. Without it, or without the folder, Play is silent.
    void setAudioDirectory(const std::filesystem::path& directory) { audioDirectory = directory; }

    // The compiled scripts of the project, loaded when Play starts; empty for none.
    void setScriptLibrary(const std::filesystem::path& library) { scriptLibrary = library; }

    void play();
    void togglePause();
    void stop();

    // Starts over from the state Play was entered with, without leaving Play mode.
    void restart();

    void update(float deltaTime, const GameInput& input);

    void toggleFirstPerson();

    const GameStatus& getGameStatus() const { return game.getStatus(); }
    const GameSession& getGame() const { return game; }

private:
    // The host comes first so that the scripts are gone before their library is.
    ScriptHost scriptHost;
    ScriptRunner scripts;
    std::filesystem::path scriptLibrary;

    SceneDocument& document;
    State state = State::Stopped;
    std::string snapshot;
    GameSession game;
    AudioEngine audio;
    GameAudio gameAudio{audio};
    std::filesystem::path audioDirectory;

    void startAudio();
    void startScripts();
};
