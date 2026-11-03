#include "Theme.h"

#include "ImGuizmo.h"

namespace {
    void applyGizmoStyle() {
        using Theme::color;
        using Theme::rgb;

        ImGuizmo::Style& style = ImGuizmo::GetStyle();
        style.TranslationLineThickness = 2.5f;
        style.RotationLineThickness = 2.0f;
        style.RotationOuterLineThickness = 2.0f;
        style.ScaleLineThickness = 2.5f;
        style.CenterCircleSize = 5.0f;

        style.Colors[ImGuizmo::DIRECTION_X] = color(Theme::AxisX);
        style.Colors[ImGuizmo::DIRECTION_Y] = color(Theme::AxisY);
        style.Colors[ImGuizmo::DIRECTION_Z] = color(Theme::AxisZ);
        style.Colors[ImGuizmo::PLANE_X] = color(rgb(0xC9615A, 110));
        style.Colors[ImGuizmo::PLANE_Y] = color(rgb(0x7FAF5C, 110));
        style.Colors[ImGuizmo::PLANE_Z] = color(rgb(0x5C8FC9, 110));
        style.Colors[ImGuizmo::SELECTION] = color(Theme::Accent);
        style.Colors[ImGuizmo::ROTATION_USING_BORDER] = color(Theme::Accent);
        style.Colors[ImGuizmo::ROTATION_USING_FILL] = color(rgb(0x6FBFAE, 80));
    }
}

void Theme::apply() {
    applyGizmoStyle();

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowPadding = ImVec2(PanelPadding, PanelPadding);
    style.FramePadding = ImVec2(8.0f, 4.0f);
    style.CellPadding = ImVec2(6.0f, 3.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 14.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 9.0f;
    style.TabBarBorderSize = 1.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    // Square panels; a slight radius only on things you press.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.PopupRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 0.0f;
    style.ScrollbarRounding = 2.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.SeparatorTextBorderSize = 1.0f;
    style.DockingSeparatorSize = 1.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = color(Text);
    c[ImGuiCol_TextDisabled]         = color(TextDim);
    c[ImGuiCol_WindowBg]             = color(Panel);
    c[ImGuiCol_ChildBg]              = color(rgb(0, 0));
    c[ImGuiCol_PopupBg]              = color(Raised);
    c[ImGuiCol_Border]               = color(Line);
    c[ImGuiCol_BorderShadow]         = color(rgb(0, 0));
    c[ImGuiCol_FrameBg]              = color(Sunken);
    c[ImGuiCol_FrameBgHovered]       = color(rgb(0x202326));
    c[ImGuiCol_FrameBgActive]        = color(rgb(0x202326));
    c[ImGuiCol_TitleBg]              = color(Sunken);
    c[ImGuiCol_TitleBgActive]        = color(Sunken);
    c[ImGuiCol_TitleBgCollapsed]     = color(Sunken);
    c[ImGuiCol_MenuBarBg]            = color(Sunken);
    c[ImGuiCol_ScrollbarBg]          = color(rgb(0, 0));
    c[ImGuiCol_ScrollbarGrab]        = color(Line);
    c[ImGuiCol_ScrollbarGrabHovered] = color(rgb(0x4A4E55));
    c[ImGuiCol_ScrollbarGrabActive]  = color(rgb(0x5A5F67));
    c[ImGuiCol_CheckMark]            = color(Accent);
    c[ImGuiCol_SliderGrab]           = color(rgb(0x5A5F67));
    c[ImGuiCol_SliderGrabActive]     = color(Accent);
    c[ImGuiCol_Button]               = color(Raised);
    c[ImGuiCol_ButtonHovered]        = color(Hover);
    c[ImGuiCol_ButtonActive]         = color(rgb(0x41454C));
    c[ImGuiCol_Header]               = color(rgb(0x6FBFAE, 46));
    c[ImGuiCol_HeaderHovered]        = color(Hover);
    c[ImGuiCol_HeaderActive]         = color(rgb(0x6FBFAE, 64));
    c[ImGuiCol_Separator]            = color(Line);
    c[ImGuiCol_SeparatorHovered]     = color(AccentDim);
    c[ImGuiCol_SeparatorActive]      = color(Accent);
    c[ImGuiCol_ResizeGrip]           = color(rgb(0, 0));
    c[ImGuiCol_ResizeGripHovered]    = color(AccentDim);
    c[ImGuiCol_ResizeGripActive]     = color(Accent);
    c[ImGuiCol_Tab]                  = color(Sunken);
    c[ImGuiCol_TabHovered]           = color(Hover);
    c[ImGuiCol_TabSelected]          = color(Panel);
    c[ImGuiCol_TabSelectedOverline]  = color(Accent);
    c[ImGuiCol_TabDimmed]            = color(Sunken);
    c[ImGuiCol_TabDimmedSelected]    = color(Panel);
    c[ImGuiCol_TabDimmedSelectedOverline] = color(rgb(0, 0));
    c[ImGuiCol_DockingPreview]       = color(rgb(0x6FBFAE, 90));
    c[ImGuiCol_DockingEmptyBg]       = color(Sunken);
    c[ImGuiCol_TextSelectedBg]       = color(rgb(0x6FBFAE, 80));
    c[ImGuiCol_DragDropTarget]       = color(Accent);
    c[ImGuiCol_NavCursor]            = color(Accent);
    c[ImGuiCol_ModalWindowDimBg]     = color(rgb(0x000000, 110));
    c[ImGuiCol_TableBorderLight]     = color(Line);
    c[ImGuiCol_TableBorderStrong]    = color(Line);
    c[ImGuiCol_TableHeaderBg]        = color(Raised);
    c[ImGuiCol_TableRowBgAlt]        = color(rgb(0xFFFFFF, 6));
}
