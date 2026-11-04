#pragma once
#include "GameSession.h"
#include "SceneGraph.h"

#include <memory>
#include <vector>

// A script is a C++ class that behaves as one object of a scene does. The Editor compiles the
// scripts of a project when Play starts; an object runs the script named in its Script field.
//
//     class Spin : public Script {
//     public:
//         void onUpdate(float deltaTime, const GameInput& input) override { self->rotate(90.0f * deltaTime, {0, 1, 0}); }
//     };
//     AUREON_SCRIPT(Spin)
class Script {
public:
    virtual ~Script() = default;

    SceneObject* self = nullptr;    // the object this script is on
    SceneGraph* scene = nullptr;    // the scene it is in

    virtual void onStart() {}
    virtual void onUpdate(float deltaTime, const GameInput& input) {}
};

struct ScriptEntry {
    const char* name;
    std::unique_ptr<Script> (*create)();
};

inline std::vector<ScriptEntry>& scriptEntries() {
    static std::vector<ScriptEntry> entries;
    return entries;
}

struct ScriptRegistration {
    ScriptRegistration(const char* name, std::unique_ptr<Script> (*create)()) {
        scriptEntries().push_back({name, create});
    }
};

// Where a compiled script library hands over the scripts it holds.
extern "C" __attribute__((weak, used, visibility("default"))) void* aureonScriptEntries() {
    return &scriptEntries();
}

#define AUREON_SCRIPT(ClassName) \
    static ScriptRegistration aureonRegister##ClassName(#ClassName, []() -> std::unique_ptr<Script> { return std::make_unique<ClassName>(); });
