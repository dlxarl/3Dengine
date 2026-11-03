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

private:
    ImGuiContext* context = nullptr;
    ImFont* ui = nullptr;
    ImFont* mono = nullptr;
    std::string layoutPath;  // ImGui keeps the pointer, so the string must outlive the context
};
