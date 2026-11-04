#include "CodeEditorPanel.h"
#include "Theme.h"
#include "UserSettings.h"
#include "Widgets.h"

#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {
    bool readFile(const fs::path& path, std::string& text) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        std::stringstream buffer;
        buffer << in.rdbuf();
        text = buffer.str();
        return true;
    }

    int lineCount(const std::string& text) {
        return 1 + (int)std::count(text.begin(), text.end(), '\n');
    }

    size_t startOfLine(const std::string& text, int line) {
        size_t at = 0;
        for (int i = 1; i < line && at != std::string::npos; i++) {
            at = text.find('\n', at);
            if (at != std::string::npos) at++;
        }
        return at == std::string::npos ? text.size() : at;
    }
}

bool CodeEditorPanel::hasUnsavedFiles() const {
    for (const Document& doc : documents) {
        if (doc.dirty()) return true;
    }
    return false;
}

bool CodeEditorPanel::openFile(EditorContext& ctx, const fs::path& file, int line) {
    if (!ctx.settings.externalEditor.empty()) {
        return openInExternalEditor(ctx.settings.externalEditor, file, line);
    }

    for (size_t i = 0; i < documents.size(); i++) {
        std::error_code ec;
        if (fs::equivalent(documents[i].path, file, ec)) {
            active = (int)i;
            documents[i].requestFocus = true;
            documents[i].goToLine = line;
            return true;
        }
    }

    Document doc;
    doc.path = file;
    if (!readFile(file, doc.text)) {
        ctx.dialogs.error("Cannot open the file", file.filename().u8string() + " could not be read.");
        return false;
    }
    doc.saved = doc.text;
    doc.goToLine = line;
    doc.requestFocus = true;
    documents.push_back(std::move(doc));
    active = (int)documents.size() - 1;
    return true;
}

bool CodeEditorPanel::save(EditorContext& ctx, Document& doc) {
    std::ofstream out(doc.path, std::ios::binary | std::ios::trunc);
    out << doc.text;
    if (!out) {
        ctx.dialogs.error("Cannot save", doc.path.filename().u8string() + " could not be written.");
        return false;
    }
    doc.saved = doc.text;
    ctx.setStatus("Saved " + doc.path.filename().u8string());
    return true;
}

