#include "EditorSession.h"
#include "ImGuiLayer.h"
#include "Log.h"
#include "Theme.h"

#include "imgui_internal.h"

#include <GLFW/glfw3.h>

#include <system_error>

namespace fs = std::filesystem;

namespace {
    const float CAMERA_TURN_SPEED = 120.0f;  // degrees per second
    const float MOUSE_TURN_SPEED = 0.25f;    // degrees per point
    const float MOUSE_LOOK_SPEED = 0.12f;    // degrees per point, first person

    GameInput readGameInput(const EditorContext& ctx) {
        GameInput input;
        const ImGuiIO& io = ImGui::GetIO();
        if (ctx.viewMode != ViewMode::Game || io.WantTextInput) return input;

        auto axis = [](ImGuiKey positive, ImGuiKey alternative, ImGuiKey negative, ImGuiKey alternativeNegative) {
            const bool up = ImGui::IsKeyDown(positive) || ImGui::IsKeyDown(alternative);
            const bool down = ImGui::IsKeyDown(negative) || ImGui::IsKeyDown(alternativeNegative);
            return (up ? 1.0f : 0.0f) - (down ? 1.0f : 0.0f);
        };

        input.moveForward = axis(ImGuiKey_W, ImGuiKey_UpArrow, ImGuiKey_S, ImGuiKey_DownArrow);
        input.moveRight = axis(ImGuiKey_D, ImGuiKey_RightArrow, ImGuiKey_A, ImGuiKey_LeftArrow);
        input.jump = ImGui::IsKeyDown(ImGuiKey_Space);

        const float keyTurn = (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f);
        input.turnDegrees = keyTurn * CAMERA_TURN_SPEED * io.DeltaTime;
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            input.turnDegrees += io.MouseDelta.x * MOUSE_TURN_SPEED;
        }
        return input;
    }

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

void EditorSession::updateMouseCapture() {
    const bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    if (windowWasFocused && !focused) {
        ctx.wantsMouseCapture = false;
    }
    windowWasFocused = focused;

    const bool stillPlaying = !ctx.play.isStopped() && ctx.viewMode == ViewMode::Game &&
                              ctx.play.getGameStatus().firstPerson;
    if (!stillPlaying) ctx.wantsMouseCapture = false;

    if (ctx.wantsMouseCapture == mouseCaptured) return;
    mouseCaptured = ctx.wantsMouseCapture;

    glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(window, &lastCursorX, &lastCursorY);

    ImGuiIO& io = ImGui::GetIO();
    if (mouseCaptured) {
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
    } else {
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    }
}

void EditorSession::addMouseLook(GameInput& input) {
    if (!mouseCaptured) return;

    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    input.turnDegrees += (float)(x - lastCursorX) * MOUSE_LOOK_SPEED;
    input.lookUpDegrees -= (float)(y - lastCursorY) * MOUSE_LOOK_SPEED;
    lastCursorX = x;
    lastCursorY = y;
}

void EditorSession::frame() {
    imgui.beginFrame();
    thumbnails.newFrame();

    ImGuiIO& io = ImGui::GetIO();
    const float deltaTime = io.DeltaTime;

    // Space and the arrows would otherwise also press whichever button has keyboard focus.
    const bool gameHasKeyboard = !ctx.play.isStopped() && ctx.viewMode == ViewMode::Game;
    if (gameHasKeyboard) {
        io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.WantTextInput) {
            ctx.play.restart();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_V, false) && !io.WantTextInput && ctx.play.getGameStatus().hasPlayer) {
            ctx.play.toggleFirstPerson();
            ctx.wantsMouseCapture = ctx.play.getGameStatus().firstPerson;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            ctx.wantsMouseCapture = false;
        }
    } else {
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }

    updateMouseCapture();

    GameInput input = readGameInput(ctx);
    addMouseLook(input);
    ctx.play.update(deltaTime, input);
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
