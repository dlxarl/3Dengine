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

    const char* const TONE_LABELS[] = {"None", "Reinhard", "Neutral", "ACES"};
    const char* const FILTER_LABELS[] = {"Nearest", "Bilinear", "Trilinear", "Nearest, mipmapped"};
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
    drawImage(ctx);
    drawPost(ctx);
    drawEnvironment(ctx);
    ImGui::End();
}

void RenderSettingsPanel::drawRendering(EditorContext& ctx) {
    RenderSettings& settings = ctx.project.graphics;

    if (!Widgets::section("Rendering")) return;

    if (beginProperties("##rendering")) {
        // A preset loads a look in one go; nothing remembers which one it was, and every setting stays editable.
        Widgets::propertyLabel("Retro preset", "Loads the image settings of a console: PS1 or PS2 lighting, resolution, dithering and color depth. Modern puts them back. The post-processing stack is kept.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::BeginCombo("##preset", "Apply a preset...")) {
            for (const ModeOption& option : MODE_OPTIONS) {
                if (ImGui::Selectable(option.label)) {
                    const PostSettings keep = settings.post;
                    settings = RenderSettings::preset(option.mode);
                    settings.post = keep;
                    ctx.graphicsChanged();
                }
                if (ImGui::IsItemHovered()) Widgets::tooltip(option.description);
            }
            ImGui::EndCombo();
        }

        Widgets::propertyLabel("Lighting");
        if (ImGui::Checkbox("##lighting", &settings.lighting)) ctx.graphicsChanged();

        Widgets::propertyLabel("Shadows");
        ImGui::BeginDisabled(!settings.lighting);
        if (ImGui::Checkbox("##shadows", &settings.shadows)) ctx.graphicsChanged();

        Widgets::propertyLabel("Ray traced shadows", "Shadows of point and spot lights found by tracing rays in the fragment shader against the scene's still (Static) geometry. Lights with Cast shadows on get them, soft when the light has a size.");
        if (ImGui::Checkbox("##rtShadows", &settings.rayTracedShadows)) ctx.graphicsChanged();

        Widgets::propertyLabel("Ray traced sun", "Also traces the sun's shadows, which then reach as far as the geometry does instead of the shadow map's range. Costs one ray or more per pixel.");
        if (ImGui::Checkbox("##rtSun", &settings.rayTracedSun)) ctx.graphicsChanged();

        ImGui::BeginDisabled(!settings.rayTracedShadows && !settings.rayTracedSun);
        Widgets::propertyLabel("Rays per pixel", "For lights with a size: more rays give smoother soft shadows and cost more.");
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::SliderInt("##raySamples", &settings.raySamples, 1, 8)) ctx.graphicsChanged();
        ImGui::EndDisabled();
        ImGui::EndDisabled();

        ImGui::EndTable();
    }
}

void RenderSettingsPanel::drawImage(EditorContext& ctx) {
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

        Widgets::propertyLabel("Vertex Precision", "Snaps vertices to a coarse screen grid, for the wobble of early 3D.");
        int precision = (int)settings.vertexPrecision;
        if (ImGui::Combo("##precision", &precision, PRECISION_LABELS, (int)std::size(PRECISION_LABELS))) {
            settings.vertexPrecision = (VertexPrecision)precision;
            ctx.graphicsChanged();
        }

        ImGui::EndTable();
    }
}

namespace {
    // A row with a slider; returns true when it changed.
    bool sliderRow(const char* label, const char* tip, const char* id, float& value, float low, float high, const char* format = "%.2f") {
        Widgets::propertyLabel(label, tip);
        return ImGui::SliderFloat(id, &value, low, high, format);
    }

    bool colorRow(const char* label, const char* tip, const char* id, glm::vec3& value) {
        Widgets::propertyLabel(label, tip);
        return ImGui::ColorEdit3(id, &value.x, ImGuiColorEditFlags_Float);
    }

    // A checkbox row that switches an effect; the rows after it are disabled while it is off.
    bool switchRow(const char* label, const char* tip, const char* id, bool& on) {
        Widgets::propertyLabel(label, tip);
        return ImGui::Checkbox(id, &on);
    }
}

