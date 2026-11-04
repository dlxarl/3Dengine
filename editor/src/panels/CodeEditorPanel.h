#pragma once
#include "EditorContext.h"

#include <filesystem>
#include <string>
#include <vector>

// A text editor for the project's scripts, shaders and data files: tabs, line numbers, find and
// replace, and saving. Settings can send files to another program instead.
class CodeEditorPanel {
public:
    void draw(EditorContext& ctx, bool* open);

    // Shows a file in a tab, or sends it to the chosen IDE.
    bool openFile(EditorContext& ctx, const std::filesystem::path& file, int line = 1);

    bool hasUnsavedFiles() const;
    int documentCount() const { return (int)documents.size(); }

private:
    struct Document {
        std::filesystem::path path;
        std::string text;
        std::string saved;
        int goToLine = 0;
        bool requestFocus = false;
        bool close = false;
        float scrollY = 0.0f;
        int cursorLine = 1;
        int cursorColumn = 1;
        bool dirty() const { return text != saved; }
    };

    std::vector<Document> documents;
    int active = -1;

    bool findOpen = false;
    std::string findText;
    std::string replaceText;
    bool findRequested = false;
    int fontSize = 14;

    void drawToolbar(EditorContext& ctx);
    void drawFind(EditorContext& ctx, Document& doc);
    void drawDocument(EditorContext& ctx, Document& doc);
    bool save(EditorContext& ctx, Document& doc);
};
