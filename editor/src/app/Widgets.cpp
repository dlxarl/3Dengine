#include "Widgets.h"
#include "AssetImporter.h"
#include "Theme.h"

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include <cmath>
#include <cstring>
#include <filesystem>

const char* const PAYLOAD_ASSET = "ENGINE_ASSET";
const char* const PAYLOAD_OBJECT = "ENGINE_OBJECT";

namespace {
    // Lets glyph coordinates be written in a 16-unit box.
    struct Grid {
        ImVec2 origin;
        float unit;
        ImVec2 at(float x, float y) const { return ImVec2(origin.x + x * unit, origin.y + y * unit); }
        float len(float value) const { return value * unit; }
    };

    Grid gridFor(ImVec2 pos, float size) {
        return Grid{pos, size / 16.0f};
    }

    constexpr float BAYER_4X4[4][4] = {
        { 0.0f,  8.0f,  2.0f, 10.0f},
        {12.0f,  4.0f, 14.0f,  6.0f},
        { 3.0f, 11.0f,  1.0f,  9.0f},
        {15.0f,  7.0f, 13.0f,  5.0f},
    };
}

void Icons::play(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddTriangleFilled(g.at(4.5f, 3), g.at(13, 8), g.at(4.5f, 13), color);
}

void Icons::pause(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddRectFilled(g.at(4, 3), g.at(7, 13), color);
    draw->AddRectFilled(g.at(9, 3), g.at(12, 13), color);
}

void Icons::stop(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddRectFilled(g.at(3.5f, 3.5f), g.at(12.5f, 12.5f), color);
}

void Icons::save(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddRect(g.at(2.5f, 2.5f), g.at(13.5f, 13.5f), color, 0.0f, 0, g.len(1.2f));
    draw->AddRectFilled(g.at(5, 2.5f), g.at(10.5f, 6.5f), color);
    draw->AddRect(g.at(5, 9), g.at(11, 13.5f), color, 0.0f, 0, g.len(1.0f));
}

void Icons::plus(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddLine(g.at(8, 3), g.at(8, 13), color, g.len(1.5f));
    draw->AddLine(g.at(3, 8), g.at(13, 8), color, g.len(1.5f));
}

void Icons::move(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    const float t = g.len(1.3f);
    draw->AddLine(g.at(8, 2.5f), g.at(8, 13.5f), color, t);
    draw->AddLine(g.at(2.5f, 8), g.at(13.5f, 8), color, t);
    draw->AddTriangleFilled(g.at(8, 1), g.at(10, 4), g.at(6, 4), color);
    draw->AddTriangleFilled(g.at(8, 15), g.at(6, 12), g.at(10, 12), color);
    draw->AddTriangleFilled(g.at(1, 8), g.at(4, 6), g.at(4, 10), color);
    draw->AddTriangleFilled(g.at(15, 8), g.at(12, 10), g.at(12, 6), color);
}

void Icons::rotate(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->PathArcTo(g.at(8, 8), g.len(5.2f), 0.5f, 5.2f, 24);
    draw->PathStroke(color, 0, g.len(1.4f));
    draw->AddTriangleFilled(g.at(11, 1.5f), g.at(14, 4.5f), g.at(9.6f, 5.2f), color);
}

void Icons::scale(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    const float t = g.len(1.3f);
    draw->AddRect(g.at(2.5f, 7.5f), g.at(8.5f, 13.5f), color, 0.0f, 0, t);
    draw->AddLine(g.at(7.5f, 8.5f), g.at(13, 3), color, t);
    draw->AddLine(g.at(9, 2.8f), g.at(13.2f, 2.8f), color, t);
    draw->AddLine(g.at(13.2f, 2.8f), g.at(13.2f, 7), color, t);
}

void Icons::folder(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddRectFilled(g.at(1, 3), g.at(7, 5.5f), color);
    draw->AddRectFilled(g.at(1, 4.5f), g.at(15, 13), color);
}

void Icons::file(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    const float t = g.len(1.2f);
    draw->PathLineTo(g.at(3.5f, 1.5f));
    draw->PathLineTo(g.at(9.5f, 1.5f));
    draw->PathLineTo(g.at(12.5f, 4.5f));
    draw->PathLineTo(g.at(12.5f, 14.5f));
    draw->PathLineTo(g.at(3.5f, 14.5f));
    draw->PathStroke(color, ImDrawFlags_Closed, t);
}

