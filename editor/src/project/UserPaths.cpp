#include "UserPaths.h"

#include <cstdlib>

namespace fs = std::filesystem;

fs::path UserPaths::home() {
#if defined(_WIN32)
    if (const char* profile = std::getenv("USERPROFILE")) return fs::u8path(profile);
#else
    if (const char* homeDir = std::getenv("HOME")) return fs::u8path(homeDir);
#endif
    return fs::current_path();
}

fs::path UserPaths::configDir() {
    if (const char* overrideDir = std::getenv("ENGINE_USER_DATA_DIR")) {
        return fs::u8path(overrideDir);
    }
#if defined(__APPLE__)
    return home() / "Library" / "Application Support" / "3D Engine";
#elif defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA")) return fs::u8path(appData) / "3D Engine";
    return home() / "3D Engine";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) return fs::u8path(xdg) / "3d-engine";
    return home() / ".config" / "3d-engine";
#endif
}

fs::path UserPaths::defaultProjectsDir() {
    return home() / "3D Engine Projects";
}

fs::path UserPaths::expand(const std::string& typedPath) {
    if (typedPath == "~") return home();
    if (typedPath.rfind("~/", 0) == 0) return home() / fs::u8path(typedPath.substr(2));
    return fs::u8path(typedPath);
}

std::string UserPaths::abbreviate(const fs::path& path) {
    const std::string full = path.u8string();
    const std::string homePrefix = home().u8string();
    if (!homePrefix.empty() && full.rfind(homePrefix, 0) == 0 &&
        (full.size() == homePrefix.size() || full[homePrefix.size()] == '/')) {
        return "~" + full.substr(homePrefix.size());
    }
    return full;
}
