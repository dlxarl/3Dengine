#include "MainBars.h"
#include "HierarchyPanel.h"
#include "Theme.h"
#include "Widgets.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace {
    const float STATUS_HEIGHT = 24.0f;
}

bool MainBars::takeLayoutReset() {
    const bool requested = resetLayout;
    resetLayout = false;
    return requested;
}

void MainBars::draw(EditorContext& ctx, PanelVisibility& panels) {
    drawMenuBar(ctx, panels);
    drawToolbar(ctx);
    drawStatusBar(ctx);
    handleShortcuts(ctx);
}

void MainBars::handleShortcuts(EditorContext& ctx) {
    const ImGuiInputFlags global = ImGuiInputFlags_RouteGlobal;

    // ImGuiMod_Ctrl is the Command key on macOS.
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, global)) ctx.saveScene();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, global)) ctx.saveSceneAs();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_N, global)) ctx.newScene();
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_D, global)) ctx.duplicateSelected();

    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, global)) {
        if (ctx.play.isStopped()) {
            ctx.startPlaying();
        } else {
            ctx.stopPlaying();
        }
    }
    if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, global)) ctx.play.togglePause();

    if (!ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
            ctx.deleteSelected();
        }
    }
}

void MainBars::drawOpenSceneItems(EditorContext& ctx) {
    // Listed when the menu opens, which is rare enough to read the disk for.
    std::vector<std::string> scenes;
    std::error_code ec;
    for (fs::recursive_directory_iterator it(ctx.project.assetsDir(), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() == ".scene") {
            const std::string assetPath = ctx.assets.toAssetPath(it->path());
            if (!assetPath.empty()) scenes.push_back(assetPath);
        }
    }
    std::sort(scenes.begin(), scenes.end());

    if (scenes.empty()) {
        ImGui::MenuItem("No scenes in this project", nullptr, false, false);
    }
    for (const std::string& scene : scenes) {
        if (ImGui::MenuItem(scene.c_str(), nullptr, scene == ctx.document.getAssetPath())) {
            ctx.openScene(scene);
        }
    }
}

void MainBars::drawMenuBar(EditorContext& ctx, PanelVisibility& panels) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    if (!ImGui::BeginMainMenuBar()) {
        ImGui::PopStyleVar();
        return;
    }

    const bool editing = ctx.play.isStopped();

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New scene", "Cmd+N")) ctx.newScene();
        if (ImGui::BeginMenu("Open scene")) {
            drawOpenSceneItems(ctx);
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Cmd+S", false, editing)) ctx.saveScene();
        if (ImGui::MenuItem("Save as", "Shift+Cmd+S", false, editing)) ctx.saveSceneAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Close project")) ctx.closeProject();
        if (ImGui::MenuItem("Quit", "Cmd+Q")) ctx.quit();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        const bool hasSelection = ctx.getSelected() != nullptr;
        if (ImGui::MenuItem("Rename", "F2", false, hasSelection)) ctx.requestRename(ctx.selectedObject);
        if (ImGui::MenuItem("Duplicate", "Cmd+D", false, hasSelection)) ctx.duplicateSelected();
        if (ImGui::MenuItem("Delete", "Delete", false, hasSelection)) ctx.deleteSelected();
        ImGui::Separator();
        if (ImGui::MenuItem("Focus selection", "F", false, hasSelection)) ctx.focusRequested = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create")) {
        drawCreateObjectItems(ctx, InvalidObjectId);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
        ImGui::MenuItem("Hierarchy", nullptr, &panels.hierarchy);
        ImGui::MenuItem("Inspector", nullptr, &panels.inspector);
        ImGui::MenuItem("Project", nullptr, &panels.project);
        ImGui::MenuItem("Console", nullptr, &panels.console);
        ImGui::MenuItem("Render Settings", nullptr, &panels.renderSettings);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset layout")) resetLayout = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Keyboard and mouse")) ctx.dialogs.shortcuts();
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    ImGui::PopStyleVar();
}