void Icons::light(ImDrawList* draw, ImVec2 center, float radius, ImU32 color) {
    draw->AddCircleFilled(center, radius * 0.45f, color, 16);
    for (int i = 0; i < 8; i++) {
        const float angle = i * 3.14159265f / 4.0f;
        const ImVec2 direction(std::cos(angle), std::sin(angle));
        draw->AddLine(ImVec2(center.x + direction.x * radius * 0.65f, center.y + direction.y * radius * 0.65f),
                      ImVec2(center.x + direction.x * radius, center.y + direction.y * radius),
                      color, std::max(1.0f, radius * 0.14f));
    }
}

void Icons::speaker(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    const Grid g = gridFor(ImVec2(center.x - size / 2, center.y - size / 2), size);
    const float t = g.len(1.1f);
    draw->PathLineTo(g.at(2, 6)); draw->PathLineTo(g.at(5, 6)); draw->PathLineTo(g.at(9, 2.5f));
    draw->PathLineTo(g.at(9, 13.5f)); draw->PathLineTo(g.at(5, 10)); draw->PathLineTo(g.at(2, 10));
    draw->PathStroke(color, ImDrawFlags_Closed, t);
    draw->PathArcTo(g.at(9, 8), g.len(3.2f), -0.9f, 0.9f, 8);
    draw->PathStroke(color, 0, t);
    draw->PathArcTo(g.at(9, 8), g.len(5.6f), -0.9f, 0.9f, 10);
    draw->PathStroke(color, 0, t);
}

void Icons::camera(ImDrawList* draw, ImVec2 center, float size, ImU32 color) {
    const Grid g = gridFor(ImVec2(center.x - size * 0.5f, center.y - size * 0.5f), size);
    draw->AddRectFilled(g.at(1.5f, 4.5f), g.at(10.5f, 11.5f), color, g.len(1.0f));
    draw->AddTriangleFilled(g.at(11, 8), g.at(14.5f, 5), g.at(14.5f, 11), color);
}

void Icons::warning(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddTriangleFilled(g.at(8, 2), g.at(15, 14), g.at(1, 14), color);
    draw->AddRectFilled(g.at(7.3f, 6), g.at(8.7f, 10.5f), Theme::Sunken);
    draw->AddRectFilled(g.at(7.3f, 11.3f), g.at(8.7f, 12.7f), Theme::Sunken);
}

void Icons::error(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddCircleFilled(g.at(8, 8), g.len(6.5f), color, 20);
    draw->AddLine(g.at(5.5f, 5.5f), g.at(10.5f, 10.5f), Theme::Sunken, g.len(1.6f));
    draw->AddLine(g.at(10.5f, 5.5f), g.at(5.5f, 10.5f), Theme::Sunken, g.len(1.6f));
}

void Icons::info(ImDrawList* draw, ImVec2 pos, float size, ImU32 color) {
    const Grid g = gridFor(pos, size);
    draw->AddCircleFilled(g.at(8, 8), g.len(6.5f), color, 20);
    draw->AddRectFilled(g.at(7.3f, 7), g.at(8.7f, 11.5f), Theme::Sunken);
    draw->AddRectFilled(g.at(7.3f, 4.5f), g.at(8.7f, 5.9f), Theme::Sunken);
}

