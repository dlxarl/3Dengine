#pragma once
#include "imgui.h"

// The Editor's visual language, in one place. Graphite surfaces separated by hairlines and one
// teal accent, reserved for what is active or selected. Warnings and errors have their own hues
// so that the accent never has to mean "problem".
namespace Theme {
    constexpr ImU32 rgb(unsigned hex, unsigned alpha = 255) {
        return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, alpha);
    }

    constexpr ImU32 Sunken   = rgb(0x1C1E21);  // fields, wells, the launcher sidebar
    constexpr ImU32 Panel    = rgb(0x25272B);  // panel bodies
    constexpr ImU32 Raised   = rgb(0x2E3136);  // headers, tabs, buttons
    constexpr ImU32 Hover    = rgb(0x383B41);
    constexpr ImU32 Line     = rgb(0x3A3D43);  // hairlines between regions
    constexpr ImU32 Text     = rgb(0xD8DADD);
    constexpr ImU32 TextDim  = rgb(0x8E939B);
    constexpr ImU32 Accent   = rgb(0x6FBFAE);
    constexpr ImU32 AccentDim = rgb(0x3F6C63);
    constexpr ImU32 OnAccent = rgb(0x14211E);  // text on accent
    constexpr ImU32 Warning  = rgb(0xD9B15C);
    constexpr ImU32 Error    = rgb(0xD9736B);

    // Axis tints, muted to sit with the graphite.
    constexpr ImU32 AxisX = rgb(0xC9615A);
    constexpr ImU32 AxisY = rgb(0x7FAF5C);
    constexpr ImU32 AxisZ = rgb(0x5C8FC9);

    constexpr float PanelPadding = 10.0f;
    constexpr float ToolbarHeight = 40.0f;

    inline ImVec4 color(ImU32 value) {
        return ImGui::ColorConvertU32ToFloat4(value);
    }

    void apply();
}