void MainBars::drawToolbar(EditorContext& ctx) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::Panel);
    const bool open = ImGui::BeginViewportSideBar("##toolbar", viewport, ImGuiDir_Up, Theme::ToolbarHeight, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open) {
        ImGui::End();
        return;
    }

    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetContentRegionAvail().y;
    const ImVec2 buttonSize(height, height);
    const float textY = ImGui::GetCursorPosY() + (height - ImGui::GetTextLineHeight()) * 0.5f;
    const float startY = ImGui::GetCursorPosY();

    // Left: where you are, and whether it is saved.
    ImGui::SetCursorPosY(textY);
    ImGui::TextUnformatted(ctx.project.getInfo().name.c_str());
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetCursorPosY(textY);
    Widgets::dimText("/");
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::SetCursorPosY(textY);
    const std::string scene = ctx.document.getDisplayName() + (ctx.document.isDirty() ? " *" : "");
    Widgets::dimText(scene.c_str());

    ImGui::SameLine(0.0f, 10.0f);
    ImGui::SetCursorPosY(startY);
    ImGui::BeginDisabled(!ctx.play.isStopped());
    if (Widgets::iconButton("##save", Icons::save, false, "Save scene (Cmd+S)", buttonSize)) {
        ctx.saveScene();
    }
    ImGui::EndDisabled();

    // Center: the transport.
    const float transportWidth = buttonSize.x * 3.0f + 4.0f;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorStartPos().x + (width - transportWidth) * 0.5f, startY));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

    if (Widgets::iconButton("##play", Icons::play, !ctx.play.isStopped(), "Play (Cmd+P)", buttonSize)) {
        if (ctx.play.isStopped()) {
            ctx.startPlaying();
        } else if (ctx.play.isPaused()) {
            ctx.play.togglePause();
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(ctx.play.isStopped());
    if (Widgets::iconButton("##pause", Icons::pause, ctx.play.isPaused(), "Pause (Shift+Cmd+P)", buttonSize)) {
        ctx.play.togglePause();
    }
    ImGui::SameLine();
    if (Widgets::iconButton("##stop", Icons::stop, false, "Stop", buttonSize)) {
        ctx.stopPlaying();
    }
    ImGui::EndDisabled();
    ImGui::PopStyleVar();

    // Right: which view the main area shows.
    const float segmentWidth = 58.0f;
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorStartPos().x + width - segmentWidth * 2.0f - 1.0f,
                               startY + (height - ImGui::GetFrameHeight()) * 0.5f));
    if (Widgets::segment("Scene", ctx.viewMode == ViewMode::Scene, segmentWidth, true, false)) {
        ctx.viewMode = ViewMode::Scene;
    }
    ImGui::SameLine(0.0f, 1.0f);
    if (Widgets::segment("Game", ctx.viewMode == ViewMode::Game, segmentWidth, false, true)) {
        ctx.viewMode = ViewMode::Game;
    }

    ImGui::End();
}

void MainBars::drawStatusBar(EditorContext& ctx) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, Theme::Sunken);
    const bool open = ImGui::BeginViewportSideBar("##status", viewport, ImGuiDir_Down, STATUS_HEIGHT, flags);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    if (!open) {
        ImGui::End();
        return;
    }

    if (!ctx.play.isStopped()) {
        ImGui::TextColored(Theme::color(Theme::Accent), ctx.play.isPaused() ? "Paused" : "Playing");
        ImGui::SameLine();
        Widgets::dimText("Changes are undone when you stop.");
    } else if (!ctx.getStatus().empty()) {
        Widgets::dimText(ctx.getStatus().c_str());
    } else {
        ConsoleEntry last;
        if (ctx.console.getLast(last)) Widgets::dimText(last.message.c_str());
    }

    char fps[32];
    std::snprintf(fps, sizeof(fps), "%.0f FPS", ImGui::GetIO().Framerate);
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(fps).x);
    Widgets::dimText(fps);

    ImGui::End();
}
