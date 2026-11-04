#include "EditorApp.h"
#include "EditorSession.h"
#include "GlWindow.h"
#include "ImGuiLayer.h"
#include "Log.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

EditorApp::EditorApp(Project openedProject) : project(std::move(openedProject)) {}

EditorExit EditorApp::run() {
    GlWindow::Config config;
    config.width = 1600;
    config.height = 960;
    config.title = project.getInfo().name + " - Aureon Editor";
    GlWindow window(config);
    if (!window.isValid()) return EditorExit::Quit;

    glfwSetWindowSizeLimits(window.handle(), 960, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);

    // Each project remembers its own panel arrangement. It is regenerable, so it lives in Library.
    const std::string layoutFile = (project.libraryDir() / "EditorLayout.ini").u8string();
    std::error_code ec;
    const bool hasSavedLayout = fs::exists(layoutFile, ec);

    ImGuiLayer imgui(window.handle(), layoutFile);

    EditorSession session(project, window.handle(), imgui, !hasSavedLayout);
    EditorContext& ctx = session.context();

    std::string windowTitle = config.title;

    while (ctx.getExitRequest() == EditorExit::None) {
        // Sleeps until something happens, unless something on screen is moving by itself.
        const bool mouseHeld = ImGui::IsAnyMouseDown();
        if (ctx.play.isPlaying() || mouseHeld) {
            glfwPollEvents();
        } else {
            glfwWaitEventsTimeout(0.1);
        }

        if (glfwWindowShouldClose(window.handle())) {
            glfwSetWindowShouldClose(window.handle(), GLFW_FALSE);
            ctx.quit();
        }

        session.frame();
        glfwSwapBuffers(window.handle());

        const std::string title = (ctx.document.isDirty() ? "* " : "") + ctx.document.getDisplayName() + " - " +
                                  project.getInfo().name + " - Aureon Editor";
        if (title != windowTitle) {
            windowTitle = title;
            glfwSetWindowTitle(window.handle(), title.c_str());
        }
    }

    // Writes anything still pending, such as a graphics change made just before closing.
    std::string error;
    if (!project.saveSettings(&error)) {
        Log::error("Project", error);
    }

    Log::info("Editor", "Closed project " + project.getInfo().name);
    return ctx.getExitRequest();
}