void RenderSettingsPanel::drawPost(EditorContext& ctx) {
    RenderSettings& s = ctx.project.graphics;
    PostSettings& p = s.post;
    bool changed = false;

    if (!Widgets::section("Post-processing")) return;
    Widgets::wrappedDim("Effects that are off cost nothing.");

    if (ImGui::CollapsingHeader("Tone Mapping & Color Grading") && beginProperties("##grade")) {
        Widgets::propertyLabel("Tone Mapping", "Maps the scene's high dynamic range to the screen. Any choice but None renders in HDR.");
        int mapper = (int)p.toneMapper;
        if (ImGui::Combo("##toneMapper", &mapper, TONE_LABELS, (int)std::size(TONE_LABELS))) {
            p.toneMapper = (ToneMapper)mapper;
            changed = true;
        }

        changed |= switchRow("Color Adjustments", "Exposure, contrast and a color filter.", "##adjust", p.colorAdjustments);
        ImGui::BeginDisabled(!p.colorAdjustments);
        changed |= sliderRow("  Exposure", "In stops: each one doubles the light.", "##exposure", p.exposure, -3.0f, 3.0f);
        changed |= sliderRow("  Contrast", "Around mid grey.", "##contrast", p.contrast, -1.0f, 1.0f);
        changed |= colorRow("  Color Filter", "Multiplies the whole image.", "##filter", p.colorFilter);
        ImGui::EndDisabled();
        changed |= sliderRow("Saturation", "Strength of color: 0 is grey, 1 leaves it as it is.", "##saturation", s.saturation, 0.0f, 2.0f);

        changed |= switchRow("White Balance", "Warms or cools the image.", "##wb", p.whiteBalance);
        ImGui::BeginDisabled(!p.whiteBalance);
        changed |= sliderRow("  Temperature", "Cool to warm.", "##temperature", p.temperature, -1.0f, 1.0f);
        changed |= sliderRow("  Tint", "Green to magenta.", "##tint", p.tint, -1.0f, 1.0f);
        ImGui::EndDisabled();

        changed |= switchRow("Split Toning", "Tints the shadows and the highlights separately. Grey leaves them alone.", "##split", p.splitToning);
        ImGui::BeginDisabled(!p.splitToning);
        changed |= colorRow("  Shadows", nullptr, "##shadowsTint", p.shadowsTint);
        changed |= colorRow("  Highlights", nullptr, "##highlightsTint", p.highlightsTint);
        changed |= sliderRow("  Balance", "Towards the shadows or the highlights.", "##balance", p.splitBalance, -1.0f, 1.0f);
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Camera & Optics") && beginProperties("##optics")) {
        changed |= switchRow("Bloom", "Bright areas glow. Renders in HDR.", "##bloom", p.bloom);
        ImGui::BeginDisabled(!p.bloom);
        changed |= sliderRow("  Threshold", "Brightness where the glow starts.", "##bloomThreshold", p.bloomThreshold, 0.0f, 4.0f);
        changed |= sliderRow("  Soft Knee", "Eases the threshold in.", "##bloomKnee", p.bloomSoftKnee, 0.0f, 1.0f);
        changed |= sliderRow("  Intensity", nullptr, "##bloomIntensity", p.bloomIntensity, 0.0f, 3.0f);
        changed |= sliderRow("  Scatter", "How far the glow spreads.", "##bloomScatter", p.bloomScatter, 0.0f, 1.0f);
        ImGui::EndDisabled();

        changed |= switchRow("Depth of Field", "Blurs what is nearer or farther than the focus.", "##dof", p.depthOfField);
        ImGui::BeginDisabled(!p.depthOfField);
        changed |= sliderRow("  Focal Distance", "Metres to the sharpest point.", "##focalDistance", p.focalDistance, 0.5f, 100.0f, "%.1f");
        changed |= sliderRow("  Focal Range", "Metres that stay sharp around it.", "##focalRange", p.focalRange, 0.5f, 100.0f, "%.1f");
        changed |= sliderRow("  Max Blur", "Largest blur, in pixels.", "##maxBlur", p.maxBlurRadius, 0.5f, 16.0f, "%.1f");
        ImGui::EndDisabled();

        changed |= sliderRow("Vignette", "Darkens the image towards its corners.", "##vignette", s.vignette, 0.0f, 1.0f);
        ImGui::BeginDisabled(s.vignette <= 0.0f);
        changed |= sliderRow("  Smoothness", "How softly it falls off.", "##vignetteSmoothness", p.vignetteSmoothness, 0.05f, 1.0f);
        changed |= sliderRow("  Roundness", "0 follows the screen's shape, 1 is a circle.", "##vignetteRoundness", p.vignetteRoundness, 0.0f, 1.0f);
        ImGui::EndDisabled();

        changed |= switchRow("Motion Blur", "Smears the image along the camera's movement.", "##motionBlur", s.motionBlur);
        ImGui::BeginDisabled(!s.motionBlur);
        changed |= sliderRow("  Strength", nullptr, "##motionBlurStrength", p.motionBlurStrength, 0.0f, 3.0f);
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Lens Artifacts") && beginProperties("##lens")) {
        changed |= sliderRow("Chromatic Aberration", "Color fringing towards the screen edges.", "##chromatic", s.chromaticAberration, 0.0f, 1.0f);
        changed |= switchRow("Lens Distortion", "Barrel or pincushion warping.", "##distortionOn", p.lensDistortion);
        ImGui::BeginDisabled(!p.lensDistortion);
        changed |= sliderRow("  Amount", "Negative is barrel, positive pincushion.", "##distortion", p.distortion, -1.0f, 1.0f);
        ImGui::EndDisabled();
        changed |= sliderRow("Film Grain", "Animated film noise.", "##grain", s.filmGrain, 0.0f, 1.0f);
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Screen Space Lighting") && beginProperties("##ssao")) {
        changed |= switchRow("SSAO", "Soft shadowing in creases and corners, from the depth and normals.", "##ssaoOn", p.ssao);
        ImGui::BeginDisabled(!p.ssao);
        changed |= sliderRow("  Radius", "Metres around a point that can shade it.", "##ssaoRadius", p.ssaoRadius, 0.1f, 3.0f);
        changed |= sliderRow("  Bias", "Avoids self-shadowing on flat surfaces.", "##ssaoBias", p.ssaoBias, 0.0f, 0.2f, "%.3f");
        changed |= sliderRow("  Intensity", nullptr, "##ssaoIntensity", p.ssaoIntensity, 0.0f, 2.0f);
        Widgets::propertyLabel("  Samples");
        int quality = p.ssaoSamples >= 32 ? 1 : 0;
        const char* const labels[] = {"16", "32"};
        if (ImGui::Combo("##ssaoSamples", &quality, labels, 2)) {
            p.ssaoSamples = quality ? 32 : 16;
            changed = true;
        }
        ImGui::EndDisabled();
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Anti-Aliasing") && beginProperties("##aa")) {
        changed |= switchRow("FXAA", "Smooths jagged edges in the finished image.", "##fxaa", p.fxaa);
        ImGui::EndTable();
    }

    if (ImGui::CollapsingHeader("Retro / Stylized") && beginProperties("##retro")) {
        Widgets::propertyLabel("Color Quantization", "Bits of color for all three channels together.");
        const char* colorPreview = "Custom";
        for (const ColorOption& option : COLOR_OPTIONS) {
            if (option.bits == s.colorBits) colorPreview = option.label;
        }
        if (ImGui::BeginCombo("##colorBits", colorPreview)) {
            for (const ColorOption& option : COLOR_OPTIONS) {
                if (ImGui::Selectable(option.label, option.bits == s.colorBits)) {
                    s.colorBits = option.bits;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }

        Widgets::propertyLabel("Dithering", "Trades color banding for a fine pattern. Needs Color Quantization.");
        const bool quantized = s.colorBits < 8;
        ImGui::BeginDisabled(!quantized);
        bool dithering = s.dithering && quantized;
        if (ImGui::Checkbox("##dithering", &dithering)) {
            s.dithering = dithering;
            changed = true;
        }
        ImGui::EndDisabled();

        changed |= sliderRow("Scanlines", "Simulates cathode ray tube horizontal beam lines.", "##scanlines", s.scanlines, 0.0f, 1.0f);
        changed |= sliderRow("CRT Curvature", "Bends the screen like an authentic curved glass monitor.", "##crtCurvature", s.crtCurvature, 0.0f, 1.0f);
        changed |= sliderRow("Phosphor Glow", "Simulates CRT phosphor luminescence bloom on bright areas.", "##phosphor", s.bloom, 0.0f, 1.0f);
        ImGui::EndTable();
    }

    if (changed) ctx.graphicsChanged();
}

void RenderSettingsPanel::drawEnvironment(EditorContext& ctx) {
    SceneGraph& scene = ctx.scene();

    if (!Widgets::section("Scene environment")) return;

    if (beginProperties("##environment")) {
        Widgets::propertyLabel("Background");
        if (ImGui::ColorEdit3("##background", &scene.clearColor.x)) {
            ctx.document.markDirty();
        }

        Widgets::propertyLabel("Gradient sky", "Paints a sky behind the scene from the background color and the light: "
                                               "deeper overhead, with clouds, the sun or moon, and stars at night.");
        if (ImGui::Checkbox("##gradientSky", &scene.gradientSky)) {
            ctx.document.markDirty();
        }

        Widgets::propertyLabel("Ambient", "Light that reaches everywhere, with a directional light.");
        if (ImGui::ColorEdit3("##ambient", &scene.ambientColor.x)) {
            ctx.document.markDirty();
        }

        Widgets::propertyLabel("Fog start", "Distance where fog begins, towards the background color.");
        if (ImGui::DragFloat("##fogStart", &scene.fogStart, 0.5f, 0.0f, 1000.0f, "%.1f")) {
            ctx.document.markDirty();
        }

        Widgets::propertyLabel("Fog end", "Distance where everything is fog. 0 turns fog off.");
        if (ImGui::DragFloat("##fogEnd", &scene.fogEnd, 0.5f, 0.0f, 1000.0f, "%.1f")) {
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
