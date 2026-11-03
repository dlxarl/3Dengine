#include "ConsolePanel.h"
#include "Theme.h"
#include "Widgets.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {
    bool containsText(const std::string& text, const std::string& needle) {
        auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(), [](char a, char b) {
            return std::tolower((unsigned char)a) == std::tolower((unsigned char)b);
        });
        return it != text.end();
    }

    ImU32 levelColor(LogLevel level) {
        switch (level) {
            case LogLevel::Warning: return Theme::Warning;
            case LogLevel::Error:   return Theme::Error;
            default:                return Theme::TextDim;
        }
    }
}

void ConsolePanel::rebuild(const EditorContext& ctx) {
    const uint64_t revision = ctx.console.getRevision();
    const bool unchanged = revision == builtRevision && search == builtSearch && builtFlags[0] == showInfo &&
                           builtFlags[1] == showWarnings && builtFlags[2] == showErrors;
    if (unchanged) return;

    builtRevision = revision;
    builtSearch = search;
    builtFlags[0] = showInfo;
    builtFlags[1] = showWarnings;
    builtFlags[2] = showErrors;

    visible.clear();
    ctx.console.forEach([&](const ConsoleEntry& entry) {
        const bool levelShown = (entry.level == LogLevel::Info && showInfo) ||
                                (entry.level == LogLevel::Warning && showWarnings) ||
                                (entry.level == LogLevel::Error && showErrors);
        if (!levelShown) return;
        if (!search.empty() && !containsText(entry.message, search) && !containsText(entry.category, search)) return;
        visible.push_back(entry);
    });
}

bool ConsolePanel::levelToggle(const char* id, IconDrawer icon, ImU32 color, int count, bool* enabled, const char* tooltip) {
    char label[16];
    std::snprintf(label, sizeof(label), "%d", count);
    const float textWidth = ImGui::CalcTextSize(label).x;
    const float height = ImGui::GetFrameHeight();
    const float width = 26.0f + textWidth + 8.0f;

    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));
    if (pressed) *enabled = !*enabled;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 max(pos.x + width, pos.y + height);
    if (*enabled) {
        draw->AddRectFilled(pos, max, Theme::Raised, 3.0f);
    } else if (ImGui::IsItemHovered()) {
        draw->AddRectFilled(pos, max, Theme::rgb(0x2A2C30), 3.0f);
    }

    const ImU32 iconColor = *enabled ? color : Theme::rgb(0x5A5F67);
    icon(draw, ImVec2(pos.x + 7.0f, pos.y + (height - 13.0f) * 0.5f), 13.0f, iconColor);
    draw->AddText(ImVec2(pos.x + 25.0f, pos.y + ImGui::GetStyle().FramePadding.y),
                  *enabled ? Theme::Text : Theme::TextDim, label);

    if (ImGui::IsItemHovered()) Widgets::tooltip(tooltip);
    return pressed;
}

void ConsolePanel::drawToolbar(EditorContext& ctx) {
    if (ImGui::Button("Clear")) {
        ctx.console.clear();
    }

    ImGui::SameLine();
    levelToggle("##info", Icons::info, Theme::TextDim, ctx.console.count(LogLevel::Info), &showInfo, "Show messages");
    ImGui::SameLine(0.0f, 2.0f);
    levelToggle("##warnings", Icons::warning, Theme::Warning, ctx.console.count(LogLevel::Warning), &showWarnings, "Show warnings");
    ImGui::SameLine(0.0f, 2.0f);
    levelToggle("##errors", Icons::error, Theme::Error, ctx.console.count(LogLevel::Error), &showErrors, "Show errors");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    Widgets::inputTextHint("##search", "Search messages", search);
}

void ConsolePanel::draw(EditorContext& ctx, bool* open) {
    if (!ImGui::Begin("Console", open)) {
        ImGui::End();
        return;
    }

    drawToolbar(ctx);

    const size_t before = visible.size();
    rebuild(ctx);
    if (visible.size() != before) scrollToBottom = true;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Theme::Sunken);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_AlwaysUseWindowPadding);

    // Only follow new messages when the view is already at the end; a person scrolled
    // back to read something should not be dragged away from it.
    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f;

    if (visible.empty()) {
        Widgets::dimText(ctx.console.getRevision() == 0 ? "No messages yet." : "No messages match the filters.");
    }

    if (ctx.monoFont) ImGui::PushFont(ctx.monoFont, 13.0f);

    ImGuiListClipper clipper;
    clipper.Begin((int)visible.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
            const ConsoleEntry& entry = visible[i];

            ImGui::PushID(i);
            ImGui::TextColored(Theme::color(Theme::TextDim), "%7.1f", entry.time);
            ImGui::SameLine();
            ImGui::TextColored(Theme::color(levelColor(entry.level)), "%-8s", entry.category.c_str());
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Text, entry.level == LogLevel::Info ? Theme::Text : levelColor(entry.level));
            ImGui::TextUnformatted(entry.message.c_str());
            ImGui::PopStyleColor();

            if (ImGui::BeginPopupContextItem("##entryMenu")) {
                if (ImGui::MenuItem("Copy message")) {
                    ImGui::SetClipboardText(entry.message.c_str());
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }

    if (ctx.monoFont) ImGui::PopFont();

    if (scrollToBottom && atBottom) {
        ImGui::SetScrollHereY(1.0f);
    }
    scrollToBottom = false;

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
}
