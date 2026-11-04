#include "UserSettings.h"
#include "Log.h"
#include "UserPaths.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    fs::path file() {
        return UserPaths::configDir() / "Settings.json";
    }

    std::string shellQuote(const std::string& text) {
        std::string result = "'";
        for (char c : text) {
            if (c == '\'') result += "'\\''";
            else result += c;
        }
        return result + "'";
    }
}

UserSettings UserSettings::load() {
    UserSettings settings;
    std::ifstream in(file());
    if (!in) return settings;
    std::stringstream text;
    text << in.rdbuf();
    try {
        const nlohmann::json j = nlohmann::json::parse(text.str());
        settings.externalEditor = j.value("externalEditor", std::string());
    } catch (const nlohmann::json::exception& e) {
        Log::warn("Editor", std::string("Settings.json is damaged; using defaults. ") + e.what());
    }
    return settings;
}

bool UserSettings::save() const {
    std::error_code ec;
    fs::create_directories(UserPaths::configDir(), ec);
    std::ofstream out(file(), std::ios::binary);
    if (!out) return false;
    nlohmann::json j;
    j["version"] = 1;
    j["externalEditor"] = externalEditor;
    out << j.dump(4);
    return (bool)out;
}

const std::vector<IdePreset>& idePresets() {
    static const std::vector<IdePreset> presets = {
        {"Built-in Code Editor", ""},
        {"Visual Studio Code", "open -a \"Visual Studio Code\" --args --goto {file}:{line}"},
        {"CLion", "open -a CLion --args --line {line} {file}"},
        {"Xcode", "open -a Xcode {file}"},
        {"Sublime Text", "open -a \"Sublime Text\" {file}"},
        {"Nova", "open -a Nova {file}"},
        {"TextEdit", "open -a TextEdit {file}"},
    };
    return presets;
}

bool openInExternalEditor(const std::string& command, const fs::path& path, int line) {
    if (command.empty()) return false;

    std::string text = command;
    auto replaceAll = [&text](const std::string& key, const std::string& value) {
        for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size())) {
            text.replace(at, key.size(), value);
        }
    };
    replaceAll("{file}", shellQuote(path.string()));
    replaceAll("{line}", std::to_string(std::max(1, line)));

    // Started in the background, so that the Editor does not wait for the other program.
    const int status = std::system((text + " >/dev/null 2>&1 &").c_str());
    if (status != 0) {
        Log::warn("Editor", "Could not run: " + text);
        return false;
    }
    return true;
}

bool isTextAsset(const fs::path& path) {
    static const char* const extensions[] = {".cpp", ".h", ".hpp", ".c", ".cc", ".lua", ".txt", ".md", ".json", ".frag",
                                             ".vert", ".glsl", ".mtl", ".mat", ".cmake", ".py", ".yml", ".yaml", ".ini"};
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });
    return std::find(std::begin(extensions), std::end(extensions), extension) != std::end(extensions);
}
