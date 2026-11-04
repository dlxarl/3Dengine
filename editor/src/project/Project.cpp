#include "Project.h"
#include "Log.h"
#include "ObjectFactory.h"
#include "Resources.h"
#include "SceneGraph.h"
#include "SceneSerializer.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <random>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;
using json = nlohmann::json;

#ifndef ENGINE_VERSION
#define ENGINE_VERSION "0.0.0"
#endif

namespace {
    const int PROJECT_FORMAT_VERSION = 1;
    const char* const EMPTY_TEMPLATE_ID = "empty";
    const char* const DEFAULT_SCENE = "Assets/Scenes/Main.scene";

    const char* const REQUIRED_DIRS[] = {
        "Assets", "ProjectSettings",
    };

    const char* const PROJECT_DIRS[] = {
        "Assets/Models", "Assets/Textures", "Assets/Materials", "Assets/Scenes",
        "Assets/Audio", "Assets/Scripts", "ProjectSettings", "Packages", "Library",
    };

    std::string currentTimestamp() {
        const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm utc{};
#if defined(_WIN32)
        gmtime_s(&utc, &now);
#else
        gmtime_r(&now, &utc);
#endif
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
        return buffer;
    }

    std::string generateId() {
        std::random_device device;
        std::mt19937_64 random(((uint64_t)device() << 32) | device());
        char buffer[33];
        std::snprintf(buffer, sizeof(buffer), "%016llx%016llx",
                      (unsigned long long)random(), (unsigned long long)random());
        return buffer;
    }

    bool readText(const fs::path& path, std::string& text) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        std::ostringstream buffer;
        buffer << file.rdbuf();
        text = buffer.str();
        return true;
    }

    bool writeTextAtomic(const fs::path& path, const std::string& text) {
        fs::path temporary = path;
        temporary += ".tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file << text;
            if (!file.good()) return false;
        }
        std::error_code ec;
        fs::rename(temporary, path, ec);
        if (ec) {
            fs::remove(temporary, ec);
            return false;
        }
        return true;
    }

    std::optional<ProjectInfo> parseInfo(const std::string& text, std::string* error) {
        try {
            const json j = json::parse(text);
            ProjectInfo info;
            info.name = j.at("name").get<std::string>();
            info.formatVersion = j.at("formatVersion").get<int>();
            info.id = j.value("id", std::string());
            info.engineVersion = j.value("engineVersion", std::string());
            info.templateId = j.value("template", std::string());
            info.createdAt = j.value("createdAt", std::string());
            return info;
        } catch (const json::exception& e) {
            if (error) *error = e.what();
            return std::nullopt;
        }
    }

    // A project stored with the engine would be overwritten by the next update, so the Editor
    // refuses locations inside its own bundle, build tree or source checkout.
    bool isInsideEngineInstall(const fs::path& location) {
        std::error_code ec;
        const fs::path target = fs::weakly_canonical(location, ec);
        if (ec) return false;

        for (fs::path dir = Resources::executableDir(); ; dir = dir.parent_path()) {
            const bool isSourceTree = fs::exists(dir / "engine" / "include" / "Engine.h", ec);
            const bool isBundle = dir.extension() == ".app";
            if (isSourceTree || isBundle) {
                const fs::path relative = target.lexically_relative(dir);
                if (!relative.empty() && *relative.begin() != "..") return true;
            }
            if (dir == dir.parent_path()) break;
        }
        return false;
    }

    bool writeTemplateContent(const fs::path& root, const std::string& templateId, std::string* error) {
        if (templateId == EMPTY_TEMPLATE_ID) {
            SceneGraph scene;

            auto camera = ObjectFactory::create(ObjectType::Camera);
            camera->name = "Main Camera";
            camera->setPosition(glm::vec3(0.0f, 2.0f, 8.0f));
            camera->setRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
            scene.add(camera);

            auto light = ObjectFactory::create(ObjectType::Light);
            light->name = "Light";
            light->setPosition(glm::vec3(8.0f, 16.0f, 10.0f));
            static_cast<LightObject*>(light.get())->type = LightType::Directional;
            light->setRotation(glm::vec3(-50.0f, 30.0f, 0.0f));
            scene.add(light);

            return SceneSerializer::saveFile(scene, root / fs::u8path(DEFAULT_SCENE), error);
        }

        if (error) *error = "Unknown project template: " + templateId;
        return false;
    }
}

