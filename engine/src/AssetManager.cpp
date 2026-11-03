#include "AssetManager.h"
#include "Log.h"
#include "ObjLoader.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    const uint32_t MESH_CACHE_MAGIC = 0x4853454D;  // "MESH"
    const uint32_t MESH_CACHE_VERSION = 1;

    struct MeshCacheHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t sourceSize;
        int64_t sourceTime;
        uint64_t floatCount;
    };

    uint64_t hashString(const std::string& text) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : text) {
            hash = (hash ^ c) * 1099511628211ull;
        }
        return hash;
    }

    fs::path meshCachePath(const fs::path& cacheDir, const std::string& assetPath) {
        char name[32];
        std::snprintf(name, sizeof(name), "%016llx.mesh", (unsigned long long)hashString(assetPath));
        return cacheDir / "Meshes" / name;
    }

    bool describeSource(const fs::path& source, uint64_t& size, int64_t& time) {
        std::error_code ec;
        size = fs::file_size(source, ec);
        if (ec) return false;
        const auto modified = fs::last_write_time(source, ec);
        if (ec) return false;
        time = (int64_t)modified.time_since_epoch().count();
        return true;
    }

    bool readMeshCache(const fs::path& cacheFile, const fs::path& source, MeshData& mesh) {
        uint64_t sourceSize = 0;
        int64_t sourceTime = 0;
        if (!describeSource(source, sourceSize, sourceTime)) return false;

        std::ifstream file(cacheFile, std::ios::binary);
        if (!file.is_open()) return false;

        MeshCacheHeader header{};
        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (!file || header.magic != MESH_CACHE_MAGIC || header.version != MESH_CACHE_VERSION ||
            header.sourceSize != sourceSize || header.sourceTime != sourceTime) {
            return false;
        }

        mesh.vertices.resize(header.floatCount);
        file.read(reinterpret_cast<char*>(mesh.vertices.data()), header.floatCount * sizeof(float));
        return (bool)file;
    }

    void writeMeshCache(const fs::path& cacheFile, const fs::path& source, const MeshData& mesh) {
        uint64_t sourceSize = 0;
        int64_t sourceTime = 0;
        if (!describeSource(source, sourceSize, sourceTime)) return;

        std::error_code ec;
        fs::create_directories(cacheFile.parent_path(), ec);

        std::ofstream file(cacheFile, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return;

        const MeshCacheHeader header{MESH_CACHE_MAGIC, MESH_CACHE_VERSION, sourceSize, sourceTime,
                                     (uint64_t)mesh.vertices.size()};
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(mesh.vertices.data()), mesh.vertices.size() * sizeof(float));
    }
}

void AssetManager::setProject(const fs::path& newRoot, const fs::path& newCacheDir) {
    clear();
    root = newRoot;
    cacheDir = newCacheDir;
}

void AssetManager::clear() {
    textures.clear();
    meshes.clear();
}

fs::path AssetManager::resolve(const std::string& assetPath) const {
    const fs::path relative = fs::u8path(assetPath);
    if (assetPath.empty() || relative.is_absolute()) return {};
    for (const fs::path& part : relative) {
        if (part == "..") return {};
    }
    return (root / relative).lexically_normal();
}

std::string AssetManager::toAssetPath(const fs::path& absolutePath) const {
    std::error_code ec;
    const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
    if (ec) return {};
    const fs::path canonicalPath = fs::weakly_canonical(absolutePath, ec);
    if (ec) return {};

    const fs::path relative = canonicalPath.lexically_relative(canonicalRoot);
    if (relative.empty() || relative == "." || *relative.begin() == "..") return {};
    return relative.generic_u8string();
}

std::shared_ptr<Texture> AssetManager::getTexture(const std::string& assetPath, const std::string& type) {
    const std::string key = type + "|" + assetPath;

    auto cached = textures.find(key);
    if (cached != textures.end()) return cached->second;

    const fs::path source = resolve(assetPath);
    if (source.empty()) {
        Log::error("Assets", "Invalid texture path: " + assetPath);
        return nullptr;
    }

    auto texture = std::make_shared<Texture>(source.u8string().c_str(), type);
    texture->assetPath = assetPath;
    textures[key] = texture;
    return texture;
}

std::shared_ptr<GpuMesh> AssetManager::getMesh(const std::string& assetPath) {
    auto cached = meshes.find(assetPath);
    if (cached != meshes.end()) return cached->second;

    const fs::path source = resolve(assetPath);
    if (source.empty()) {
        Log::error("Assets", "Invalid mesh path: " + assetPath);
        return nullptr;
    }

    MeshData data;
    if (!loadMeshData(assetPath, source, data)) return nullptr;

    auto mesh = std::make_shared<GpuMesh>(data);
    meshes[assetPath] = mesh;
    return mesh;
}

bool AssetManager::loadMeshData(const std::string& assetPath, const fs::path& source, MeshData& mesh) {
    const fs::path cacheFile = meshCachePath(cacheDir, assetPath);
    if (!cacheDir.empty() && readMeshCache(cacheFile, source, mesh)) {
        return true;
    }

    std::string error;
    if (!ObjLoader::load(source, mesh, &error)) {
        Log::error("Assets", "Failed to load mesh " + assetPath + ": " + error);
        return false;
    }

    if (!cacheDir.empty()) {
        writeMeshCache(cacheFile, source, mesh);
    }
    return true;
}

void AssetManager::invalidate(const std::string& assetPath) {
    const std::string suffix = "|" + assetPath;
    for (auto it = textures.begin(); it != textures.end();) {
        const std::string& key = it->first;
        const bool matches = key.size() >= suffix.size() &&
                             key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0;
        it = matches ? textures.erase(it) : std::next(it);
    }
    meshes.erase(assetPath);
}
