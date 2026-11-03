#include "RenderSettingsPanel.h"
#include "AssetImporter.h"
#include "RenderSettings.h"
#include "Theme.h"
#include "Widgets.h"

#include <iterator>

namespace {
    struct ModeOption {
        RenderMode mode;
        const char* label;
        const char* description;
    };

    const ModeOption MODE_OPTIONS[] = {
        {RenderMode::Modern, "Modern", "Per-pixel lighting with shadow mapping."},
        {RenderMode::PS1, "PS1", "Per-vertex lighting and affine texture mapping. Textures warp on large polygons."},
        {RenderMode::PS2, "PS2", "Per-vertex lighting with perspective-correct textures."},
    };

    struct ResolutionOption {
        int height;
        const char* label;
    };

    const ResolutionOption RESOLUTION_OPTIONS[] = {
        {0, "Native"}, {720, "720 lines"}, {480, "480 lines"}, {448, "448 lines"}, {240, "240 lines"}, {160, "160 lines"},
    };

    struct ColorOption {
        int bits;
        const char* label;
    };

    const ColorOption COLOR_OPTIONS[] = {
        {8, "Off (24-bit)"}, {6, "18-bit"}, {5, "15-bit"}, {4, "12-bit"}, {3, "9-bit"}, {2, "6-bit"},
    };

    const char* const FILTER_LABELS[] = {"Nearest", "Bilinear", "Trilinear"};
    const char* const PRECISION_LABELS[] = {"Off", "High (480 lines)", "Medium (240 lines)", "Low (120 lines)"};

    bool beginProperties(const char* id) {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 126.0f);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }
}

void RenderSettingsPanel::draw(EditorContext& ctx, bool* open) {
    if (!ImGui::Begin("Render Settings", open)) {
        ImGui::End();
        return;
    }

    drawRendering(ctx);
    drawRetro(ctx);
    drawEnvironment(ctx);

    ImGui::End();
}

void RenderSettingsPanel::drawRendering(EditorContext& ctx) {
    RenderSettings& settings = ctx.project.graphics;

    if (!Widgets::section("Rendering")) return;

    if (beginProperties("##rendering")) {
        Widgets::propertyLabel("Rendering Mode");
        const char* preview = "";
        const char* description = "";
        for (const ModeOption& option : MODE_OPTIONS) {
            if (option.mode == settings.mode) {
                preview = option.label;
                description = option.description;
            }
        }
        if (ImGui::BeginCombo("##mode", preview)) {
            for (const ModeOption& option : MODE_OPTIONS) {
                if (ImGui::Selectable(option.label, option.mode == settings.mode)) {
                    // Choosing a mode loads its whole look. Each setting stays editable afterwards.
                    settings = RenderSettings::preset(option.mode);
                    ctx.graphicsChanged();
                }
            }
            ImGui::EndCombo();
        }

        Widgets::propertyLabel("Lighting");
        if (ImGui::Checkbox("##lighting", &settings.lighting)) ctx.graphicsChanged();

        Widgets::propertyLabel("Shadows");
        ImGui::BeginDisabled(!settings.lighting);
        if (ImGui::Checkbox("##shadows", &settings.shadows)) ctx.graphicsChanged();
        ImGui::EndDisabled();

        Widgets::propertyLabel("Motion Blur");
        if (ImGui::Checkbox("##motionBlur", &settings.motionBlur)) ctx.graphicsChanged();

        ImGui::EndTable();

        ImGui::Spacing();
        Widgets::wrappedDim(description);
    }
}

void RenderSettingsPanel::drawRetro(EditorContext& ctx) {
    RenderSettings& settings = ctx.project.graphics;

    if (!Widgets::section("Image")) return;

    if (beginProperties("##image")) {
        Widgets::propertyLabel("Internal Resolution", "Lines rendered before upscaling. Width follows the viewport's shape.");
        const char* resolutionPreview = "Custom";
        for (const ResolutionOption& option : RESOLUTION_OPTIONS) {
            if (option.height == settings.internalHeight) resolutionPreview = option.label;
        }
        if (ImGui::BeginCombo("##resolution", resolutionPreview)) {
            for (const ResolutionOption& option : RESOLUTION_OPTIONS) {
                if (ImGui::Selectable(option.label, option.height == settings.internalHeight)) {
                    settings.internalHeight = option.height;
                    ctx.graphicsChanged();
                }
            }
            ImGui::EndCombo();
        }

        Widgets::propertyLabel("Texture Filtering");
        int filter = (int)settings.textureFilter;
        if (ImGui::Combo("##filter", &filter, FILTER_LABELS, (int)std::size(FILTER_LABELS))) {
            settings.textureFilter = (TextureFilter)filter;
            ctx.graphicsChanged();
        }

        Widgets::propertyLabel("Color Quantization", "Bits of color for all three channels together.");
        const char* colorPreview = "Custom";
        for (const ColorOption& option : COLOR_OPTIONS) {
            if (option.bits == settings.colorBits) colorPreview = option.label;
        }
        if (ImGui::BeginCombo("##colorBits", colorPreview)) {
            for (const ColorOption& option : COLOR_OPTIONS) {
                if (ImGui::Selectable(option.label, option.bits == settings.colorBits)) {
                    settings.colorBits = option.bits;
                    ctx.graphicsChanged();
                }
            }
            ImGui::EndCombo();
        }

        Widgets::propertyLabel("Dithering", "Trades color banding for a fine pattern. Needs Color Quantization.");
        const bool quantized = settings.colorBits < 8;
        ImGui::BeginDisabled(!quantized);
        bool dithering = settings.dithering && quantized;
        if (ImGui::Checkbox("##dithering", &dithering)) {
            settings.dithering = dithering;
            ctx.graphicsChanged();
        }
        ImGui::EndDisabled();

        Widgets::propertyLabel("Vertex Precision", "Snaps vertices to a coarse screen grid, for the wobble of early 3D.");
        int precision = (int)settings.vertexPrecision;
        if (ImGui::Combo("##precision", &precision, PRECISION_LABELS, (int)std::size(PRECISION_LABELS))) {
            settings.vertexPrecision = (VertexPrecision)precision;
            ctx.graphicsChanged();
        }

        ImGui::EndTable();
    }
}

void RenderSettingsPanel::drawEnvironment(EditorContext& ctx) {
    SceneGraph& scene = ctx.scene();

    if (!Widgets::section("Scene environment")) return;

    if (beginProperties("##environment")) {
        Widgets::propertyLabel("Background");
        if (ImGui::ColorEdit3("##background", &scene.clearColor.x)) {
            ctx.document.markDirty();
        }

        Widgets::propertyLabel("Skybox", "An equirectangular image. Drag a texture here from the Project panel.");
        std::string skybox = scene.getSkyboxAsset();
        if (Widgets::assetSlot("skybox", skybox, AssetKind::Texture)) {
            scene.setSkybox(skybox, ctx.assets);
            ctx.document.markDirty();
        }

        ImGui::EndTable();
    }
}
