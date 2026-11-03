#include "Resources.h"
#include "Log.h"

#include <cstdint>
#include <cstdlib>
#include <system_error>
#include <vector>

#if defined(__APPLE__)
  #include <mach-o/dyld.h>
#elif defined(_WIN32)
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

fs::path Resources::executableDir() {
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return fs::current_path();
    }
    fs::path exe(buffer.data());
#elif defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH * 4, L'\0');
    GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    fs::path exe(buffer.data());
#else
    std::error_code readError;
    fs::path exe = fs::read_symlink("/proc/self/exe", readError);
    if (readError) {
        return fs::current_path();
    }
#endif
    std::error_code ec;
    fs::path resolved = fs::weakly_canonical(exe, ec);
    return (ec ? exe : resolved).parent_path();
}

const fs::path& Resources::root() {
    static const fs::path resolved = [] {
        if (const char* overrideDir = std::getenv("ENGINE_RESOURCE_DIR")) {
            return fs::path(overrideDir);
        }

        const fs::path exeDir = executableDir();
        const fs::path candidates[] = {
            exeDir.parent_path() / "Resources",
            exeDir / "resources",
        };

        std::error_code ec;
        for (const fs::path& candidate : candidates) {
            if (fs::is_directory(candidate / "shaders", ec)) {
                return candidate;
            }
        }

        Log::error("Resources", "Built-in resources not found next to " + exeDir.string());
        return exeDir / "resources";
    }();
    return resolved;
}

std::string Resources::path(const std::string& relative) {
    return (root() / relative).string();
}

std::string Resources::shader(const std::string& fileName) {
    return (root() / "shaders" / fileName).string();
}
