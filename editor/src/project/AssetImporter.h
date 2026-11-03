#pragma once
#include "Project.h"

#include <filesystem>
#include <string>

enum class AssetKind { Folder, Texture, Model, Scene, Material, Audio, Script, Other };

AssetKind classifyAsset(const std::filesystem::path& path);

// Whether a file is something the engine can actually load after importing it.
bool isImportable(const std::filesystem::path& path);

struct ImportResult {
    std::filesystem::path imported;  // the copy inside the project; empty on failure
    std::string error;

    bool ok() const { return error.empty(); }
};

// Brings an outside file into the project. The source is copied, never moved.
//
// destination is the folder being browsed. When that is the Assets root, the file is
// sorted into Assets/Textures or Assets/Models instead.
ImportResult importAsset(const Project& project, const std::filesystem::path& source,
                         const std::filesystem::path& destination);

// A name in folder that is not taken: "stone.png", then "stone (1).png" and so on.
std::filesystem::path uniquePath(const std::filesystem::path& folder, const std::string& fileName);
