#include "ProjectPanel.h"
#include "Log.h"
#include "Platform.h"
#include "SceneSerializer.h"
#include "Theme.h"
#include "Widgets.h"

#include "imgui_internal.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    const float TREE_WIDTH = 170.0f;
    const float TILE_WIDTH = 92.0f;
    const float TILE_HEIGHT = 96.0f;
    const float TILE_ICON = 52.0f;
    const float RESCAN_INTERVAL = 2.0f;

    bool isHidden(const fs::path& path) {
        const std::string name = path.filename().u8string();
        if (name.empty() || name[0] == '.') return true;
        const fs::path extension = path.extension();
        return extension == ".tmp";
    }

    std::string lowered(std::string text) {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return text;
    }

    bool validFolderName(const std::string& name) {
        return !name.empty() && name.front() != '.' && name.find_first_of("/\\:") == std::string::npos;
    }
}

ProjectPanel::ProjectPanel(EditorContext& ctx) : currentFolder(ctx.project.assetsDir()) {}

void ProjectPanel::rescan(EditorContext& ctx) {
    rescanRequested = false;
    rescanTimer = RESCAN_INTERVAL;

    std::error_code ec;

    // A folder can vanish behind the Editor's back.
    if (!fs::is_directory(currentFolder, ec)) {
        currentFolder = ctx.project.assetsDir();
    }

    entries.clear();
    for (fs::directory_iterator it(currentFolder, ec), end; !ec && it != end; it.increment(ec)) {
        if (isHidden(it->path())) continue;

        Entry entry;
        entry.path = it->path();
        entry.name = entry.path.filename().u8string();
        entry.assetPath = ctx.assets.toAssetPath(entry.path);
        entry.kind = classifyAsset(entry.path);
        entries.push_back(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
        const bool aFolder = a.kind == AssetKind::Folder;
        const bool bFolder = b.kind == AssetKind::Folder;
        if (aFolder != bFolder) return aFolder;
        return lowered(a.name) < lowered(b.name);
    });

    auto scanFolders = [](auto&& self, FolderNode& node) -> void {
        std::error_code scanError;
        for (fs::directory_iterator it(node.path, scanError), end; !scanError && it != end; it.increment(scanError)) {
            if (!it->is_directory(scanError) || isHidden(it->path())) continue;
            FolderNode child;
            child.path = it->path();
            child.name = child.path.filename().u8string();
            self(self, child);
            node.children.push_back(std::move(child));
        }
        std::sort(node.children.begin(), node.children.end(),
                  [](const FolderNode& a, const FolderNode& b) { return lowered(a.name) < lowered(b.name); });
    };

    folderTree = FolderNode();
    folderTree.path = ctx.project.assetsDir();
    folderTree.name = "Assets";
    scanFolders(scanFolders, folderTree);
}

void ProjectPanel::importFiles(EditorContext& ctx, const std::vector<fs::path>& files) {
    std::string firstError;
    int imported = 0;

    for (const fs::path& file : files) {
        const ImportResult result = importAsset(ctx.project, file, currentFolder);
        if (result.ok()) {
            imported++;
        } else {
            Log::warn("Assets", result.error);
            if (firstError.empty()) firstError = result.error;
        }
    }

    rescanRequested = true;
    if (imported > 0) {
        ctx.setStatus("Imported " + std::to_string(imported) + (imported == 1 ? " file" : " files"));
    }
    if (!firstError.empty()) {
        ctx.dialogs.error("Some files were not imported", firstError);
    }
}

void ProjectPanel::chooseAndImport(EditorContext& ctx) {
    const auto files = Platform::chooseFiles("Choose models and textures to import",
                                             {"png", "jpg", "jpeg", "tga", "bmp", "hdr", "obj"});
    if (!files.empty()) {
        importFiles(ctx, files);
    }
}

