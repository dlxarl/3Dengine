#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Native operating system services. Everything here works on macOS; elsewhere the functions
// report that they are unavailable, and the UI offers typed paths instead of a dead button.
namespace Platform {
    bool hasNativeDialogs();

    // Empty when the user cancels.
    std::filesystem::path chooseFolder(const std::string& title, const std::filesystem::path& startIn);
    std::vector<std::filesystem::path> chooseFiles(const std::string& title,
                                                   const std::vector<std::string>& extensions);

    void revealInFileManager(const std::filesystem::path& path);

    // Moves to the Trash, so that a mistake can be undone.
    bool moveToTrash(const std::filesystem::path& path, std::string* error = nullptr);
}
