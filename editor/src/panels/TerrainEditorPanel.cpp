#include "TerrainEditorPanel.h"
#include "AssetImporter.h"
#include "Theme.h"
#include "Widgets.h"

#include <algorithm>

void TerrainEditorPanel::drawCreate(EditorContext& ctx) {
    if (!Widgets::section("Terrain")) return;

    static const int CELL_COUNTS[] = {16, 32, 64, 128, 256};
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::BeginCombo("Cells per side", std::to_string(newCells).c_str())) {
        for (int count : CELL_COUNTS) {
            if (ImGui::Selectable(std::to_string(count).c_str(), count == newCells)) newCells = count;
        }
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(120.0f);
    ImGui::DragFloat("Cell size (m)", &newCellSize, 0.05f, 0.25f, 16.0f, "%.2f");
    Widgets::dimText((std::to_string((int)(newCells * newCellSize)) + " m across").c_str());

    if (ImGui::Button("New terrain")) {
        if (auto* terrain = dynamic_cast<TerrainObject*>(ctx.createObject(ObjectType::Terrain))) {
            terrain->reset(newCells, newCellSize);
            terrain->generate(1, 0.0f, 40.0f, 1);
            ctx.terrainBrush.enabled = true;
        }
    }

    // The terrains of the scene, to pick one to work on.
    bool any = false;
    for (const auto& shape : ctx.scene().getShapes()) {
        if (shape->getType() != ObjectType::Terrain) continue;
        if (!any) ImGui::Separator();
        any = true;
        if (ImGui::Selectable(shape->name.c_str(), ctx.selectedObject == shape->id)) ctx.select(shape->id);
    }
}

void TerrainEditorPanel::drawBrush(EditorContext& ctx, TerrainObject& terrain) {
    if (!Widgets::section("Brush")) return;
    auto& brush = ctx.terrainBrush;

    ImGui::Checkbox("Brush in the Scene view", &brush.enabled);
    if (ImGui::IsItemHovered()) Widgets::tooltip("Left mouse button shapes the terrain. Turn it off to select and move objects again.");

    struct Mode {
        TerrainObject::Brush mode;
        const char* label;
    };
    static const Mode MODES[] = {{TerrainObject::Brush::Raise, "Raise"}, {TerrainObject::Brush::Lower, "Lower"},
                                 {TerrainObject::Brush::Smooth, "Smooth"}, {TerrainObject::Brush::Flatten, "Flatten"},
                                 {TerrainObject::Brush::Paint, "Paint"}};
    for (const Mode& option : MODES) {
        if (ImGui::RadioButton(option.label, brush.mode == option.mode)) brush.mode = option.mode;
        ImGui::SameLine();
    }
    ImGui::NewLine();

    ImGui::SliderFloat("Radius", &brush.radius, 0.5f, std::max(2.0f, terrain.size() * 0.25f), "%.1f m");
    ImGui::SliderFloat("Strength", &brush.strength, 0.1f, 20.0f, "%.1f");
    ImGui::SliderFloat("Softness", &brush.falloff, 0.0f, 1.0f, "%.2f");
    if (brush.mode == TerrainObject::Brush::Flatten) Widgets::dimText("Flattens to the height where the stroke starts.");
}

void TerrainEditorPanel::drawPaint(EditorContext& ctx, TerrainObject& terrain) {
    if (!Widgets::section("Painting")) return;

    std::string texture;
    for (const auto& t : terrain.getTextures()) {
        if (t->type == "texture_albedo") texture = t->assetPath;
    }
    ImGui::TextUnformatted("Texture");
    if (Widgets::assetSlot("terrainTexture", texture, AssetKind::Texture)) {
        ctx.setObjectTexture(terrain.id, texture);
    }

    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragInt("Columns", &terrain.atlasColumns, 0.1f, 1, 64)) terrain.markChanged(), ctx.document.markDirty();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60.0f);
    if (ImGui::DragInt("Rows", &terrain.atlasRows, 0.1f, 1, 64)) terrain.markChanged(), ctx.document.markDirty();
    if (terrain.atlasColumns * terrain.atlasRows <= 1) {
        Widgets::dimText("A texture of many tiles (columns by rows) lets you paint with each of them.");
        return;
    }

    // The texture itself, cut into its tiles; the chosen one is outlined.
    std::shared_ptr<Texture> picture = texture.empty() ? nullptr : ctx.assets.getTexture(texture);
    const float width = std::min(ImGui::GetContentRegionAvail().x, 360.0f);
    const float height = width * terrain.atlasRows / terrain.atlasColumns;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##tiles", ImVec2(width, height));
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (picture && picture->loaded) {
        draw->AddImage((ImTextureID)(intptr_t)picture->ID, origin, ImVec2(origin.x + width, origin.y + height));
    } else {
        draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Theme::Sunken);
    }
    const float tileW = width / terrain.atlasColumns, tileH = height / terrain.atlasRows;
    if (ImGui::IsItemClicked()) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const int column = std::clamp((int)((mouse.x - origin.x) / tileW), 0, terrain.atlasColumns - 1);
        const int row = std::clamp((int)((mouse.y - origin.y) / tileH), 0, terrain.atlasRows - 1);
        ctx.terrainBrush.tile = row * terrain.atlasColumns + column;
        ctx.terrainBrush.mode = TerrainObject::Brush::Paint;
    }
    const int tile = std::clamp(ctx.terrainBrush.tile, 0, terrain.atlasColumns * terrain.atlasRows - 1);
    const ImVec2 corner(origin.x + (tile % terrain.atlasColumns) * tileW, origin.y + (tile / terrain.atlasColumns) * tileH);
    draw->AddRect(corner, ImVec2(corner.x + tileW, corner.y + tileH), Theme::Accent, 0.0f, 0, 2.0f);
}

void TerrainEditorPanel::drawGenerate(EditorContext& ctx, TerrainObject& terrain) {
    if (!Widgets::section("Hills")) return;

    ImGui::SliderInt("Seed", &seed, 1, 999);
    ImGui::SliderFloat("Height", &amplitude, 0.5f, 60.0f, "%.1f m");
    ImGui::SliderFloat("Wavelength", &wavelength, 4.0f, 200.0f, "%.0f m");
    ImGui::SliderInt("Detail", &octaves, 1, 8);
    if (ImGui::Button("Generate hills")) {
        terrain.generate((uint32_t)seed, amplitude, wavelength, octaves);
        ctx.document.markDirty();
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten all")) {
        std::fill(terrain.heights.begin(), terrain.heights.end(), 0.0f);
        terrain.markChanged();
        ctx.document.markDirty();
    }
}

void TerrainEditorPanel::draw(EditorContext& ctx, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(380.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Terrain Editor", open)) {
        ctx.terrainBrush.enabled = false;
        ImGui::End();
        return;
    }

    drawCreate(ctx);
    if (auto* terrain = ctx.scene().findAs<TerrainObject>(ctx.selectedObject)) {
        drawBrush(ctx, *terrain);
        drawPaint(ctx, *terrain);
        drawGenerate(ctx, *terrain);
    } else {
        ctx.terrainBrush.enabled = false;
        ImGui::Spacing();
        Widgets::dimText("Create a terrain, or select one in the Hierarchy, to start shaping it.");
    }

    ImGui::End();
}
