#include "ImGuiLayer.h"
#include "Log.h"
#include "Resources.h"
#include "Theme.h"

#include "ImGuizmo.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <system_error>

namespace {
    const float UI_FONT_SIZE = 14.0f;
    const float GAME_FONT_SIZE = 48.0f;  // Loaded large enough for the biggest HUD text; ImGui scales down cleanly.

    ImFont* loadFont(const char* fileName, float size) {
        const std::string path = Resources::path(std::string("fonts/") + fileName);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            Log::warn("Editor", "Font not found, using the built-in one: " + path);
            return nullptr;
        }
        return ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), size);
    }
}

ImGuiLayer::ImGuiLayer(GLFWwindow* window, const std::string& layoutFile) : layoutPath(layoutFile) {
    IMGUI_CHECKVERSION();
    context = ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigDragClickToInputText = true;
    io.IniFilename = layoutPath.empty() ? nullptr : layoutPath.c_str();

    ui = loadFont("Roboto-Medium.ttf", UI_FONT_SIZE);
    mono = loadFont("Cousine-Regular.ttf", UI_FONT_SIZE);
    game = bundledGame = loadFont("Inter.ttf", GAME_FONT_SIZE);
    if (!ui) ui = io.Fonts->AddFontDefault();
    if (!mono) mono = ui;
    if (!game) game = bundledGame = ui;
    io.FontDefault = ui;

    Theme::apply();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
}

ImFont* ImGuiLayer::setGameFont(const std::string& path) {
    ImFont* chosen = bundledGame;
    std::error_code ec;
    if (!path.empty() && std::filesystem::exists(path, ec)) {
        if (ImFont* loaded = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), GAME_FONT_SIZE)) {
            chosen = loaded;
        } else {
            Log::warn("Editor", "Cannot read the font, using Inter: " + path);
        }
    } else if (!path.empty()) {
        Log::warn("Editor", "Font not found, using Inter: " + path);
    }
    // An earlier custom font is no longer needed.
    if (game && game != bundledGame && game != chosen) ImGui::GetIO().Fonts->RemoveFont(game);
    game = chosen;
    return game;
}

ImGuiLayer::~ImGuiLayer() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(context);
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();

    const ImGuiIO& io = ImGui::GetIO();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x),
               (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y));
    glClearColor(0.11f, 0.118f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
