#include "ScriptHost.h"
#include "Log.h"

#if defined(_WIN32)
// Scripts need a compiler and a loader that the Editor only uses on macOS and Linux for now.
ScriptHost::~ScriptHost() = default;
bool ScriptHost::load(const std::filesystem::path&, std::string* error) {
    if (error) *error = "Scripts are not supported on this platform yet.";
    return false;
}
void ScriptHost::unload() {}
#else
#include <dlfcn.h>

ScriptHost::~ScriptHost() {
    unload();
}

bool ScriptHost::load(const std::filesystem::path& library, std::string* error) {
    unload();

    // The library registers its scripts while it loads, into a list of its own.
    scriptEntries().clear();
    handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        if (error) *error = dlerror();
        return false;
    }

    using EntriesFunction = void* (*)();
    auto entriesOf = reinterpret_cast<EntriesFunction>(dlsym(handle, "aureonScriptEntries"));
    if (!entriesOf) {
        if (error) *error = "The library does not hold any scripts.";
        unload();
        return false;
    }

    for (const ScriptEntry& entry : *static_cast<std::vector<ScriptEntry>*>(entriesOf())) {
        factories[entry.name] = entry.create;
    }
    scriptEntries().clear();
    return true;
}

void ScriptHost::unload() {
    factories.clear();
    if (handle) dlclose(handle);
    handle = nullptr;
}
#endif

std::unique_ptr<Script> ScriptHost::create(const std::string& name) const {
    auto found = factories.find(name);
    return found == factories.end() ? nullptr : found->second();
}

std::vector<std::string> ScriptHost::names() const {
    std::vector<std::string> result;
    for (const auto& entry : factories) result.push_back(entry.first);
    return result;
}

void ScriptRunner::begin(SceneGraph& scene, const ScriptHost& host) {
    end();
    for (const auto& object : scene.getObjects()) {
        if (object->script.empty()) continue;

        std::unique_ptr<Script> script = host.create(object->script);
        if (!script) {
            Log::warn("Script", object->name + " uses the script " + object->script + ", which does not exist.");
            continue;
        }
        script->self = object.get();
        script->scene = &scene;
        running.push_back(std::move(script));
    }
    for (auto& script : running) script->onStart();
}

void ScriptRunner::update(float deltaTime, const GameInput& input) {
    for (auto& script : running) script->onUpdate(deltaTime, input);
}

void ScriptRunner::end() {
    running.clear();
}
