#pragma once
#include "AssetThumbnails.h"
#include "ConsolePanel.h"
#include "EditorContext.h"
#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "MainBars.h"
#include "ProjectPanel.h"
#include "RenderSettingsPanel.h"
#include "ViewportPanel.h"

class ImGuiLayer;
struct GLFWwindow;

// Everything that is on screen for one open project. It draws a frame when asked and knows
// nothing about the event loop, so that tests can run the real UI with scripted input.
class EditorSession {
public:
    // The window and imgui must outlive the session.
    EditorSession(Project& project, GLFWwindow* window, ImGuiLayer& imgui, bool useDefaultLayout);
    ~EditorSession();

    void frame();

    EditorContext& context() { return ctx; }

private:
    GLFWwindow* window;
    ImGuiLayer& imgui;

    EditorContext ctx;
    AssetThumbnails thumbnails;
    HierarchyPanel hierarchy;
    InspectorPanel inspector;
    ProjectPanel projectPanel;
    ConsolePanel consolePanel;
    RenderSettingsPanel renderSettings;
    ViewportPanel viewport;
    MainBars bars;
    PanelVisibility panels;

    bool layoutPending;

    bool mouseCaptured = false;
    bool windowWasFocused = false;
    double lastCursorX = 0.0;
    double lastCursorY = 0.0;

    void updateMouseCapture();
    void addMouseLook(GameInput& input);
};
