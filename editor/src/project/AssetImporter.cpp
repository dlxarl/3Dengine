#include "AssetImporter.h"
#include "Log.h"
#include "ObjLoader.h"

#include "stb_image.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    std::string lowerExtension(const fs::path& path) {
        std::string extension = path.extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return extension;
    }

    bool isInside(const fs::path& path, const fs::path& folder) {
        std::error_code ec;
        const fs::path relative = fs::weakly_canonical(path, ec).lexically_relative(fs::weakly_canonical(folder, ec));
        return !relative.empty() && *relative.begin() != "..";
    }

    bool sameFile(const fs::path& a, const fs::path& b) {
        std::error_code ec;
        return fs::weakly_canonical(a, ec) == fs::weakly_canonical(b, ec);
    }
}

AssetKind classifyAsset(const fs::path& path) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) return AssetKind::Folder;

    const std::string extension = lowerExtension(path);
    if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" ||
        extension == ".bmp" || extension == ".hdr") {
        return AssetKind::Texture;
    }
    if (extension == ".obj") return AssetKind::Model;
    if (extension == ".scene") return AssetKind::Scene;
    if (extension == ".mat" || extension == ".mtl") return AssetKind::Material;
    if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") return AssetKind::Audio;
    if (extension == ".lua" || extension == ".cpp" || extension == ".h") return AssetKind::Script;
    if (extension == ".ttf" || extension == ".otf") return AssetKind::Font;
    return AssetKind::Other;
}

bool isImportable(const fs::path& path) {
    const AssetKind kind = classifyAsset(path);
    return kind == AssetKind::Texture || kind == AssetKind::Model || kind == AssetKind::Font || kind == AssetKind::Audio;
}

fs::path uniquePath(const fs::path& folder, const std::string& fileName) {
    const fs::path name = fs::u8path(fileName);
    fs::path candidate = folder / name;

    std::error_code ec;
    for (int suffix = 1; fs::exists(candidate, ec); suffix++) {
        candidate = folder / fs::u8path(name.stem().u8string() + " (" + std::to_string(suffix) + ")" +
                                        name.extension().u8string());
    }
    return candidate;
}

ImportResult importAsset(const Project& project, const fs::path& source, const fs::path& destination) {
    ImportResult result;
    std::error_code ec;

    if (!fs::is_regular_file(source, ec)) {
        result.error = "Cannot import " + source.filename().u8string() + ": it is not a file.";
        return result;
    }
    if (isInside(source, project.getRoot())) {
        result.error = source.filename().u8string() + " is already in this project.";
        return result;
    }

    const AssetKind kind = classifyAsset(source);

    // Validated before copying, so that the project never holds a file that will not load.
    if (kind == AssetKind::Texture) {
        int width = 0, height = 0, channels = 0;
        if (!stbi_info(source.u8string().c_str(), &width, &height, &channels)) {
            result.error = "Cannot import " + source.filename().u8string() + ": " + stbi_failure_reason() + ".";
            return result;
        }
    } else if (kind == AssetKind::Model) {
        MeshData mesh;
        std::string parseError;
        if (!ObjLoader::load(source, mesh, &parseError)) {
            result.error = "Cannot import " + source.filename().u8string() + ": " + parseError;
            return result;
        }
    } else if (kind != AssetKind::Font && kind != AssetKind::Audio) {
        result.error = "Cannot import " + source.filename().u8string() +
                       ": the supported formats are PNG, JPEG, TGA, BMP and HDR textures, OBJ models, WAV sounds and TTF or OTF fonts.";
        return result;
    }

    fs::path folder = destination;
    if (sameFile(folder, project.assetsDir())) {
        folder = project.assetsDir() / (kind == AssetKind::Texture ? "Textures" : kind == AssetKind::Font ? "Fonts" : kind == AssetKind::Audio ? "Audio" : "Models");
    }
    fs::create_directories(folder, ec);

    const fs::path target = uniquePath(folder, source.filename().u8string());
    fs::copy_file(source, target, ec);
    if (ec) {
        result.error = "Cannot copy " + source.filename().u8string() + ": " + ec.message();
        return result;
    }

    // Kept with the model so that its materials are at hand once the engine reads them.
    if (kind == AssetKind::Model) {
        fs::path material = source;
        material.replace_extension(".mtl");
        if (fs::is_regular_file(material, ec)) {
            fs::path materialTarget = target;
            materialTarget.replace_extension(".mtl");
            fs::copy_file(material, materialTarget, ec);
        }
    }

    Log::info("Assets", "Imported " + source.filename().u8string());
    result.imported = target;
    return result;
}
