#pragma once
#include "Project.h"
#include "RecentProjects.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

class ImGuiLayer;

// The startup window: lists known projects, creates new ones and opens existing ones.
// It knows nothing about scenes or rendering; its whole job is to name a project.
class LauncherApp {
public:
    LauncherApp();

    // Runs until the user picks a project. Empty when they close the window instead.
    std::optional<std::filesystem::path> run();

private:
    enum class Page { Projects, NewProject };

    struct ProjectDetails {
        std::string path;
        bool exists = false;
        std::optional<ProjectInfo> info;
        std::string problem;
        int sceneCount = 0;
    };

    RecentProjects recents;
    Page page = Page::Projects;

    int selectedIndex = -1;
    ProjectDetails details;
    std::string search;
    std::string banner;

    std::string newName;
    std::string newLocation;
    size_t templateIndex = 0;

    std::string typedOpenPath;

    std::optional<std::filesystem::path> chosen;

    void drawFrame();
    void drawSidebar(float width, float height);
    void drawProjectsPage();
    void drawNewProjectPage();
    void drawProjectList(float width, float height);
    void drawDetails(float width, float height);
    void drawEmptyState(float width, float height);
    void drawBanner();

    void selectRecent(int index);
    void openChooser();
    void openProject(const std::filesystem::path& root);
    void createProject();
    void beginNewProject();
    void chooseLocation();
};