void CodeEditorPanel::drawToolbar(EditorContext& ctx) {
    Document* doc = active >= 0 && active < (int)documents.size() ? &documents[active] : nullptr;

    ImGui::BeginDisabled(!doc);
    if (ImGui::Button("Save") && doc) save(ctx, *doc);
    ImGui::SameLine();
    if (ImGui::Button("Find")) {
        findOpen = !findOpen;
        findRequested = findOpen;
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!doc || ctx.settings.externalEditor.empty());
    if (ImGui::Button("Open in IDE") && doc) {
        save(ctx, *doc);
        openInExternalEditor(ctx.settings.externalEditor, doc->path, doc->cursorLine);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("IDE settings")) ImGui::OpenPopup("##ide");
    if (ImGui::BeginPopup("##ide")) {
        Widgets::dimText("Open code files with");
        bool custom = true;
        for (const IdePreset& preset : idePresets()) {
            const bool selected = ctx.settings.externalEditor == preset.command;
            custom &= !selected;
            if (ImGui::RadioButton(preset.name, selected)) {
                ctx.settings.externalEditor = preset.command;
                ctx.settings.save();
            }
        }
        ImGui::Separator();
        Widgets::dimText("Or your own command. {file} is the path and {line} the line.");
        ImGui::SetNextItemWidth(380.0f);
        std::string command = ctx.settings.externalEditor;
        if (ImGui::InputText("##command", &command)) {
            ctx.settings.externalEditor = command;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) ctx.settings.save();
        if (custom && !ctx.settings.externalEditor.empty()) Widgets::dimText("Using a custom command.");
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderInt("##fontSize", &fontSize, 10, 24, "%d px");
    if (ImGui::IsItemHovered()) Widgets::tooltip("Text size");
}

void CodeEditorPanel::drawFind(EditorContext& ctx, Document& doc) {
    if (!findOpen) return;

    ImGui::SetNextItemWidth(200.0f);
    if (findRequested) {
        ImGui::SetKeyboardFocusHere();
        findRequested = false;
    }
    const bool submitted = ImGui::InputTextWithHint("##find", "Find", &findText, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##replace", "Replace with", &replaceText);

    int matches = 0;
    if (!findText.empty()) {
        for (size_t at = doc.text.find(findText); at != std::string::npos; at = doc.text.find(findText, at + findText.size())) matches++;
    }

    ImGui::SameLine();
    const bool next = ImGui::Button("Next") || submitted;
    ImGui::SameLine();
    const bool replaceOne = ImGui::Button("Replace");
    ImGui::SameLine();
    const bool replaceAll = ImGui::Button("Replace all");
    ImGui::SameLine();
    Widgets::dimText((std::to_string(matches) + " found").c_str());

    if (findText.empty() || matches == 0) return;

    if (replaceAll) {
        std::string result;
        size_t from = 0;
        for (size_t at = doc.text.find(findText); at != std::string::npos; at = doc.text.find(findText, from)) {
            result.append(doc.text, from, at - from);
            result += replaceText;
            from = at + findText.size();
        }
        result.append(doc.text, from, std::string::npos);
        doc.text = result;
        return;
    }

    ImGuiID id = ImGui::GetID(("##code" + doc.path.u8string()).c_str());
    // The text field's own state tells where the caret is, and a selection is set the same way.
    ImGuiInputTextState* state = ImGui::GetInputTextState(id);
    if (!state) return;
    const int caret = std::min(state->GetCursorPos(), (int)doc.text.size());

    if (replaceOne) {
        const int start = state->GetSelectionStart();
        const int end = state->GetSelectionEnd();
        if (end - start == (int)findText.size() && doc.text.compare(start, findText.size(), findText) == 0) {
            doc.text.replace(start, findText.size(), replaceText);
            state->SetSelection(start + (int)replaceText.size(), start + (int)replaceText.size());
        }
    }
    if (next || replaceOne) {
        const int from = std::max(caret, state->GetSelectionEnd());
        size_t found = doc.text.find(findText, from);
        if (found == std::string::npos) found = doc.text.find(findText);
        if (found != std::string::npos) {
            state->SetSelection((int)found, (int)(found + findText.size()));
            doc.goToLine = 1 + (int)std::count(doc.text.begin(), doc.text.begin() + found, '\n');
            doc.requestFocus = true;
        }
    }
}

void CodeEditorPanel::drawDocument(EditorContext& ctx, Document& doc) {
    ImGui::PushFont(ctx.monoFont ? ctx.monoFont : ImGui::GetFont(), (float)fontSize);
    const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    const int lines = lineCount(doc.text);
    const std::string widest = std::to_string(lines) + "  ";
    const float gutterWidth = ImGui::CalcTextSize(widest.c_str()).x + 8.0f;

    const float statusHeight = ImGui::GetFrameHeightWithSpacing();
    const ImVec2 area(ImGui::GetContentRegionAvail().x, std::max(60.0f, ImGui::GetContentRegionAvail().y - statusHeight));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + gutterWidth, origin.y + area.y), Theme::Sunken);

    // Line numbers follow the text's scroll, as of the last frame.
    draw->PushClipRect(origin, ImVec2(origin.x + gutterWidth, origin.y + area.y), true);
    const float topPadding = ImGui::GetStyle().FramePadding.y;
    const int first = std::max(0, (int)(doc.scrollY / lineHeight));
    const int visibleLines = (int)(area.y / lineHeight) + 2;
    for (int line = first; line < std::min(lines, first + visibleLines); line++) {
        const std::string number = std::to_string(line + 1);
        const float width = ImGui::CalcTextSize(number.c_str()).x;
        const ImU32 color = (line + 1 == doc.cursorLine) ? Theme::Text : Theme::TextDim;
        draw->AddText(ImVec2(origin.x + gutterWidth - width - 6.0f, origin.y + topPadding + line * lineHeight - doc.scrollY), color, number.c_str());
    }
    draw->PopClipRect();

    ImGui::SetCursorScreenPos(ImVec2(origin.x + gutterWidth, origin.y));
    const std::string id = "##code" + doc.path.u8string();
    const ImGuiID widgetId = ImGui::GetID(id.c_str());

    ImGui::PushStyleColor(ImGuiCol_FrameBg, Theme::Sunken);
    if (doc.requestFocus) {
        ImGui::SetKeyboardFocusHere();
        doc.requestFocus = false;
    }
    ImGui::InputTextMultiline(id.c_str(), &doc.text, ImVec2(area.x - gutterWidth, area.y), ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopStyleColor();

    ctx.textEditorFocused |= ImGui::IsItemActive() || ImGui::IsItemFocused();

    if (ImGuiInputTextState* state = ImGui::GetInputTextState(widgetId)) {
        doc.scrollY = state->Scroll.y;

        const int caret = std::min(state->GetCursorPos(), (int)doc.text.size());
        doc.cursorLine = 1 + (int)std::count(doc.text.begin(), doc.text.begin() + caret, '\n');
        const size_t lineStart = doc.text.rfind('\n', caret > 0 ? caret - 1 : 0);
        doc.cursorColumn = caret - (lineStart == std::string::npos || caret == 0 ? 0 : (int)lineStart + 1) + 1;

        if (doc.goToLine > 0) {
            const int target = std::clamp(doc.goToLine, 1, lines);
            const int at = (int)startOfLine(doc.text, target);
            // Only a jump asked for from outside; a find has already placed the selection.
            if (!state->HasSelection()) state->SetSelection(at, at);
            state->Scroll.y = std::max(0.0f, (target - 4) * lineHeight);
            doc.goToLine = 0;
        }
    }

    ImGui::PopFont();

    char status[128];
    std::snprintf(status, sizeof(status), "Line %d, column %d    %d lines    %s", doc.cursorLine, doc.cursorColumn, lines,
                  doc.dirty() ? "unsaved" : "saved");
    Widgets::dimText(status);
}

void CodeEditorPanel::draw(EditorContext& ctx, bool* open) {
    ImGui::SetNextWindowSize(ImVec2(780.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Code Editor", open)) {
        ImGui::End();
        return;
    }

    // Cmd+S saves the file here, not the scene.
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (focused && active >= 0 && active < (int)documents.size() && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
        save(ctx, documents[active]);
    }
    if (focused && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F)) {
        findOpen = true;
        findRequested = true;
    }

    drawToolbar(ctx);

    if (documents.empty()) {
        ImGui::Spacing();
        Widgets::dimText("Double-click a script or text file in the Project panel to open it here.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("##documents", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (size_t i = 0; i < documents.size(); i++) {
            Document& doc = documents[i];
            bool tabOpen = true;
            std::string label = doc.path.filename().u8string() + (doc.dirty() ? " *" : "") + "###" + doc.path.u8string();
            ImGuiTabItemFlags flags = (int)i == active && doc.goToLine > 0 ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label.c_str(), &tabOpen, flags)) {
                active = (int)i;
                drawFind(ctx, doc);
                drawDocument(ctx, doc);
                ImGui::EndTabItem();
            }
            if (!tabOpen) doc.close = true;
        }
        ImGui::EndTabBar();
    }

    for (size_t i = 0; i < documents.size();) {
        if (documents[i].close) {
            if (documents[i].dirty()) save(ctx, documents[i]);   // closing keeps what was typed
            documents.erase(documents.begin() + i);
            active = std::min(active, (int)documents.size() - 1);
        } else {
            i++;
        }
    }

    ImGui::End();
}
