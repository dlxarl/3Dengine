#include "EditorSession.h"
#include "ImGuiLayer.h"
#include "Log.h"
#include "Theme.h"

#include "imgui_internal.h"

#include <GLFW/glfw3.h>

#include <system_error>

namespace fs = std::filesystem;

namespace {
    void onFilesDropped(GLFWwindow* window, int count, const char** paths) {
        auto* ctx = static_cast<EditorContext*>(glfwGetWindowUserPointer(window));
        for (int i = 0; i < count; i++) {
            ctx->droppedFiles.push_back(fs::u8path(paths[i]));
        }
    }

    void buildDefaultLayout(ImGuiID dockspace, ImVec2 size) {
        ImGui::DockBuilderRemoveNode(dockspace);
        ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace, size);

        ImGuiID center = dockspace;
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
        ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.30f, nullptr, &center);

        ImGui::DockBuilderDockWindow("###Viewport", center);
        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Project", bottom);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Render Settings", right);
        ImGui::DockBuilderFinish(dockspace);
    }

    void openInitialScene(EditorContext& ctx) {
        const std::string startup = ctx.project.startupScene;

        std::error_code ec;
        if (!startup.empty() && fs::exists(ctx.assets.resolve(startup), ec)) {
            std::string error;
            if (ctx.document.open(startup, &error)) return;
            Log::error("Scene", "Cannot open " + startup + ": " + error);
        } else if (!startup.empty()) {
            Log::warn("Scene", "The startup scene is missing: " + startup);
        }

        ctx.document.createNew();
    }
}

EditorSession::EditorSession(Project& project, GLFWwindow* sessionWindow, ImGuiLayer& imguiLayer, bool useDefaultLayout)
    : window(sessionWindow),
      imgui(imguiLayer),
      ctx(project),
      thumbnails(ctx.assets),
      projectPanel(ctx),
      layoutPending(useDefaultLayout) {
    ctx.monoFont = imgui.monoFont();

    glfwSetWindowUserPointer(window, &ctx);
    glfwSetDropCallback(window, onFilesDropped);

    Log::info("Editor", "Opened project " + project.getInfo().name + " at " + project.getRoot().u8string());
    openInitialScene(ctx);
}

EditorSession::~EditorSession() {
    glfwSetDropCallback(window, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
}

void EditorSession::frame() {
    imgui.beginFrame();
    thumbnails.newFrame();

    const float deltaTime = ImGui::GetIO().DeltaTime;
    ctx.play.update(deltaTime);
    ctx.updateAutosave(deltaTime);

    bars.draw(ctx, panels);

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    const ImGuiID dockspace = ImGui::GetID("##dockspace");

    if (bars.takeLayoutReset()) layoutPending = true;
    if (layoutPending) {
        buildDefaultLayout(dockspace, mainViewport->WorkSize);
        layoutPending = false;
    }

    // Each tab has its own close button; the node-wide one would only repeat it.
    ImGui::DockSpaceOverViewport(dockspace, mainViewport, ImGuiDockNodeFlags_NoCloseButton);

    if (panels.hierarchy) hierarchy.draw(ctx, &panels.hierarchy);
    if (panels.inspector) inspector.draw(ctx, thumbnails, &panels.inspector);
    if (panels.renderSettings) renderSettings.draw(ctx, &panels.renderSettings);
    projectPanel.importDroppedFiles(ctx);
    if (panels.project) projectPanel.draw(ctx, thumbnails, &panels.project);
    if (panels.console) consolePanel.draw(ctx, &panels.console);
    viewport.draw(ctx);

    ctx.dialogs.draw();

    viewport.render(ctx);

    imgui.endFrame();
}
