#pragma once
#include "EditorContext.h"
#include "Widgets.h"

#include <cstdint>
#include <string>
#include <vector>

// Engine and Editor messages, with filtering by level and text.
class ConsolePanel {
public:
    void draw(EditorContext& ctx, bool* open);

private:
    bool showInfo = true;
    bool showWarnings = true;
    bool showErrors = true;
    std::string search;

    // Rebuilt only when the log or the filter changes, not on every frame.
    std::vector<ConsoleEntry> visible;
    uint64_t builtRevision = UINT64_MAX;
    std::string builtSearch;
    bool builtFlags[3] = {false, false, false};
    bool scrollToBottom = true;

    void rebuild(const EditorContext& ctx);
    void drawToolbar(EditorContext& ctx);
    bool levelToggle(const char* id, IconDrawer icon, ImU32 color, int count, bool* enabled, const char* tooltip);
};
