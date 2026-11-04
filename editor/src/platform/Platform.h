#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Native operating system services. Everything here works on macOS; elsewhere the functions
// report that they are unavailable, and the UI offers typed paths instead of a dead button.
namespace Platform {
    // One entry of the operating system's menu bar. An item with children is a submenu; a
    // command of 0 is inert. A key is a single character that is pressed with Cmd.
    struct MenuItem {
        std::string title;
        int command = 0;
        std::string key;
        bool shift = false;
        bool enabled = true;
        bool checked = false;
        bool separator = false;
        std::vector<MenuItem> children;
    };

    // Replaces the menu bar. Cheap to call every frame: nothing is rebuilt unless it changed.
    void setMenuBar(const std::string& appName, const std::vector<MenuItem>& menus);

    // The commands chosen from the menu bar since the last call, in order.
    std::vector<int> takeMenuCommands();

    bool hasNativeDialogs();

    // Empty when the user cancels.
    std::filesystem::path chooseFolder(const std::string& title, const std::filesystem::path& startIn);
    std::vector<std::filesystem::path> chooseFiles(const std::string& title,
                                                   const std::vector<std::string>& extensions);

    void revealInFileManager(const std::filesystem::path& path);

    // Moves to the Trash, so that a mistake can be undone.
    bool moveToTrash(const std::filesystem::path& path, std::string* error = nullptr);
}
