#pragma once
#include "Script.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

// A compiled script library, loaded into the running program.
class ScriptHost {
public:
    ScriptHost() = default;
    ~ScriptHost();

    ScriptHost(const ScriptHost&) = delete;
    ScriptHost& operator=(const ScriptHost&) = delete;

    // Every script made from the library must be gone before it is unloaded.
    bool load(const std::filesystem::path& library, std::string* error = nullptr);
    void unload();

    bool isLoaded() const { return handle != nullptr; }
    std::unique_ptr<Script> create(const std::string& name) const;
    std::vector<std::string> names() const;

private:
    void* handle = nullptr;
    std::map<std::string, std::unique_ptr<Script> (*)()> factories;
};

// Runs the scripts of a scene's objects while it plays.
class ScriptRunner {
public:
    ~ScriptRunner() { end(); }

    // Makes a script for every object that names one, and starts them in hierarchy order.
    void begin(SceneGraph& scene, const ScriptHost& host);
    void update(float deltaTime, const GameInput& input);

    // Destroys the scripts. Call this before the scene or the library goes away.
    void end();

    int count() const { return (int)running.size(); }

private:
    std::vector<std::unique_ptr<Script>> running;
};