void Icons::object(ImDrawList* draw, ImVec2 pos, float size, ObjectType type, ImU32 color) {
    const Grid g = gridFor(pos, size);
    const float t = g.len(1.2f);
    switch (type) {
        case ObjectType::Light:
            light(draw, g.at(8, 8), g.len(6.5f), color);
            break;
        case ObjectType::Camera:
            camera(draw, g.at(8, 8), size, color);
            break;
        case ObjectType::AudioSource:
            speaker(draw, g.at(8, 8), size, color);
            break;
        case ObjectType::Ui:
            draw->AddRect(g.at(2.5f, 3.5f), g.at(13.5f, 12.5f), color, g.len(1.2f), 0, g.len(0.9f));
            draw->AddLine(g.at(4.5f, 6.5f), g.at(9.5f, 6.5f), color, g.len(0.9f));
            draw->AddLine(g.at(4.5f, 9.5f), g.at(7.5f, 9.5f), color, g.len(0.9f));
            break;
        case ObjectType::Empty:
            draw->AddRect(g.at(3.5f, 3.5f), g.at(12.5f, 12.5f), color, 0.0f, 0, g.len(0.8f));
            break;
        case ObjectType::Plane:
            draw->PathLineTo(g.at(1.5f, 11));
            draw->PathLineTo(g.at(5, 5));
            draw->PathLineTo(g.at(14.5f, 5));
            draw->PathLineTo(g.at(11, 11));
            draw->PathStroke(color, ImDrawFlags_Closed, t);
            break;
        case ObjectType::Sphere:
            draw->AddCircle(g.at(8, 8), g.len(6), color, 24, t);
            draw->AddEllipse(g.at(8, 8), ImVec2(g.len(6), g.len(2.2f)), color, 0.0f, 24, g.len(0.8f));
            break;
        case ObjectType::Cylinder:
            draw->AddEllipse(g.at(8, 4), ImVec2(g.len(4.5f), g.len(1.8f)), color, 0.0f, 20, t);
            draw->AddLine(g.at(3.5f, 4), g.at(3.5f, 12), color, t);
            draw->AddLine(g.at(12.5f, 4), g.at(12.5f, 12), color, t);
            draw->PathArcTo(g.at(8, 12), g.len(4.5f), 0.0f, 3.14159265f, 12);
            draw->PathStroke(color, 0, t);
            break;
        case ObjectType::Mesh:
            draw->AddTriangle(g.at(8, 2), g.at(14, 13), g.at(2, 13), color, t);
            draw->AddLine(g.at(8, 2), g.at(8, 9), color, g.len(0.8f));
            draw->AddLine(g.at(8, 9), g.at(14, 13), color, g.len(0.8f));
            draw->AddLine(g.at(8, 9), g.at(2, 13), color, g.len(0.8f));
            break;
        case ObjectType::Particles:
            for (int i = 0; i < 7; i++) {
                const float angle = float(i) * 0.9f;
                draw->AddCircleFilled(g.at(8 + 3.5f * std::cos(angle) * (0.4f + 0.12f * i), 13 - 1.6f * i), g.len(0.9f + 0.12f * i), color, 10);
            }
            break;
        case ObjectType::Fire:
            draw->PathLineTo(g.at(8, 1.5f));
            draw->PathBezierCubicCurveTo(g.at(13, 6), g.at(13, 13), g.at(8, 14.5f));
            draw->PathBezierCubicCurveTo(g.at(3, 13), g.at(3, 7), g.at(8, 1.5f));
            draw->PathStroke(color, ImDrawFlags_Closed, t);
            draw->AddCircle(g.at(8, 11), g.len(2.2f), color, 12, g.len(0.8f));
            break;
        case ObjectType::Water:
            for (int row = 0; row < 3; row++) {
                const float y = 5.0f + 3.4f * row;
                draw->PathLineTo(g.at(1.5f, y));
                draw->PathBezierCubicCurveTo(g.at(4, y - 2.4f), g.at(6, y - 2.4f), g.at(8, y));
                draw->PathBezierCubicCurveTo(g.at(10, y + 2.4f), g.at(12, y + 2.4f), g.at(14.5f, y));
                draw->PathStroke(color, 0, g.len(0.9f));
            }
            break;
        case ObjectType::Cube:
        default:
            draw->PathLineTo(g.at(8, 1.5f));
            draw->PathLineTo(g.at(14, 4.8f));
            draw->PathLineTo(g.at(14, 11.2f));
            draw->PathLineTo(g.at(8, 14.5f));
            draw->PathLineTo(g.at(2, 11.2f));
            draw->PathLineTo(g.at(2, 4.8f));
            draw->PathStroke(color, ImDrawFlags_Closed, t);
            draw->AddLine(g.at(2, 4.8f), g.at(8, 8), color, t);
            draw->AddLine(g.at(14, 4.8f), g.at(8, 8), color, t);
            draw->AddLine(g.at(8, 8), g.at(8, 14.5f), color, t);
            break;
    }
}

void Icons::asset(ImDrawList* draw, ImVec2 pos, float size, AssetKind kind) {
    const Grid g = gridFor(pos, size);
    switch (kind) {
        case AssetKind::Folder:
            folder(draw, pos, size, Theme::rgb(0x8E939B));
            break;
        case AssetKind::Scene:
            file(draw, pos, size, Theme::TextDim);
            draw->AddTriangleFilled(g.at(6, 6.5f), g.at(10.5f, 9), g.at(6, 11.5f), Theme::Accent);
            break;
        case AssetKind::Model:
            object(draw, pos, size, ObjectType::Cube, Theme::TextDim);
            break;
        case AssetKind::Texture:
            draw->AddRect(g.at(2, 3), g.at(14, 13), Theme::TextDim, 0.0f, 0, g.len(1.2f));
            draw->AddTriangleFilled(g.at(3.5f, 11.5f), g.at(7.5f, 6.5f), g.at(11, 11.5f), Theme::TextDim);
            break;
        default:
            file(draw, pos, size, Theme::TextDim);
            break;
    }
}

