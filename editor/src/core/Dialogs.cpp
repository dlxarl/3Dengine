#include "Dialogs.h"
#include "Theme.h"
#include "Widgets.h"

#include "imgui.h"

namespace {
    const float DIALOG_WIDTH = 420.0f;

    const char* const SHORTCUTS[][2] = {
        {"Look around", "Hold right mouse button and drag"},
        {"Fly", "Hold right mouse button, then W A S D, Q E"},
        {"Move faster", "Hold Shift while flying"},
        {"Orbit", "Option + left mouse button"},
        {"Pan", "Middle mouse button, or Option + Shift + left mouse button"},
        {"Zoom", "Scroll"},
        {"Focus selection", "F"},
        {"Move, rotate, scale tool", "W, E, R"},
        {"Save scene", "Command + S"},
        {"Play, pause", "Command + P, Command + Shift + P"},
        {"Duplicate", "Command + D"},
        {"Delete", "Delete"},
        {"Rename", "F2"},
        {"In the Game view: move, jump", "W A S D or arrows, Space"},
        {"In the Game view: camera, restart", "Q E or right mouse button, R"},
        {"In the Game view: view, free mouse", "V, Esc"},
    };
}

void Dialogs::present(Kind newKind, const std::string& newTitle) {
    kind = newKind;
    title = newTitle;
    openRequested = true;
}

void Dialogs::close() {
    kind = Kind::None;
    primary = nullptr;
    secondary = nullptr;
    textHandler = nullptr;
    ImGui::CloseCurrentPopup();
}

void Dialogs::unsavedChanges(const std::string& sceneName, Action onSave, Action onDiscard) {
    message = "Save the changes to " + sceneName + "? They are lost if you don't.";
    primary = std::move(onSave);
    secondary = std::move(onDiscard);
    present(Kind::Unsaved, "Unsaved changes");
}

void Dialogs::confirm(const std::string& newTitle, const std::string& newMessage, const std::string& label, Action onConfirm) {
    message = newMessage;
    confirmLabel = label;
    primary = std::move(onConfirm);
    present(Kind::Confirm, newTitle);
}

void Dialogs::promptText(const std::string& newTitle, const std::string& label, const std::string& initial,
                         const std::string& buttonLabel, TextHandler onConfirm) {
    inputLabel = label;
    input = initial;
    inputError.clear();
    confirmLabel = buttonLabel;
    textHandler = std::move(onConfirm);
    focusInput = true;
    present(Kind::Text, newTitle);
}

void Dialogs::error(const std::string& newTitle, const std::string& newMessage) {
    message = newMessage;
    present(Kind::Error, newTitle);
}

void Dialogs::shortcuts() {
    present(Kind::Shortcuts, "Keyboard and mouse");
}

void Dialogs::draw() {
    if (kind == Kind::None) return;

    if (openRequested) {
        ImGui::OpenPopup(title.c_str());
        openRequested = false;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(kind == Kind::Shortcuts ? 520.0f : DIALOG_WIDTH, 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 10.0f));

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, flags)) {
        switch (kind) {
            case Kind::Unsaved:   drawUnsaved(); break;
            case Kind::Confirm:   drawConfirm(); break;
            case Kind::Text:      drawText(); break;
            case Kind::Error:     drawError(); break;
            case Kind::Shortcuts: drawShortcuts(); break;
            case Kind::None:      break;
        }
        ImGui::EndPopup();
    }

    ImGui::PopStyleVar(2);
}

void Dialogs::drawUnsaved() {
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::Spacing();

    if (Widgets::primaryButton("Save")) {
        Action action = primary;
        close();
        if (action) action();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't save")) {
        Action action = secondary;
        close();
        if (action) action();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void Dialogs::drawConfirm() {
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::Spacing();

    if (Widgets::primaryButton(confirmLabel.c_str())) {
        Action action = primary;
        close();
        if (action) action();
        return;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void Dialogs::drawText() {
    Widgets::dimText(inputLabel.c_str());

    if (focusInput) {
        ImGui::SetKeyboardFocusHere();
        focusInput = false;
    }
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool submitted = Widgets::inputText("##dialogInput", input, ImGuiInputTextFlags_EnterReturnsTrue);

    if (!inputError.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::Error);
        ImGui::TextWrapped("%s", inputError.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    submitted |= Widgets::primaryButton(confirmLabel.c_str());

    if (submitted) {
        TextHandler handler = textHandler;
        const std::string value = input;
        inputError = handler ? handler(value) : std::string();
        if (inputError.empty()) {
            close();
        } else {
            focusInput = true;
        }
        return;
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void Dialogs::drawError() {
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::Spacing();
    if (Widgets::primaryButton("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}

void Dialogs::drawShortcuts() {
    if (ImGui::BeginTable("##shortcuts", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthFixed, 170.0f);
        ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& row : SHORTCUTS) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row[0]);
            ImGui::TableSetColumnIndex(1);
            Widgets::dimText(row[1]);
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        close();
    }
}
