#include "LauncherApp.h"
#include "GlWindow.h"
#include "ImGuiLayer.h"
#include "Log.h"
#include "Platform.h"
#include "Theme.h"
#include "UserPaths.h"
#include "Widgets.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    const float SIDEBAR_WIDTH = 212.0f;
    const float DETAILS_WIDTH = 288.0f;
    const float ROW_HEIGHT = 54.0f;
    const float PAGE_PADDING = 24.0f;
    const float TITLE_SIZE = 22.0f;

    std::string relativeTime(int64_t seconds) {
        if (seconds <= 0) return "Never";

        const int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        const int64_t age = std::max<int64_t>(0, now - seconds);

        if (age < 60) return "Just now";
        if (age < 3600) return std::to_string(age / 60) + " min ago";
        if (age < 86400) return std::to_string(age / 3600) + " h ago";
        if (age < 86400 * 2) return "Yesterday";
        if (age < 86400 * 30) return std::to_string(age / 86400) + " days ago";

        const std::time_t time = (std::time_t)seconds;
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%d %b %Y", &local);
        return buffer;
    }

    bool containsIgnoreCase(const std::string& text, const std::string& needle) {
        if (needle.empty()) return true;
        auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(), [](char a, char b) {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
        });
        return it != text.end();
    }

    void pageTitle(const char* text) {
        ImGui::PushFont(nullptr, TITLE_SIZE);
        ImGui::TextUnformatted(text);
        ImGui::PopFont();
    }

    std::string uniqueProjectName(const fs::path& location) {
        std::error_code ec;
        std::string name = "My Game";
        for (int suffix = 2; fs::exists(location / fs::u8path(name), ec); suffix++) {
            name = "My Game " + std::to_string(suffix);
        }
        return name;
    }

    void detailRow(const char* label, const std::string& value) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        Widgets::dimText(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(value.c_str());
        ImGui::PopTextWrapPos();
    }

    int countScenes(const fs::path& root) {
        std::error_code ec;
        int count = 0;
        for (fs::recursive_directory_iterator it(root / "Assets", ec), end; !ec && it != end; it.increment(ec)) {
            if (it->path().extension() == ".scene") count++;
        }
        return count;
    }
}

LauncherApp::LauncherApp() : recents(RecentProjects::defaultFile()) {
    recents.load();

    newLocation = recents.getLastLocation();
    if (newLocation.empty()) {
        newLocation = UserPaths::defaultProjectsDir().u8string();
    }

    if (!recents.getEntries().empty()) {
        selectRecent(0);
    }
}

std::optional<fs::path> LauncherApp::run() {
    GlWindow::Config config;
    config.width = 1080;
    config.height = 680;
    config.title = "Aureon Editor";
    GlWindow window(config);
    if (!window.isValid()) return std::nullopt;

    glfwSetWindowSizeLimits(window.handle(), 860, 540, GLFW_DONT_CARE, GLFW_DONT_CARE);

    ImGuiLayer imgui(window.handle(), "");

    while (!glfwWindowShouldClose(window.handle()) && !chosen) {
        glfwWaitEventsTimeout(0.05);

        imgui.beginFrame();
        drawFrame();
        imgui.endFrame();

        glfwSwapBuffers(window.handle());
    }

    recents.save();
    return chosen;
}

void LauncherApp::drawFrame() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##launcher", nullptr, flags);

    const ImVec2 size = ImGui::GetContentRegionAvail();
    drawSidebar(SIDEBAR_WIDTH, size.y);
    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PAGE_PADDING, PAGE_PADDING - 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
    ImGui::BeginChild("##page", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar);
    if (page == Page::Projects) {
        drawProjectsPage();
    } else {
        drawNewProjectPage();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);

    ImGui::End();
    ImGui::PopStyleVar(3);
}