void drawBrandMark(ImDrawList* draw, ImVec2 pos, float size) {
    const float cell = size / 4.0f;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            const float level = BAYER_4X4[y][x] / 15.0f;
            const ImVec2 min(pos.x + x * cell, pos.y + y * cell);
            const ImVec2 max(min.x + cell - 1.0f, min.y + cell - 1.0f);
            const unsigned alpha = (unsigned)(60.0f + level * 195.0f);
            draw->AddRectFilled(min, max, (Theme::Accent & 0x00FFFFFF) | (alpha << 24));
        }
    }
}

void drawDitherFalloff(ImDrawList* draw, ImVec2 min, ImVec2 max, float cell, ImU32 color) {
    const int rows = (int)((max.y - min.y) / cell);
    const int columns = (int)((max.x - min.x) / cell);
    for (int y = 0; y < rows; y++) {
        const float density = (float)(y + 1) / (float)rows;
        for (int x = 0; x < columns; x++) {
            if ((BAYER_4X4[y & 3][x & 3] + 0.5f) / 16.0f < density * density) {
                const ImVec2 at(min.x + x * cell, min.y + y * cell);
                draw->AddRectFilled(at, ImVec2(at.x + cell, at.y + cell), color);
            }
        }
    }
}

namespace Widgets {

bool iconButton(const char* id, IconDrawer icon, bool active, const char* tooltipText, ImVec2 size) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool disabled = (ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + size.x, pos.y + size.y);
    if (active) {
        draw->AddRectFilled(pos, max, Theme::rgb(0x6FBFAE, 50), 3.0f);
    } else if (held && !disabled) {
        draw->AddRectFilled(pos, max, Theme::rgb(0x41454C), 3.0f);
    } else if (hovered && !disabled) {
        draw->AddRectFilled(pos, max, Theme::Hover, 3.0f);
    }

    const float glyph = std::floor(size.y * 0.58f);
    const ImVec2 glyphPos(std::floor(pos.x + (size.x - glyph) * 0.5f), std::floor(pos.y + (size.y - glyph) * 0.5f));
    const ImU32 color = disabled ? Theme::rgb(0x5A5F67) : (active ? Theme::Accent : Theme::Text);
    icon(draw, glyphPos, glyph, color);

    if (tooltipText && hovered) {
        tooltip(tooltipText);
    }
    return pressed;
}

bool primaryButton(const char* label, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, Theme::Accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::rgb(0x82CDBD));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::rgb(0x5FAF9E));
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::OnAccent);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 6.0f));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    return pressed;
}

bool segment(const char* label, bool selected, float width, bool first, bool last) {
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight();
    const bool pressed = ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawFlags corners = ImDrawFlags_RoundCornersNone;
    if (first) corners |= ImDrawFlags_RoundCornersLeft;
    if (last) corners |= ImDrawFlags_RoundCornersRight;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + width, pos.y + height);
    const ImU32 fill = selected ? Theme::rgb(0x6FBFAE, 50) : (hovered ? Theme::Hover : Theme::Raised);
    draw->AddRectFilled(pos, max, fill, 3.0f, corners);

    const char* visibleEnd = ImGui::FindRenderedTextEnd(label);
    const ImVec2 textSize = ImGui::CalcTextSize(label, visibleEnd);
    draw->AddText(ImVec2(std::floor(pos.x + (width - textSize.x) * 0.5f), std::floor(pos.y + (height - textSize.y) * 0.5f)),
                  selected ? Theme::Accent : Theme::Text, label, visibleEnd);
    return pressed;
}

void propertyLabel(const char* label, const char* tooltipText) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(Theme::color(Theme::TextDim), "%s", label);
    if (tooltipText && ImGui::IsItemHovered()) {
        tooltip(tooltipText);
    }
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool vec3Row(const char* label, glm::vec3& value, float speed, float resetValue) {
    propertyLabel(label);

    bool changed = false;
    ImGui::PushID(label);

    const float spacing = 4.0f;
    const float width = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
    const ImU32 tints[3] = {Theme::AxisX, Theme::AxisY, Theme::AxisZ};
    const char* axes[3] = {"##x", "##y", "##z"};

    for (int axis = 0; axis < 3; axis++) {
        if (axis > 0) ImGui::SameLine(0.0f, spacing);

        ImGui::SetNextItemWidth(width);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 4.0f));
        changed |= ImGui::DragFloat(axes[axis], &value[axis], speed, 0.0f, 0.0f, "%.3f");
        ImGui::PopStyleVar();

        // A right-click puts the component back to its neutral value.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            value[axis] = resetValue;
            changed = true;
        }

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(min.x, min.y), ImVec2(min.x + 3.0f, max.y), tints[axis], 2.0f,
                                                  ImDrawFlags_RoundCornersLeft);
    }

    ImGui::PopID();
    return changed;
}

