#pragma once
#include <filesystem>
#include <string>
#include <vector>

// Preferences of the person using the Editor, kept with the recent projects list.
struct UserSettings {
    // How a source file opens in another program: a command with {file} for the path and
    // {line} for a line number. Empty uses the Editor's own Code Editor.
    std::string externalEditor;

    static UserSettings load();
    bool save() const;
};

struct IdePreset {
    const char* name;
    const char* command;
};

// Well-known editors, in the order they are offered.
const std::vector<IdePreset>& idePresets();

// Runs the command for a file, without waiting for it. False when it could not be started.
bool openInExternalEditor(const std::string& command, const std::filesystem::path& file, int line = 1);

// Source and plain text files the Code Editor can show.
bool isTextAsset(const std::filesystem::path& path);
