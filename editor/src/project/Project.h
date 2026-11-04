#pragma once
#include "RenderSettings.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct ProjectInfo {
    std::string name;
    std::string id;
    std::string engineVersion;
    std::string templateId;
    std::string createdAt;
    int formatVersion = 0;
};

struct ProjectValidation {
    std::vector<std::string> errors;

    bool isValid() const { return errors.empty(); }
};

struct ProjectTemplate {
    const char* id;
    const char* name;
    const char* description;
};

// The templates offered when creating a project.
const std::vector<ProjectTemplate>& projectTemplates();

struct ProjectCreateOptions {
    std::string name;
    std::filesystem::path location;  // the project becomes <location>/<name>
    std::string templateId = "empty";
};

// Why a name cannot be a project folder; empty when it can.
std::string validateProjectName(const std::string& name);

// Why a project cannot be created with these options; empty when it can.
std::string validateCreateOptions(const ProjectCreateOptions& options);

// A project on disk. It lives wherever the user put it, independent of the engine install:
//
//   MyGame/
//     Project.json            identity: name, id, engine version
//     Assets/                 the user's source files
//     ProjectSettings/        Project.json, Graphics.json
//     Packages/
//     Library/                generated data; safe to delete, never checked in
class Project {
public:
    // Reads a project folder without changing anything.
    static ProjectValidation validate(const std::filesystem::path& root);
    static std::optional<ProjectInfo> readInfo(const std::filesystem::path& root);

    static std::optional<Project> create(const ProjectCreateOptions& options, std::string* error = nullptr);

    // Recreates regenerable folders that are missing, such as Library.
    static std::optional<Project> open(const std::filesystem::path& root, std::string* error = nullptr);

    const std::filesystem::path& getRoot() const { return root; }
    const ProjectInfo& getInfo() const { return info; }

    std::filesystem::path assetsDir() const { return root / "Assets"; }
    std::filesystem::path libraryDir() const { return root / "Library"; }
    std::filesystem::path settingsDir() const { return root / "ProjectSettings"; }

    // Project-relative asset path of the scene to open first; may be empty.
    std::string startupScene;

    // Project-relative asset path of the font the game UI uses; empty is the bundled Inter.
    std::string uiFont;
    RenderSettings graphics;

    bool saveSettings(std::string* error = nullptr) const;

private:
    std::filesystem::path root;
    ProjectInfo info;

    void loadSettings();
};
