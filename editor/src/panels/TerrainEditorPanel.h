#pragma once
#include "EditorContext.h"

// Creates terrain and shapes it: sculpting brushes, painting with the tiles of a texture, and
// generated hills. Brushing itself happens in the Scene view; this panel holds the controls.
class TerrainEditorPanel {
public:
    void draw(EditorContext& ctx, bool* open);

private:
    int newCells = 64;
    float newCellSize = 2.0f;

    int seed = 1;
    float amplitude = 6.0f;
    float wavelength = 40.0f;
    int octaves = 4;

    void drawCreate(EditorContext& ctx);
    void drawBrush(EditorContext& ctx, TerrainObject& terrain);
    void drawPaint(EditorContext& ctx, TerrainObject& terrain);
    void drawGenerate(EditorContext& ctx, TerrainObject& terrain);
};