const std::vector<ProjectTemplate>& projectTemplates() {
    static const std::vector<ProjectTemplate> templates = {
        {EMPTY_TEMPLATE_ID, "Empty Project", "A scene with a camera and a light, ready for your own objects."},
    };
    return templates;
}

std::string validateProjectName(const std::string& name) {
    if (name.empty()) return "Enter a project name.";
    if (name.size() > 64) return "Use a name of 64 characters or fewer.";
    if (name.front() == ' ' || name.back() == ' ') return "Remove the spaces at the start or end of the name.";
    if (name.front() == '.') return "A project name cannot start with a dot.";
    if (name.find_first_of("/\\:*?\"<>|") != std::string::npos) {
        return "A project name cannot contain / \\ : * ? \" < > or |.";
    }
    for (unsigned char c : name) {
        if (c < 0x20 || c == 0x7F) return "A project name cannot contain control characters.";
    }
    return {};
}

std::string validateCreateOptions(const ProjectCreateOptions& options) {
    std::string nameError = validateProjectName(options.name);
    if (!nameError.empty()) return nameError;

    if (options.location.empty()) return "Choose a location for the project.";
    if (!options.location.is_absolute()) return "Enter the full path of the location.";

    std::error_code ec;
    if (fs::exists(options.location, ec) && !fs::is_directory(options.location, ec)) {
        return "The location is a file. Choose a folder.";
    }

    if (isInsideEngineInstall(options.location)) {
        return "Choose a location outside the engine. Projects stored with it could be lost when it updates.";
    }

    const fs::path target = options.location / fs::u8path(options.name);
    if (fs::exists(target, ec)) {
        if (!fs::is_directory(target, ec)) return "A file with this name already exists in the location.";
        if (!fs::is_empty(target, ec)) return "A folder with this name already exists and is not empty.";
    }
    return {};
}

ProjectValidation Project::validate(const fs::path& root) {
    ProjectValidation result;
    std::error_code ec;

    if (!fs::is_directory(root, ec)) {
        result.errors.push_back("The folder does not exist: " + root.u8string());
        return result;
    }

    std::string text;
    if (!readText(root / "Project.json", text)) {
        result.errors.push_back("This folder has no Project.json, so it is not a project.");
        return result;
    }

    std::string parseError;
    const std::optional<ProjectInfo> info = parseInfo(text, &parseError);
    if (!info) {
        result.errors.push_back("Project.json cannot be read: " + parseError);
        return result;
    }

    if (info->name.empty()) {
        result.errors.push_back("Project.json does not name the project.");
    }
    if (info->formatVersion < 1 || info->formatVersion > PROJECT_FORMAT_VERSION) {
        result.errors.push_back("This project uses format version " + std::to_string(info->formatVersion) +
                                ". This Editor supports up to version " + std::to_string(PROJECT_FORMAT_VERSION) + ".");
    }
    for (const char* dir : REQUIRED_DIRS) {
        if (!fs::is_directory(root / dir, ec)) {
            result.errors.push_back(std::string("The project is missing its ") + dir + " folder.");
        }
    }
    return result;
}

std::optional<ProjectInfo> Project::readInfo(const fs::path& root) {
    std::string text;
    if (!readText(root / "Project.json", text)) return std::nullopt;
    return parseInfo(text, nullptr);
}

