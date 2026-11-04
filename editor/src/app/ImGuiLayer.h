#pragma once
#include "imgui.h"

#include <string>

struct GLFWwindow;

// One Dear ImGui context bound to one window: the launcher and the Editor each own their own.
class ImGuiLayer {
public:
    // layoutFile holds docked panel positions; leave it empty when there is nothing to remember.
    ImGuiLayer(GLFWwindow* window, const std::string& layoutFile);
    ~ImGuiLayer();

    ImGuiLayer(const ImGuiLayer&) = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    void beginFrame();
    void endFrame();

    ImFont* uiFont() const { return ui; }
    ImFont* monoFont() const { return mono; }
    ImFont* gameFont() const { return game; }

    // The font of the game's own interface. An empty or unreadable path is the bundled Inter.
    ImFont* setGameFont(const std::string& path);

private:
    ImGuiContext* context = nullptr;
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
    ImFont* game = nullptr;
    ImFont* bundledGame = nullptr;
    std::string layoutPath;  // ImGui keeps the pointer, so the string must outlive the context
};