void LauncherApp::drawSidebar(float width, float height) {
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Theme::Sunken);
    draw->AddLine(ImVec2(origin.x + width - 1.0f, origin.y), ImVec2(origin.x + width - 1.0f, origin.y + height), Theme::Line);

    // The falloff anchors the sidebar's lower edge, under everything else.
    const float falloffHeight = 132.0f;
    drawDitherFalloff(draw, ImVec2(origin.x, origin.y + height - falloffHeight),
                      ImVec2(origin.x + width - 1.0f, origin.y + height), 3.0f, Theme::AccentDim);

    ImGui::BeginChild("##sidebar", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);

    drawBrandMark(draw, ImVec2(origin.x + 20.0f, origin.y + 22.0f), 24.0f);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + 54.0f, origin.y + 22.0f));
    ImGui::PushFont(nullptr, 17.0f);
    ImGui::TextUnformatted("Aureon Editor");
    ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + 70.0f));

    auto navItem = [&](const char* label, bool selected) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 itemSize(width - 1.0f, 34.0f);
        const bool pressed = ImGui::InvisibleButton(label, itemSize);
        const ImVec2 max(pos.x + itemSize.x, pos.y + itemSize.y);

        if (selected) {
            draw->AddRectFilled(pos, max, Theme::Panel);
            draw->AddRectFilled(pos, ImVec2(pos.x + 2.0f, max.y), Theme::Accent);
        } else if (ImGui::IsItemHovered()) {
            draw->AddRectFilled(pos, max, Theme::rgb(0x202326));
        }
        draw->AddText(ImVec2(pos.x + 20.0f, pos.y + 9.0f), selected ? Theme::Text : Theme::TextDim, label);
        return pressed;
    };

    if (navItem("Projects", page == Page::Projects)) page = Page::Projects;
    if (navItem("New project", page == Page::NewProject)) beginNewProject();

    ImGui::EndChild();
}

void LauncherApp::drawBanner() {
    if (banner.empty()) return;

    ImGui::PushStyleColor(ImGuiCol_Text, Theme::Error);
    ImGui::TextWrapped("%s", banner.c_str());
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

void LauncherApp::drawProjectsPage() {
    const float available = ImGui::GetContentRegionAvail().x;

    pageTitle("Projects");

    // Actions, right-aligned on the title's line.
    const float newWidth = ImGui::CalcTextSize("New project").x + 28.0f;
    const float openWidth = ImGui::CalcTextSize("Open project").x + 16.0f;
    ImGui::SameLine(available - newWidth - openWidth - 8.0f);
    if (ImGui::Button("Open project")) openChooser();
    ImGui::SameLine();
    if (Widgets::primaryButton("New project")) beginNewProject();

    ImGui::Spacing();
    ImGui::Spacing();
    drawBanner();

    if (!Platform::hasNativeDialogs()) {
        ImGui::SetNextItemWidth(available - 90.0f);
        Widgets::inputTextHint("##typedOpenPath", "Path of a project folder", typedOpenPath);
        ImGui::SameLine();
        if (ImGui::Button("Open") && !typedOpenPath.empty()) openProject(UserPaths::expand(typedOpenPath));
        ImGui::Spacing();
    }

    if (recents.getEntries().empty()) {
        drawEmptyState(available, ImGui::GetContentRegionAvail().y);
        return;
    }

    const float height = ImGui::GetContentRegionAvail().y;
    const float listWidth = available - DETAILS_WIDTH - 16.0f;
    drawProjectList(listWidth, height);
    ImGui::SameLine(0.0f, 16.0f);
    drawDetails(DETAILS_WIDTH, height);
}

void LauncherApp::drawEmptyState(float width, float height) {
    const char* line = "No projects yet";
    const char* hint = "Create a project, or open a folder that contains a Project.json file.";

    const ImVec2 textSize = ImGui::CalcTextSize(hint);
    ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (width - textSize.x) * 0.5f,
                               ImGui::GetCursorPosY() + height * 0.34f));
    const float x = ImGui::GetCursorPosX();

    ImGui::PushFont(nullptr, 17.0f);
    ImGui::SetCursorPosX(x + (textSize.x - ImGui::CalcTextSize(line).x) * 0.5f);
    ImGui::TextUnformatted(line);
    ImGui::PopFont();

    ImGui::SetCursorPosX(x);
    Widgets::dimText(hint);
}