void ProjectPanel::promptNewFolder(EditorContext& ctx) {
    ctx.dialogs.promptText("New folder", "Folder name", "New Folder", "Create folder", [this](const std::string& name) {
        if (!validFolderName(name)) return std::string("A folder name cannot contain / \\ or : or start with a dot.");

        std::error_code ec;
        const fs::path target = currentFolder / fs::u8path(name);
        if (fs::exists(target, ec)) return std::string("A file or folder with this name already exists.");

        fs::create_directory(target, ec);
        if (ec) return "Cannot create the folder: " + ec.message();

        rescanRequested = true;
        return std::string();
    });
}

void ProjectPanel::promptNewScene(EditorContext& ctx) {
    ctx.dialogs.promptText("New scene file", "Scene name", "New Scene", "Create scene", [this, &ctx](const std::string& name) {
        if (!validFolderName(name)) return std::string("A scene name cannot contain / \\ or : or start with a dot.");

        const fs::path target = currentFolder / fs::u8path(name + ".scene");
        std::error_code ec;
        if (fs::exists(target, ec)) return std::string("A scene with this name already exists here.");

        SceneGraph empty;
        std::string error;
        if (!SceneSerializer::saveFile(empty, target, &error)) return error;

        rescanRequested = true;
        ctx.setStatus("Created " + name + ".scene");
        return std::string();
    });
}

void ProjectPanel::promptNewScript(EditorContext& ctx, const ScriptBuilder::Template& source) {
    const ScriptBuilder::Template* chosen = &source;
    ctx.dialogs.promptText("New script", "Script name", chosen->name, "Create script", [this, &ctx, chosen](const std::string& name) {
        std::string error;
        // Scripts collect in their own folder unless the user is somewhere else on purpose.
        std::error_code ec;
        const fs::path folder = fs::equivalent(currentFolder, ctx.project.assetsDir(), ec) ? ctx.project.assetsDir() / "Scripts" : currentFolder;
        const fs::path file = ScriptBuilder::createFromTemplate(*chosen, name, folder, error);
        if (file.empty()) return error;

        rescanRequested = true;
        ctx.setStatus("Created " + file.filename().u8string());
        return std::string();
    });
}

void ProjectPanel::confirmTrash(EditorContext& ctx, const Entry& entry) {
    const fs::path path = entry.path;
    const std::string assetPath = entry.assetPath;

    ctx.dialogs.confirm("Move to Trash", "Move " + entry.name + " to the Trash? Objects that use it lose it the next time the scene loads.",
                        "Move to Trash", [this, &ctx, path, assetPath] {
        std::string error;
        if (!Platform::moveToTrash(path, &error)) {
            ctx.dialogs.error("Cannot move to the Trash", error);
            return;
        }
        ctx.assets.invalidate(assetPath);
        if (ctx.selectedAsset == assetPath) ctx.selectedAsset.clear();
        rescanRequested = true;
    });
}

void ProjectPanel::activate(EditorContext& ctx, const Entry& entry) {
    switch (entry.kind) {
        case AssetKind::Folder:
            navigateTo = entry.path;
            break;
        case AssetKind::Scene:
            ctx.openScene(entry.assetPath);
            break;
        case AssetKind::Model:
            ctx.addModel(entry.assetPath, glm::vec3(0.0f));
            break;
        case AssetKind::Texture:
            if (Shape* shape = dynamic_cast<Shape*>(ctx.getSelected())) {
                ctx.setObjectTexture(shape->id, entry.assetPath);
            }
            break;
        default:
            break;
    }
}

void ProjectPanel::importDroppedFiles(EditorContext& ctx) {
    if (ctx.droppedFiles.empty()) return;

    const std::vector<fs::path> dropped = std::move(ctx.droppedFiles);
    ctx.droppedFiles.clear();
    importFiles(ctx, dropped);
}

