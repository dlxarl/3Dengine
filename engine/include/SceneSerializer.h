#pragma once
#include "AssetManager.h"
#include "SceneGraph.h"

#include <filesystem>
#include <string>

// Scene files are JSON. Assets are referenced by project-relative path and objects by id, so
// a saved scene is independent of where the project lives and survives load/save unchanged.
namespace SceneSerializer {
    std::string serialize(const SceneGraph& scene);

    // Fills an empty scene. On failure the scene is left empty and error says why.
    bool deserialize(const std::string& text, SceneGraph& scene, AssetManager& assets, std::string* error = nullptr);

    bool saveFile(const SceneGraph& scene, const std::filesystem::path& path, std::string* error = nullptr);
    bool loadFile(const std::filesystem::path& path, SceneGraph& scene, AssetManager& assets, std::string* error = nullptr);

    // Copies an object and everything below it, next to the original. Returns the copy.
    SceneObject* duplicate(SceneGraph& scene, ObjectId id, AssetManager& assets);
}