std::optional<Project> Project::create(const ProjectCreateOptions& options, std::string* error) {
    const std::string invalid = validateCreateOptions(options);
    if (!invalid.empty()) {
        if (error) *error = invalid;
        return std::nullopt;
    }

    const fs::path root = (options.location / fs::u8path(options.name)).lexically_normal();

    std::error_code ec;
    const bool rootExisted = fs::exists(root, ec);

    auto fail = [&](const std::string& message) -> std::optional<Project> {
        // Leaves the disk as it was. An existing folder was verified empty above.
        if (!rootExisted) {
            std::error_code cleanup;
            fs::remove_all(root, cleanup);
        }
        if (error) *error = message;
        return std::nullopt;
    };

    fs::create_directories(root, ec);
    if (ec) return fail("Cannot create the project folder: " + ec.message());

    for (const char* dir : PROJECT_DIRS) {
        fs::create_directories(root / dir, ec);
        if (ec) return fail(std::string("Cannot create ") + dir + ": " + ec.message());
    }

    Project project;
    project.root = root;
    project.info.name = options.name;
    project.info.id = generateId();
    project.info.engineVersion = ENGINE_VERSION;
    project.info.templateId = options.templateId;
    project.info.createdAt = currentTimestamp();
    project.info.formatVersion = PROJECT_FORMAT_VERSION;
    project.startupScene = DEFAULT_SCENE;

    json metadata;
    metadata["formatVersion"] = project.info.formatVersion;
    metadata["name"] = project.info.name;
    metadata["id"] = project.info.id;
    metadata["engineVersion"] = project.info.engineVersion;
    metadata["template"] = project.info.templateId;
    metadata["createdAt"] = project.info.createdAt;
    if (!writeTextAtomic(root / "Project.json", metadata.dump(4))) {
        return fail("Cannot write Project.json.");
    }

    if (!writeTextAtomic(root / ".gitignore", "Library/\n")) {
        return fail("Cannot write .gitignore.");
    }

    std::string contentError;
    if (!writeTemplateContent(root, options.templateId, &contentError)) {
        return fail(contentError);
    }

    std::string settingsError;
    if (!project.saveSettings(&settingsError)) {
        return fail(settingsError);
    }

    Log::info("Project", "Created project '" + options.name + "' at " + root.u8string());
    return project;
}

std::optional<Project> Project::open(const fs::path& requestedRoot, std::string* error) {
    std::error_code ec;
    const fs::path canonicalRoot = fs::weakly_canonical(requestedRoot, ec);
    const fs::path root = ec ? requestedRoot : canonicalRoot;

    const ProjectValidation validation = validate(root);
    if (!validation.isValid()) {
        if (error) *error = validation.errors.front();
        return std::nullopt;
    }

    for (const char* dir : PROJECT_DIRS) {
        fs::create_directories(root / dir, ec);
        if (ec) {
            if (error) *error = std::string("Cannot create ") + dir + ": " + ec.message();
            return std::nullopt;
        }
    }

    Project project;
    project.root = root;
    project.info = *readInfo(root);
    project.loadSettings();
    return project;
}

void Project::loadSettings() {
    std::string text;

    if (readText(settingsDir() / "Project.json", text)) {
        try {
            const json j = json::parse(text);
            startupScene = j.value("startupScene", std::string());
            uiFont = j.value("uiFont", std::string());
        } catch (const json::exception& e) {
            Log::warn("Project", std::string("ProjectSettings/Project.json is damaged; using defaults. ") + e.what());
        }
    }

    if (readText(settingsDir() / "Graphics.json", text)) {
        std::string parseError;
        if (!RenderSettings::fromJson(text, graphics, &parseError)) {
            Log::warn("Project", "ProjectSettings/Graphics.json is damaged; using defaults. " + parseError);
        }
    }
}

bool Project::saveSettings(std::string* error) const {
    json settings;
    settings["version"] = 1;
    settings["startupScene"] = startupScene;
    if (!uiFont.empty()) settings["uiFont"] = uiFont;

    if (!writeTextAtomic(settingsDir() / "Project.json", settings.dump(4))) {
        if (error) *error = "Cannot write ProjectSettings/Project.json.";
        return false;
    }
    if (!writeTextAtomic(settingsDir() / "Graphics.json", graphics.toJson())) {
        if (error) *error = "Cannot write ProjectSettings/Graphics.json.";
        return false;
    }
    return true;
}
