#pragma once
#include <functional>
#include <string>

// The Editor's modal dialogs. Callers describe what to ask and what to do with the answer;
// draw() runs once per frame at the root of the UI, where ImGui popups must live.
class Dialogs {
public:
    using Action = std::function<void()>;

    // Returns the problem with the entered text; empty accepts it and closes the dialog.
    using TextHandler = std::function<std::string(const std::string&)>;

    void unsavedChanges(const std::string& sceneName, Action onSave, Action onDiscard);
    void confirm(const std::string& title, const std::string& message, const std::string& confirmLabel, Action onConfirm);
    void promptText(const std::string& title, const std::string& label, const std::string& initial,
                    const std::string& confirmLabel, TextHandler onConfirm);
    void error(const std::string& title, const std::string& message);
    void shortcuts();

    void draw();

private:
    enum class Kind { None, Unsaved, Confirm, Text, Error, Shortcuts };

    Kind kind = Kind::None;
    bool openRequested = false;

    std::string title;
    std::string message;
    std::string confirmLabel;
    std::string inputLabel;
    std::string input;
    std::string inputError;
    bool focusInput = false;

    Action primary;
    Action secondary;
    TextHandler textHandler;

    void present(Kind newKind, const std::string& newTitle);
    void close();

    void drawUnsaved();
    void drawConfirm();
    void drawText();
    void drawError();
    void drawShortcuts();
};
