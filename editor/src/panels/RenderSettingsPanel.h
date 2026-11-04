#pragma once
#include "EditorContext.h"

// The project's graphics settings and the open scene's environment. Every control here
// feeds the renderer directly; there is no apply step.
class RenderSettingsPanel {
public:
    void draw(EditorContext& ctx, bool* open);

private:
    void drawRendering(EditorContext& ctx);
    void drawImage(EditorContext& ctx);
    void drawPost(EditorContext& ctx);
    void drawEnvironment(EditorContext& ctx);
};