void ProjectPanel::draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open) {
    if (!ImGui::Begin("Project", open)) {
        ImGui::End();
        return;
    }

    if (!navigateTo.empty()) {
        currentFolder = navigateTo;
        navigateTo.clear();
        rescanRequested = true;
    }

    rescanTimer -= ImGui::GetIO().DeltaTime;
    if (rescanRequested || (rescanTimer <= 0.0f && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))) {
        rescan(ctx);
    }

    drawHeader(ctx);

    const float height = ImGui::GetContentRegionAvail().y;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Sunken);
    ImGui::BeginChild("##folders", ImVec2(TREE_WIDTH, height), ImGuiChildFlags_AlwaysUseWindowPadding);
    drawFolderNode(folderTree);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    ImGui::SameLine(0.0f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::BeginChild("##grid", ImVec2(0.0f, height), ImGuiChildFlags_AlwaysUseWindowPadding);
    drawGrid(ctx, thumbnails);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    ImGui::End();
}

void ProjectPanel::drawHeader(EditorContext& ctx) {
    if (Platform::hasNativeDialogs()) {
        if (ImGui::Button("Import")) chooseAndImport(ctx);
        if (ImGui::IsItemHovered()) Widgets::tooltip("Copy models and textures into the project. You can also drop files on the window.");
        ImGui::SameLine();
    }

    if (ImGui::Button("New folder")) promptNewFolder(ctx);
    ImGui::SameLine();

    drawBreadcrumbs(ctx);

    const float searchWidth = 170.0f;
    ImGui::SameLine(ImGui::GetContentRegionMax().x - searchWidth);
    ImGui::SetNextItemWidth(searchWidth);
    Widgets::inputTextHint("##search", "Search this folder", search);
}

