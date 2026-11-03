#pragma once
#include "MeshShape.h"
#include "Texture.h"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>

// Loads project assets by their project-relative path ("Assets/Textures/stone.png").
// Scenes only ever store those paths, so a project can move on disk without breaking.
class AssetManager {
public:
    // cacheDir receives generated data that can be rebuilt from the sources at any time.
    void setProject(const std::filesystem::path& root, const std::filesystem::path& cacheDir);
    void clear();

    const std::filesystem::path& getRoot() const { return root; }

    // Empty when the path is absolute or climbs out of the project.
    std::filesystem::path resolve(const std::string& assetPath) const;

    // The asset path of a file inside the project; empty for one outside it.
    std::string toAssetPath(const std::filesystem::path& absolutePath) const;

    std::shared_ptr<Texture> getTexture(const std::string& assetPath, const std::string& type = "texture_albedo");
    std::shared_ptr<GpuMesh> getMesh(const std::string& assetPath);

    // Drops the cached copy so that the next request reads the file again.
    void invalidate(const std::string& assetPath);

private:
    std::filesystem::path root;
    std::filesystem::path cacheDir;

    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    std::unordered_map<std::string, std::shared_ptr<GpuMesh>> meshes;

    bool loadMeshData(const std::string& assetPath, const std::filesystem::path& source, MeshData& mesh);
};
