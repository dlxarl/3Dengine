#include "EditorContext.h"
#include "Log.h"
#include "MeshShape.h"
#include "ObjectFactory.h"
#include "SceneSerializer.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace fs = std::filesystem;

EditorContext::EditorContext(Project& ownerProject)
    : project(ownerProject), document(ownerProject, assets), play(document) {
    assets.setProject(project.getRoot(), project.libraryDir());
}

SceneObject* EditorContext::getSelected() {
    return scene().find(selectedObject);
}

void EditorContext::select(ObjectId id) {
    selectedObject = id;
    if (id != InvalidObjectId) selectedAsset.clear();
}

void EditorContext::selectAsset(const std::string& assetPath) {
    selectedAsset = assetPath;
    if (!assetPath.empty()) selectedObject = InvalidObjectId;
}

ObjectId EditorContext::takeRenameRequest() {
    const ObjectId id = renameRequest;
    renameRequest = InvalidObjectId;
    return id;
}

SceneObject* EditorContext::createObject(ObjectType type, ObjectId parent) {
    auto object = ObjectFactory::create(type);
    object->name = scene().makeUniqueName(ObjectFactory::typeName(type));

    SceneObject* created = scene().add(std::move(object), scene().find(parent));
    select(created->id);
    document.markDirty();
    return created;
}

SceneObject* EditorContext::addModel(const std::string& assetPath, glm::vec3 position) {
    auto mesh = assets.getMesh(assetPath);
    if (!mesh) {
        dialogs.error("Cannot add the model", assetPath + " could not be loaded. The Console has the details.");
        return nullptr;
    }

    auto object = std::make_shared<MeshShape>();
    object->setMesh(std::move(mesh), assetPath);
    object->name = scene().makeUniqueName(fs::u8path(assetPath).stem().u8string());
    object->setColor(glm::vec3(0.8f));
    object->tintTexture = true;
    object->setPosition(position);

    SceneObject* created = scene().add(std::move(object));
    select(created->id);
    document.markDirty();
    return created;
}

void EditorContext::deleteSelected() {
    if (getSelected()) {
        scene().remove(selectedObject);
        select(InvalidObjectId);
        document.markDirty();
    }
}

void EditorContext::duplicateSelected() {
    if (!getSelected()) return;

    if (SceneObject* copy = SceneSerializer::duplicate(scene(), selectedObject, assets)) {
        select(copy->id);
        document.markDirty();
    }
}

void EditorContext::setObjectTexture(ObjectId id, const std::string& assetPath) {
    Shape* shape = scene().findAs<Shape>(id);
    if (!shape) return;

    shape->setTexture("texture_albedo", assetPath.empty() ? nullptr : assets.getTexture(assetPath));
    document.markDirty();
}

void EditorContext::saveScene() {
    if (!play.isStopped()) {
        dialogs.error("Cannot save during Play mode", "Stop playback first. Changes made while playing are not kept.");
        return;
    }
    saveSceneNow();
}

bool EditorContext::saveSceneNow() {
    if (document.getAssetPath().empty()) {
        promptSaveAs(nullptr);
        return false;
    }

    std::string error;
    if (!document.save(&error)) {
        dialogs.error("Cannot save the scene", error);
        return false;
    }
    setStatus("Saved " + document.getAssetPath());
    return true;
}

void EditorContext::saveSceneAs() {
    if (!play.isStopped()) {
        dialogs.error("Cannot save during Play mode", "Stop playback first. Changes made while playing are not kept.");
        return;
    }
    promptSaveAs(nullptr);
}

std::string EditorContext::saveSceneTo(const std::string& text, std::function<void()> afterSave) {
    std::string name = text;
    if (name.empty()) return "Enter a scene name.";
    if (name.find_first_of("/\\:") != std::string::npos || name.front() == '.') {
        return "A scene name cannot contain / \\ or : or start with a dot.";
    }

    const std::string extension = ".scene";
    if (name.size() < extension.size() || name.compare(name.size() - extension.size(), extension.size(), extension) != 0) {
        name += extension;
    }

    const std::string assetPath = "Assets/Scenes/" + name;
    std::error_code ec;
    if (assetPath != document.getAssetPath() && fs::exists(assets.resolve(assetPath), ec)) {
        return "A scene with this name already exists.";
    }

    std::string error;
    if (!document.saveAs(assetPath, &error)) return error;

    setStatus("Saved " + assetPath);
    if (afterSave) afterSave();
    return {};
}

void EditorContext::promptSaveAs(std::function<void()> afterSave) {
    dialogs.promptText("Save scene as", "Scene name, saved to Assets/Scenes", document.getDisplayName(), "Save scene",
                       [this, afterSave](const std::string& text) { return saveSceneTo(text, afterSave); });
}

void EditorContext::withSavedScene(std::function<void()> action) {
    if (!document.isDirty()) {
        action();
        return;
    }

    dialogs.unsavedChanges(
        document.getDisplayName(),
        [this, action] {
            if (document.getAssetPath().empty()) {
                promptSaveAs(action);
            } else if (saveSceneNow()) {
                action();
            }
        },
        action);
}

void EditorContext::openScene(const std::string& assetPath) {
    auto openNow = [this, assetPath] {
        play.stop();
        std::string error;
        if (document.open(assetPath, &error)) {
            select(InvalidObjectId);
            setStatus("Opened " + assetPath);
        } else {
            dialogs.error("Cannot open the scene", error);
        }
    };
    withSavedScene(openNow);
}

void EditorContext::newScene() {
    withSavedScene([this] {
        play.stop();
        document.createNew();
        select(InvalidObjectId);
    });
}

void EditorContext::startPlaying() {
    play.play();
    if (scene().getMainCamera()) {
        viewMode = ViewMode::Game;
    }
}

void EditorContext::stopPlaying() {
    play.stop();
    viewMode = ViewMode::Scene;
}

void EditorContext::closeProject() {
    withSavedScene([this] {
        play.stop();
        exitRequest = EditorExit::CloseProject;
    });
}

void EditorContext::quit() {
    withSavedScene([this] {
        play.stop();
        exitRequest = EditorExit::Quit;
    });
}

void EditorContext::graphicsChanged() {
    graphicsSaveDelay = 0.5f;
}

void EditorContext::updateAutosave(float deltaTime) {
    if (graphicsSaveDelay < 0.0f) return;

    graphicsSaveDelay -= deltaTime;
    if (graphicsSaveDelay < 0.0f) {
        std::string error;
        if (!project.saveSettings(&error)) {
            Log::error("Project", error);
        }
    }
}

void EditorContext::setStatus(const std::string& text) {
    status = text;
}
