#include "EditorApp.h"
#include "GlWindow.h"
#include "LauncherApp.h"
#include "Log.h"
#include "Project.h"
#include "RecentProjects.h"
#include "UserPaths.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace {
    std::optional<fs::path> projectFromArguments(int argc, char** argv) {
        for (int i = 1; i + 1 < argc; i++) {
            if (std::string(argv[i]) == "--project") {
                return UserPaths::expand(argv[i + 1]);
            }
        }
        return std::nullopt;
    }

    // Keeps the launcher's list current however the project was opened.
    void rememberProject(const Project& project) {
        RecentProjects recents(RecentProjects::defaultFile());
        recents.load();
        recents.touch(project.getRoot(), project.getInfo().name);
        recents.save();
    }
}

int main(int argc, char** argv) {
    // Windows come and go between the launcher and the Editor, but GLFW stays up throughout.
    GlfwLibrary glfw;
    if (!glfw.isReady()) return 1;

    std::optional<fs::path> next = projectFromArguments(argc, argv);

    while (true) {
        if (!next) {
            LauncherApp launcher;
            next = launcher.run();
            if (!next) return 0;
        }

        std::string error;
        std::optional<Project> project = Project::open(*next, &error);
        next.reset();

        if (!project) {
            Log::error("Project", error);
            continue;
        }

        rememberProject(*project);

        EditorApp editor(std::move(*project));
        if (editor.run() != EditorExit::CloseProject) return 0;
    }
}
