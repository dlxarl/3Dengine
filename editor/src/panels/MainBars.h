#pragma once
#include "EditorContext.h"

// Which panels are open, as toggled from the Window menu.
struct PanelVisibility {
    bool hierarchy = true;
    bool inspector = true;
    bool project = true;
    bool console = true;
    bool renderSettings = true;
};

// The application menu, the toolbar under it and the status bar along the bottom.
class MainBars {
public:
    void draw(EditorContext& ctx, PanelVisibility& panels);

    bool takeLayoutReset();

private:
    bool resetLayout = false;

    void drawMenuBar(EditorContext& ctx, PanelVisibility& panels);
    void drawToolbar(EditorContext& ctx);
    void drawStatusBar(EditorContext& ctx);
    void drawOpenSceneItems(EditorContext& ctx);
    void handleShortcuts(EditorContext& ctx);
};
