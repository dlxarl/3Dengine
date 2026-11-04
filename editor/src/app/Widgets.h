#pragma once
#include "SceneObject.h"
#include "imgui.h"

#include <filesystem>
#include <string>

enum class AssetKind;

// Vector glyphs drawn into a square at pos. No icon font: they stay sharp at any display scale,
// and the Editor carries no extra asset.
namespace Icons {
    void play(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void pause(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void stop(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void save(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void plus(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void move(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void rotate(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void scale(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void folder(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void file(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void object(ImDrawList* draw, ImVec2 pos, float size, ObjectType type, ImU32 color);
    void asset(ImDrawList* draw, ImVec2 pos, float size, AssetKind kind);
    void light(ImDrawList* draw, ImVec2 center, float radius, ImU32 color);
    void speaker(ImDrawList* draw, ImVec2 center, float size, ImU32 color);
    void camera(ImDrawList* draw, ImVec2 center, float size, ImU32 color);
    void warning(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void error(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
    void info(ImDrawList* draw, ImVec2 pos, float size, ImU32 color);
}

using IconDrawer = void (*)(ImDrawList*, ImVec2, float, ImU32);

// The identity mark: the 4x4 Bayer matrix the renderer dithers with, as quantized pixels.
void drawBrandMark(ImDrawList* draw, ImVec2 pos, float size);

// Ordered-dither falloff, densest at the region's bottom edge. The launcher's one decorative
// element, and the same pattern the engine's PS1 mode puts on screen.
void drawDitherFalloff(ImDrawList* draw, ImVec2 min, ImVec2 max, float cell, ImU32 color);

namespace Widgets {
    // A flat icon button. Returns true when pressed. active shows it in the accent color.
    bool iconButton(const char* id, IconDrawer icon, bool active, const char* tooltip, ImVec2 size = ImVec2(28, 28));

    // The one filled button on a page: the action the page exists for.
    bool primaryButton(const char* label, ImVec2 size = ImVec2(0, 0));

    // One segment of a segmented control.
    bool segment(const char* label, bool selected, float width, bool first, bool last);

    // A left-aligned label with its control in a fixed column; pairs with propertyEnd().
    void propertyLabel(const char* label, const char* tooltip = nullptr);

    // Returns true while the value changes. X, Y and Z are tinted like the scene axes.
    bool vec3Row(const char* label, glm::vec3& value, float speed, float resetValue);

    // Starts a section; a thin title with a hairline above it. Returns false when collapsed.
    bool section(const char* title, bool defaultOpen = true);

    void tooltip(const char* text);
    void dimText(const char* text);
    void wrappedDim(const char* text);

    // A drop well for an asset. Returns true when its value changes (assetPath is updated).
    // Empty when no asset is assigned.
    bool assetSlot(const char* label, std::string& assetPath, AssetKind accepted);

    // True when an asset of this kind was dropped on the last item. Sets droppedPath.
    bool acceptAssetDrop(AssetKind accepted, std::string& droppedPath);
    void beginAssetDrag(const std::string& assetPath, AssetKind kind, const char* label);

    bool inputText(const char* id, std::string& text, ImGuiInputTextFlags flags = 0);
    bool inputTextHint(const char* id, const char* hint, std::string& text);
}

// Payload ids shared by drag sources and drop targets.
extern const char* const PAYLOAD_ASSET;
extern const char* const PAYLOAD_OBJECT;
