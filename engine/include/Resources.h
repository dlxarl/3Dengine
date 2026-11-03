#pragma once
#include <filesystem>
#include <string>

// Locates the engine's built-in resources (shaders, fonts). They ship with the executable
// and are never looked up inside a user project or relative to the working directory.
namespace Resources {
    std::filesystem::path executableDir();

    // <bundle>/Contents/Resources, <exe dir>/resources, or $ENGINE_RESOURCE_DIR if set.
    const std::filesystem::path& root();

    std::string path(const std::string& relative);
    std::string shader(const std::string& fileName);
}
