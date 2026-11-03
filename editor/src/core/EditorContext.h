#pragma once
#include "AssetManager.h"
#include "ConsoleLog.h"
#include "Dialogs.h"
#include "PlayController.h"
#include "Project.h"
#include "SceneDocument.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

struct ImFont;

enum class ViewMode { Scene, Game };

// What the user asked to happen to the Editor as a whole.
enum class EditorExit { None, CloseProject, Quit };

// State shared by the panels, and the operations they have in common. A panel changes the
// scene through here so that a menu item and a shortcut do exactly the same thing.
class EditorContext {
public:
    explicit EditorContext(Project& project);

    Project& project;
    AssetManager assets;
    SceneDocument document;
    PlayController play;
    ConsoleLog console;
    Dialogs dialogs;

    ViewMode viewMode = ViewMode::Scene;
    ImFont* monoFont = nullptr;

    SceneGraph& scene() { return document.getScene(); }

    // Selection. An object and an asset are mutually exclusive; the Inspector shows either.
    ObjectId selectedObject = InvalidObjectId;
    std::string selectedAsset;

    SceneObject* getSelected();
    void select(ObjectId id);
    void selectAsset(const std::string& assetPath);

    // Set by the UI; the Scene view frames the selection on its next frame.
    bool focusRequested = false;
    void requestRename(ObjectId id) { renameRequest = id; }
    ObjectId takeRenameRequest();

    // Objects
    SceneObject* createObject(ObjectType type, ObjectId parent = InvalidObjectId);
    SceneObject* addModel(const std::string& assetPath, glm::vec3 position);
    void deleteSelected();
    void duplicateSelected();
    void setObjectTexture(ObjectId id, const std::string& assetPath);

    // Scene files. Each of these asks about unsaved changes first where that applies.
    void saveScene();
    void saveSceneAs();
    void openScene(const std::string& assetPath);
    void newScene();

    // Play mode. The main view follows it: Game while playing, Scene again after.
    void startPlaying();
    void stopPlaying();

    // Project
    void closeProject();
    void quit();
    EditorExit getExitRequest() const { return exitRequest; }

    // Graphics settings are written shortly after the last change.
    void graphicsChanged();
    void updateAutosave(float deltaTime);

    // Files dropped on the window, from the operating system.
    std::vector<std::filesystem::path> droppedFiles;

    void setStatus(const std::string& text);
    const std::string& getStatus() const { return status; }

private:
    ObjectId renameRequest = InvalidObjectId;
    EditorExit exitRequest = EditorExit::None;
    std::string status;
    float graphicsSaveDelay = -1.0f;

    // Runs action, first offering to save the scene when it has unsaved changes.
    void withSavedScene(std::function<void()> action);
    bool saveSceneNow();
    std::string saveSceneTo(const std::string& text, std::function<void()> afterSave);
    void promptSaveAs(std::function<void()> afterSave);
};
