#include "AssetThumbnails.h"
#include "Log.h"

#include <glad/glad.h>

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {
    const int THUMBNAIL_SIZE = 128;

    uint64_t hashText(const std::string& text) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : text) {
            hash = (hash ^ c) * 1099511628211ull;
        }
        return hash;
    }

    fs::path thumbnailFile(const fs::path& root, const std::string& assetPath, int64_t sourceTime) {
        char name[64];
        std::snprintf(name, sizeof(name), "%016llx-%llx.png", (unsigned long long)hashText(assetPath),
                      (unsigned long long)sourceTime);
        return root / "Library" / "Thumbnails" / name;
    }

    // Averages source pixels into each destination pixel: the previews come from images
    // that can be thousands of pixels wide, where sampling single pixels would shimmer.
    std::vector<unsigned char> downscale(const unsigned char* source, int width, int height, int& outWidth, int& outHeight) {
        const float ratio = std::min(1.0f, (float)THUMBNAIL_SIZE / (float)std::max(width, height));
        outWidth = std::max(1, (int)(width * ratio));
        outHeight = std::max(1, (int)(height * ratio));

        std::vector<unsigned char> result((size_t)outWidth * outHeight * 4);
        for (int y = 0; y < outHeight; y++) {
            const int y0 = y * height / outHeight;
            const int y1 = std::max(y0 + 1, (y + 1) * height / outHeight);
            for (int x = 0; x < outWidth; x++) {
                const int x0 = x * width / outWidth;
                const int x1 = std::max(x0 + 1, (x + 1) * width / outWidth);

                unsigned sum[4] = {0, 0, 0, 0};
                for (int sy = y0; sy < y1; sy++) {
                    for (int sx = x0; sx < x1; sx++) {
                        const unsigned char* pixel = source + ((size_t)sy * width + sx) * 4;
                        for (int c = 0; c < 4; c++) sum[c] += pixel[c];
                    }
                }

                const unsigned count = (unsigned)((x1 - x0) * (y1 - y0));
                unsigned char* out = result.data() + ((size_t)y * outWidth + x) * 4;
                for (int c = 0; c < 4; c++) out[c] = (unsigned char)(sum[c] / count);
            }
        }
        return result;
    }

    unsigned int upload(const unsigned char* pixels, int width, int height) {
        unsigned int texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }
}

AssetThumbnails::AssetThumbnails(const AssetManager& assetManager) : assets(assetManager) {}

AssetThumbnails::~AssetThumbnails() {
    clear();
}

void AssetThumbnails::clear() {
    for (auto& [path, entry] : entries) {
        if (entry.texture != 0) glDeleteTextures(1, &entry.texture);
    }
    entries.clear();
}

unsigned int AssetThumbnails::get(const std::string& assetPath) {
    auto found = entries.find(assetPath);
    if (found != entries.end()) return found->second.texture;

    if (decodeBudget <= 0) return 0;
    decodeBudget--;

    const fs::path source = assets.resolve(assetPath);
    std::error_code ec;
    const auto modified = fs::last_write_time(source, ec);
    const int64_t sourceTime = ec ? 0 : (int64_t)modified.time_since_epoch().count();

    Entry entry;
    entry.sourceTime = sourceTime;
    entry.texture = load(assetPath, sourceTime);
    entries[assetPath] = entry;
    return entry.texture;
}

unsigned int AssetThumbnails::load(const std::string& assetPath, int64_t sourceTime) {
    // The engine flips images for OpenGL; previews are shown as they are stored.
    stbi_set_flip_vertically_on_load(false);

    const fs::path cached = thumbnailFile(assets.getRoot(), assetPath, sourceTime);

    int width = 0, height = 0, channels = 0;
    if (unsigned char* pixels = stbi_load(cached.u8string().c_str(), &width, &height, &channels, 4)) {
        const unsigned int texture = upload(pixels, width, height);
        stbi_image_free(pixels);
        return texture;
    }

    const fs::path source = assets.resolve(assetPath);
    unsigned char* pixels = stbi_load(source.u8string().c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        Log::warn("Assets", "No preview for " + assetPath + ": " + stbi_failure_reason());
        return 0;
    }

    int thumbWidth = 0, thumbHeight = 0;
    const std::vector<unsigned char> thumbnail = downscale(pixels, width, height, thumbWidth, thumbHeight);
    stbi_image_free(pixels);

    std::error_code ec;
    fs::create_directories(cached.parent_path(), ec);
    stbi_write_png(cached.u8string().c_str(), thumbWidth, thumbHeight, 4, thumbnail.data(), thumbWidth * 4);

    return upload(thumbnail.data(), thumbWidth, thumbHeight);
}
