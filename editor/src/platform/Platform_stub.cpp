#include "Platform.h"

#include <system_error>

namespace fs = std::filesystem;

bool Platform::hasNativeDialogs() {
    return false;
}

fs::path Platform::chooseFolder(const std::string&, const fs::path&) {
    return {};
}

std::vector<fs::path> Platform::chooseFiles(const std::string&, const std::vector<std::string>&) {
    return {};
}

void Platform::revealInFileManager(const fs::path&) {}

bool Platform::moveToTrash(const fs::path& path, std::string* error) {
    std::error_code ec;
    fs::remove_all(path, ec);
    if (ec && error) *error = ec.message();
    return !ec;
}

void Platform::setMenuBar(const std::string&, const std::vector<MenuItem>&) {}

std::vector<int> Platform::takeMenuCommands() {
    return {};
}
