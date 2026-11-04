#pragma once
#include "AssetImporter.h"
#include "AssetThumbnails.h"
#include "EditorContext.h"

#include <filesystem>
#include <string>
#include <vector>

// The asset browser: the project's Assets folder as a folder tree and a grid of its contents.
class ProjectPanel {
public:
    explicit ProjectPanel(EditorContext& ctx);

    void draw(EditorContext& ctx, AssetThumbnails& thumbnails, bool* open);

    // Runs even while the panel is closed: a file dropped on the window must not be lost.
    void importDroppedFiles(EditorContext& ctx);

private:
    struct Entry {
        std::filesystem::path path;
        std::string name;
        std::string assetPath;
        AssetKind kind = AssetKind::Other;
    };

    struct FolderNode {
        std::filesystem::path path;
        std::string name;
        std::vector<FolderNode> children;
    };

    std::filesystem::path currentFolder;
    std::vector<Entry> entries;
    FolderNode folderTree;
    float rescanTimer = 0.0f;
    bool rescanRequested = true;

    std::string search;
    std::filesystem::path navigateTo;

    void rescan(EditorContext& ctx);
    void importFiles(EditorContext& ctx, const std::vector<std::filesystem::path>& files);
    void chooseAndImport(EditorContext& ctx);

    void drawHeader(EditorContext& ctx);
    void drawBreadcrumbs(EditorContext& ctx);
    void drawFolderNode(const FolderNode& node);
    void drawGrid(EditorContext& ctx, AssetThumbnails& thumbnails);
    bool drawTile(EditorContext& ctx, AssetThumbnails& thumbnails, const Entry& entry);
    void drawEntryMenu(EditorContext& ctx, const Entry& entry);
    void drawBackgroundMenu(EditorContext& ctx);
    void activate(EditorContext& ctx, const Entry& entry);

    void promptNewFolder(EditorContext& ctx);
    void promptNewScene(EditorContext& ctx);
    void promptNewScript(EditorContext& ctx, const ScriptBuilder::Template& source);
    void confirmTrash(EditorContext& ctx, const Entry& entry);
};