void ProjectPanel::drawBreadcrumbs(EditorContext& ctx) {
    const fs::path relative = currentFolder.lexically_relative(ctx.project.getRoot());

    fs::path walked = ctx.project.getRoot();
    bool first = true;

    ImGui::PushStyleColor(ImGuiCol_Button, Theme::rgb(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
    int id = 0;
    for (const fs::path& part : relative) {
        walked /= part;
        if (!first) {
            ImGui::SameLine(0.0f, 2.0f);
            Widgets::dimText("/");
            ImGui::SameLine(0.0f, 2.0f);
        }
        first = false;

        ImGui::PushID(id++);
        if (ImGui::Button(part.u8string().c_str())) {
            navigateTo = walked;
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void ProjectPanel::drawFolderNode(const FolderNode& node) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
                               ImGuiTreeNodeFlags_FramePadding;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (node.path == currentFolder) flags |= ImGuiTreeNodeFlags_Selected;
    if (node.name == "Assets") flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const bool open = ImGui::TreeNodeEx(node.path.u8string().c_str(), flags, "%s", node.name.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        navigateTo = node.path;
    }

    if (open) {
        for (const FolderNode& child : node.children) {
            drawFolderNode(child);
        }
        ImGui::TreePop();
    }
}

void ProjectPanel::drawEntryMenu(EditorContext& ctx, const Entry& entry) {
    if (!ImGui::BeginPopupContextItem("##entryMenu")) return;

    if (entry.kind == AssetKind::Scene && ImGui::MenuItem("Open scene")) {
        ctx.openScene(entry.assetPath);
    }
    if (entry.kind == AssetKind::Model && ImGui::MenuItem("Add to scene")) {
        ctx.addModel(entry.assetPath, glm::vec3(0.0f));
    }
    if (entry.kind == AssetKind::Texture) {
        Shape* shape = dynamic_cast<Shape*>(ctx.getSelected());
        if (ImGui::MenuItem("Apply to selected object", nullptr, false, shape != nullptr)) {
            ctx.setObjectTexture(shape->id, entry.assetPath);
        }
    }
    if (entry.kind == AssetKind::Folder && ImGui::MenuItem("Open")) {
        navigateTo = entry.path;
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Show in Finder", nullptr, false, Platform::hasNativeDialogs())) {
        Platform::revealInFileManager(entry.path);
    }
    if (ImGui::MenuItem("Move to Trash")) {
        confirmTrash(ctx, entry);
    }
    ImGui::EndPopup();
}

void ProjectPanel::drawBackgroundMenu(EditorContext& ctx) {
    if (!ImGui::BeginPopupContextWindow("##gridMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) return;

    if (ImGui::MenuItem("New folder")) promptNewFolder(ctx);
    if (ImGui::MenuItem("New scene file")) promptNewScene(ctx);
    if (ImGui::BeginMenu("New script")) {
        for (const ScriptBuilder::Template& source : ScriptBuilder::templates()) {
            if (ImGui::MenuItem(source.name)) promptNewScript(ctx, source);
            if (ImGui::IsItemHovered()) Widgets::tooltip(source.description);
        }
        ImGui::EndMenu();
    }
    if (Platform::hasNativeDialogs()) {
        if (ImGui::MenuItem("Import")) chooseAndImport(ctx);
        ImGui::Separator();
        if (ImGui::MenuItem("Show in Finder")) Platform::revealInFileManager(currentFolder);
    }
    ImGui::EndPopup();
}

bool ProjectPanel::drawTile(EditorContext& ctx, AssetThumbnails& thumbnails, const Entry& entry) {
    ImGui::PushID(entry.name.c_str());

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 max(pos.x + TILE_WIDTH, pos.y + TILE_HEIGHT);

    ImGui::InvisibleButton("##tile", ImVec2(TILE_WIDTH, TILE_HEIGHT));
    const bool hovered = ImGui::IsItemHovered();
    const bool selected = ctx.selectedAsset == entry.assetPath;

    if (ImGui::IsItemClicked()) {
        ctx.selectAsset(entry.assetPath);
    }
    bool activated = hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

    if (entry.kind != AssetKind::Folder) {
        Widgets::beginAssetDrag(entry.assetPath, entry.kind, entry.name.c_str());
    }
    drawEntryMenu(ctx, entry);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (selected) {
        draw->AddRectFilled(pos, max, Theme::rgb(0x6FBFAE, 40), 3.0f);
    } else if (hovered) {
        draw->AddRectFilled(pos, max, Theme::Raised, 3.0f);
    }

    const ImVec2 iconPos(pos.x + (TILE_WIDTH - TILE_ICON) * 0.5f, pos.y + 8.0f);
    const unsigned int thumbnail = entry.kind == AssetKind::Texture ? thumbnails.get(entry.assetPath) : 0;
    if (thumbnail != 0) {
        draw->AddImage((ImTextureID)(intptr_t)thumbnail, iconPos, ImVec2(iconPos.x + TILE_ICON, iconPos.y + TILE_ICON));
    } else {
        Icons::asset(draw, iconPos, TILE_ICON, entry.kind);
    }

    // The name stays on one line and shortens from the end; the tooltip has it in full.
    const ImVec2 textMin(pos.x + 4.0f, pos.y + TILE_ICON + 16.0f);
    const ImVec2 textMax(max.x - 4.0f, max.y - 4.0f);
    const float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;
    const float textX = textWidth < textMax.x - textMin.x ? pos.x + (TILE_WIDTH - textWidth) * 0.5f : textMin.x;
    ImGui::RenderTextEllipsis(draw, ImVec2(textX, textMin.y), textMax, textMax.x,
                              entry.name.c_str(), nullptr, nullptr);

    if (hovered && textWidth >= textMax.x - textMin.x) {
        Widgets::tooltip(entry.name.c_str());
    }

    ImGui::PopID();
    return activated;
}

void ProjectPanel::drawGrid(EditorContext& ctx, AssetThumbnails& thumbnails) {
    if (entries.empty()) {
        Widgets::wrappedDim("This folder is empty. Drop models or textures on the window to import them.");
    }

    const float available = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const int columns = std::max(1, (int)((available + spacing) / (TILE_WIDTH + spacing)));

    const std::string needle = lowered(search);

    const Entry* activated = nullptr;
    int column = 0;
    for (const Entry& entry : entries) {
        if (!needle.empty() && lowered(entry.name).find(needle) == std::string::npos) continue;

        if (column > 0) ImGui::SameLine();
        if (drawTile(ctx, thumbnails, entry)) activated = &entry;
        column = (column + 1) % columns;
    }

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
        ctx.selectAsset("");
    }

    drawBackgroundMenu(ctx);

    if (activated) {
        activate(ctx, *activated);
    }
}
