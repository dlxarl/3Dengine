#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct RecentProject {
    std::string path;
    std::string name;
    int64_t lastOpened = 0;  // seconds since the epoch
};

// The launcher's list. Stored per user, because it describes this machine, not any project.
class RecentProjects {
public:
    explicit RecentProjects(std::filesystem::path file);

    static std::filesystem::path defaultFile();

    void load();
    bool save() const;

    // Moves the project to the top of the list, adding it if needed.
    void touch(const std::filesystem::path& root, const std::string& name);
    void remove(const std::string& path);

    const std::vector<RecentProject>& getEntries() const { return entries; }

    const std::string& getLastLocation() const { return lastLocation; }
    void setLastLocation(const std::string& location) { lastLocation = location; }

private:
    std::filesystem::path file;
    std::vector<RecentProject> entries;
    std::string lastLocation;
};
