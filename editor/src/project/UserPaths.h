#pragma once
#include <filesystem>
#include <string>

// Per-user locations. Nothing here depends on the working directory or the engine install.
namespace UserPaths {
    std::filesystem::path home();

    // Where the Editor keeps its own preferences, such as the recent projects list.
    // ~/Library/Application Support/Aureon on macOS; $ENGINE_USER_DATA_DIR overrides it.
    std::filesystem::path configDir();

    std::filesystem::path defaultProjectsDir();

    // Replaces a leading ~ with the home directory, for paths typed by hand.
    std::filesystem::path expand(const std::string& typedPath);

    // Shortens the home directory to ~ for display.
    std::string abbreviate(const std::filesystem::path& path);
}