bool section(const char* title, bool defaultOpen) {
    ImGui::Spacing();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                               ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    ImGui::PushStyleColor(ImGuiCol_Header, Theme::rgb(0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Theme::rgb(0xFFFFFF, 10));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, Theme::rgb(0xFFFFFF, 16));
    const bool open = ImGui::CollapsingHeader(title, flags);
    ImGui::PopStyleColor(3);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, min.y - 1.0f), ImVec2(max.x, min.y - 1.0f), Theme::Line);
    return open;
}

void tooltip(const char* text) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 5.0f));
    ImGui::SetTooltip("%s", text);
    ImGui::PopStyleVar();
}

void dimText(const char* text) {
    ImGui::TextColored(Theme::color(Theme::TextDim), "%s", text);
}

void wrappedDim(const char* text) {
    ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

bool acceptAssetDrop(AssetKind accepted, std::string& droppedPath) {
    bool dropped = false;
    if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        if (payload && payload->IsDataType(PAYLOAD_ASSET)) {
            const auto* data = static_cast<const char*>(payload->Data);
            const AssetKind kind = static_cast<AssetKind>(data[0]);
            if (kind == accepted) {
                if (ImGui::AcceptDragDropPayload(PAYLOAD_ASSET)) {
                    droppedPath.assign(data + 1);
                    dropped = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
    return dropped;
}

void beginAssetDrag(const std::string& assetPath, AssetKind kind, const char* label) {
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoHoldToOpenOthers)) {
        char payload[1024];
        payload[0] = static_cast<char>(kind);
        std::strncpy(payload + 1, assetPath.c_str(), sizeof(payload) - 2);
        payload[sizeof(payload) - 1] = '\0';
        ImGui::SetDragDropPayload(PAYLOAD_ASSET, payload, std::strlen(payload + 1) + 2);
        ImGui::TextUnformatted(label);
        ImGui::EndDragDropSource();
    }
}

bool assetSlot(const char* label, std::string& assetPath, AssetKind accepted) {
    bool changed = false;
    ImGui::PushID(label);

    const float clearWidth = assetPath.empty() ? 0.0f : ImGui::GetFrameHeight() + 4.0f;
    const float width = ImGui::GetContentRegionAvail().x - clearWidth;

    const std::string fileName = assetPath.empty() ? std::string()
                                                   : std::filesystem::u8path(assetPath).filename().u8string();
    const char* hint = accepted == AssetKind::Texture ? "Drag a texture here"
                       : accepted == AssetKind::Audio ? "Drag a .wav sound here"
                       : accepted == AssetKind::Font ? "Inter (drag a .ttf font to change)" : "Drag an asset here";

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetFrameHeight();
    ImGui::InvisibleButton("##slot", ImVec2(width, height));
    const bool hovered = ImGui::IsItemHovered();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), Theme::Sunken, 3.0f);

    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    const bool compatible = payload && payload->IsDataType(PAYLOAD_ASSET) &&
                            static_cast<AssetKind>(static_cast<const char*>(payload->Data)[0]) == accepted;
    if (compatible) {
        draw->AddRect(pos, ImVec2(pos.x + width, pos.y + height), Theme::Accent, 3.0f);
    }

    draw->PushClipRect(pos, ImVec2(pos.x + width - 4.0f, pos.y + height), true);
    draw->AddText(ImVec2(pos.x + 8.0f, pos.y + ImGui::GetStyle().FramePadding.y),
                  fileName.empty() ? Theme::TextDim : Theme::Text, fileName.empty() ? hint : fileName.c_str());
    draw->PopClipRect();

    if (!assetPath.empty() && hovered) {
        tooltip(assetPath.c_str());
    }

    std::string dropped;
    if (acceptAssetDrop(accepted, dropped)) {
        assetPath = dropped;
        changed = true;
    }

    if (!assetPath.empty()) {
        ImGui::SameLine(0.0f, 4.0f);
        if (ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f))) {
            assetPath.clear();
            changed = true;
        }
        if (ImGui::IsItemHovered()) tooltip("Remove");
    }

    ImGui::PopID();
    return changed;
}

bool inputText(const char* id, std::string& text, ImGuiInputTextFlags flags) {
    return ImGui::InputText(id, &text, flags);
}

bool inputTextHint(const char* id, const char* hint, std::string& text) {
    return ImGui::InputTextWithHint(id, hint, &text);
}

}  // namespace Widgets