void LauncherApp::drawProjectList(float width, float height) {
    ImGui::SetNextItemWidth(width);
    Widgets::inputTextHint("##search", "Search projects", search);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##list", ImVec2(width, height - ImGui::GetFrameHeight() - 12.0f), ImGuiChildFlags_None);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const auto& entries = recents.getEntries();
    int openRequest = -1;

    for (int i = 0; i < (int)entries.size(); i++) {
        const RecentProject& entry = entries[i];
        if (!containsIgnoreCase(entry.name, search) && !containsIgnoreCase(entry.path, search)) continue;

        ImGui::PushID(i);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, ROW_HEIGHT);
        const ImVec2 max(pos.x + size.x, pos.y + size.y);

        ImGui::InvisibleButton("##row", size);
        if (ImGui::IsItemClicked()) selectRecent(i);
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) openRequest = i;

        const bool selected = i == selectedIndex;
        if (selected) {
            draw->AddRectFilled(pos, max, Theme::rgb(0x6FBFAE, 26));
            draw->AddRectFilled(pos, ImVec2(pos.x + 2.0f, max.y), Theme::Accent);
        } else if (ImGui::IsItemHovered()) {
            draw->AddRectFilled(pos, max, Theme::Raised);
        }
        draw->AddLine(ImVec2(pos.x, max.y - 1.0f), ImVec2(max.x, max.y - 1.0f), Theme::Line);

        std::error_code ec;
        const bool exists = fs::is_directory(fs::u8path(entry.path), ec);

        const std::string when = exists ? relativeTime(entry.lastOpened) : "Folder not found";
        const float whenWidth = ImGui::CalcTextSize(when.c_str()).x;
        draw->AddText(ImVec2(max.x - whenWidth - 12.0f, pos.y + 19.0f), exists ? Theme::TextDim : Theme::Warning, when.c_str());

        const float textMaxX = max.x - whenWidth - 24.0f;
        draw->PushClipRect(pos, ImVec2(textMaxX, max.y), true);
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 10.0f), exists ? Theme::Text : Theme::TextDim, entry.name.c_str());
        draw->PopClipRect();

        const std::string shortPath = UserPaths::abbreviate(fs::u8path(entry.path));
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
        ImGui::RenderTextEllipsis(draw, ImVec2(pos.x + 14.0f, pos.y + 29.0f), ImVec2(textMaxX, max.y), textMaxX,
                                  shortPath.c_str(), nullptr, nullptr);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(pos.x, max.y));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        ImGui::PopID();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();

    if (openRequest >= 0) {
        openProject(fs::u8path(entries[openRequest].path));
    }
}

