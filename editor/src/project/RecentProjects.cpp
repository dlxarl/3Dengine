#include "RecentProjects.h"
#include "Log.h"
#include "UserPaths.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
    const size_t MAX_ENTRIES = 30;
}

RecentProjects::RecentProjects(fs::path recentFile) : file(std::move(recentFile)) {}

fs::path RecentProjects::defaultFile() {
    return UserPaths::configDir() / "RecentProjects.json";
}

void RecentProjects::load() {
    entries.clear();

    std::ifstream stream(file, std::ios::binary);
    if (!stream.is_open()) return;

    try {
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        const json j = json::parse(buffer.str());

        lastLocation = j.value("lastLocation", std::string());
        for (const json& entry : j.value("projects", json::array())) {
            RecentProject project;
            project.path = entry.at("path").get<std::string>();
            project.name = entry.value("name", std::string());
            project.lastOpened = entry.value("lastOpened", int64_t(0));
            entries.push_back(std::move(project));
        }
    } catch (const json::exception& e) {
        Log::warn("Project", std::string("The recent projects list is damaged and was reset. ") + e.what());
        entries.clear();
    }
}

bool RecentProjects::save() const {
    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);

    json projects = json::array();
    for (const RecentProject& project : entries) {
        projects.push_back({{"path", project.path}, {"name", project.name}, {"lastOpened", project.lastOpened}});
    }

    json j;
    j["lastLocation"] = lastLocation;
    j["projects"] = std::move(projects);

    fs::path temporary = file;
    temporary += ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream << j.dump(4);
        if (!stream.good()) return false;
    }
    fs::rename(temporary, file, ec);
    return !ec;
}

void RecentProjects::touch(const fs::path& root, const std::string& name) {
    const std::string path = root.u8string();
    remove(path);

    RecentProject project;
    project.path = path;
    project.name = name;
    project.lastOpened = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entries.insert(entries.begin(), std::move(project));

    if (entries.size() > MAX_ENTRIES) {
        entries.resize(MAX_ENTRIES);
    }
}

void RecentProjects::remove(const std::string& path) {
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const RecentProject& p) { return p.path == path; }),
                  entries.end());
}
