#pragma once
#include "AssetManager.h"

#include <filesystem>
#include <string>
#include <unordered_map>

// Small previews of texture assets. Each is generated once into Library/Thumbnails, so a
// project with thousands of textures does not decode them all again on every launch.
class AssetThumbnails {
public:
    explicit AssetThumbnails(const AssetManager& assets);
    ~AssetThumbnails();

    AssetThumbnails(const AssetThumbnails&) = delete;
    AssetThumbnails& operator=(const AssetThumbnails&) = delete;

    // The GL texture for an asset, or 0 while it is not ready. Decoding is limited to one
    // image per frame, so that opening a large folder does not freeze the UI.
    unsigned int get(const std::string& assetPath);

    void newFrame() { decodeBudget = 1; }

    // Forgets everything, for when a file on disk changed.
    void clear();

private:
    struct Entry {
        unsigned int texture = 0;
        int64_t sourceTime = 0;
    };

    const AssetManager& assets;
    std::unordered_map<std::string, Entry> entries;
    int decodeBudget = 1;

    unsigned int load(const std::string& assetPath, int64_t sourceTime);
};