void LauncherApp::drawDetails(float width, float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 14.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Sunken);
    ImGui::BeginChild("##details", ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding);

    if (selectedIndex < 0 || selectedIndex >= (int)recents.getEntries().size()) {
        Widgets::wrappedDim("Select a project to see its details.");
    } else {
        const RecentProject entry = recents.getEntries()[selectedIndex];

        ImGui::PushFont(nullptr, 17.0f);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::Spacing();

        if (ImGui::BeginTable("##info", 2, ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 86.0f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

            detailRow("Location", UserPaths::abbreviate(fs::u8path(entry.path)));
            if (details.info) {
                detailRow("Engine", details.info->engineVersion.empty() ? "Unknown" : details.info->engineVersion);
                detailRow("Created", details.info->createdAt.substr(0, 10));
                detailRow("Scenes", std::to_string(details.sceneCount));
            }
            detailRow("Last opened", relativeTime(entry.lastOpened));
            ImGui::EndTable();
        }

        ImGui::Spacing();

        if (!details.problem.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, Theme::Warning);
            ImGui::TextWrapped("%s", details.problem.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (details.problem.empty()) {
            if (Widgets::primaryButton("Open", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
                openProject(fs::u8path(entry.path));
            }
        }

        if (details.exists && ImGui::Button("Show in Finder", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            Platform::revealInFileManager(fs::u8path(entry.path));
        }

        if (ImGui::Button("Remove from list", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
            recents.remove(entry.path);
            recents.save();
            selectRecent(recents.getEntries().empty() ? -1 : 0);
        }
        if (ImGui::IsItemHovered()) Widgets::tooltip("The project stays on disk.");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void LauncherApp::drawNewProjectPage() {
    pageTitle("New project");
    ImGui::Spacing();
    ImGui::Spacing();
    drawBanner();

    Widgets::dimText("Template");

    const auto& templates = projectTemplates();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < templates.size(); i++) {
        ImGui::PushID((int)i);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, ROW_HEIGHT);
        const ImVec2 max(pos.x + size.x, pos.y + size.y);

        if (ImGui::InvisibleButton("##template", size)) templateIndex = i;

        const bool selected = i == templateIndex;
        if (selected) {
            draw->AddRectFilled(pos, max, Theme::rgb(0x6FBFAE, 26));
            draw->AddRectFilled(pos, ImVec2(pos.x + 2.0f, max.y), Theme::Accent);
        } else if (ImGui::IsItemHovered()) {
            draw->AddRectFilled(pos, max, Theme::Raised);
        }
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 10.0f), Theme::Text, templates[i].name);
        draw->AddText(ImVec2(pos.x + 14.0f, pos.y + 29.0f), Theme::TextDim, templates[i].description);
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    Widgets::dimText("Project name");
    ImGui::SetNextItemWidth(-FLT_MIN);
    Widgets::inputText("##name", newName);

    ImGui::Spacing();
    Widgets::dimText("Location");
    const bool canBrowse = Platform::hasNativeDialogs();
    ImGui::SetNextItemWidth(canBrowse ? -(ImGui::CalcTextSize("Browse").x + 24.0f) : -FLT_MIN);
    Widgets::inputText("##location", newLocation);
    if (canBrowse) {
        ImGui::SameLine();
        if (ImGui::Button("Browse")) chooseLocation();
    }

    ProjectCreateOptions options;
    options.name = newName;
    options.location = UserPaths::expand(newLocation);
    options.templateId = templates[templateIndex].id;

    const std::string problem = validateCreateOptions(options);

    ImGui::Spacing();
    if (problem.empty()) {
        const std::string target = "Creates " + UserPaths::abbreviate(options.location / fs::u8path(newName));
        Widgets::wrappedDim(target.c_str());
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Warning);
        ImGui::TextWrapped("%s", problem.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::BeginDisabled(!problem.empty());
    if (Widgets::primaryButton("Create project")) createProject();
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
    if (ImGui::Button("Cancel")) {
        banner.clear();
        page = Page::Projects;
    }
    ImGui::PopStyleVar();
}

void LauncherApp::selectRecent(int index) {
    selectedIndex = index;
    details = ProjectDetails();
    if (index < 0 || index >= (int)recents.getEntries().size()) return;

    // Read once per selection rather than every frame.
    const RecentProject& entry = recents.getEntries()[index];
    const fs::path root = fs::u8path(entry.path);
    details.path = entry.path;

    std::error_code ec;
    details.exists = fs::is_directory(root, ec);
    if (!details.exists) {
        details.problem = "The folder was moved or deleted. Open it from its new location, or remove it from this list.";
        return;
    }

    const ProjectValidation validation = Project::validate(root);
    if (!validation.isValid()) {
        details.problem = validation.errors.front();
    }
    details.info = Project::readInfo(root);
    details.sceneCount = countScenes(root);
}

void LauncherApp::openChooser() {
    if (!Platform::hasNativeDialogs()) return;

    const fs::path folder = Platform::chooseFolder("Choose a project folder", UserPaths::expand(newLocation));
    if (!folder.empty()) {
        openProject(folder);
    }
}

void LauncherApp::openProject(const fs::path& root) {
    banner.clear();

    const ProjectValidation validation = Project::validate(root);
    if (!validation.isValid()) {
        banner = validation.errors.front();
        return;
    }

    chosen = root;
}

void LauncherApp::beginNewProject() {
    banner.clear();
    page = Page::NewProject;
    newName = uniqueProjectName(UserPaths::expand(newLocation));
}

void LauncherApp::chooseLocation() {
    const fs::path folder = Platform::chooseFolder("Choose where to create the project", UserPaths::expand(newLocation));
    if (!folder.empty()) {
        newLocation = folder.u8string();
    }
}

void LauncherApp::createProject() {
    banner.clear();

    ProjectCreateOptions options;
    options.name = newName;
    options.location = UserPaths::expand(newLocation);
    options.templateId = projectTemplates()[templateIndex].id;

    std::string error;
    const std::optional<Project> project = Project::create(options, &error);
    if (!project) {
        banner = error;
        return;
    }

    recents.setLastLocation(options.location.u8string());
    recents.save();
    chosen = project->getRoot();
}
