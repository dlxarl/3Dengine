#include "EditorContext.h"
#include "Log.h"
#include "MeshShape.h"
#include "ObjectFactory.h"
#include "ScriptBuilder.h"
#include "SceneSerializer.h"
#include "UiObject.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <system_error>

namespace fs = std::filesystem;

EditorContext::EditorContext(Project& ownerProject)
    : project(ownerProject), document(ownerProject, assets), play(document) {
    assets.setProject(project.getRoot(), project.libraryDir());
    play.setAudioDirectory(project.assetsDir() / "Audio");
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

SceneObject* EditorContext::createUi(const std::string& what, ObjectId parent) {
    auto make = [&](UiKind kind, const char* name, SceneObject* under) {
        auto element = std::make_shared<UiObject>();
        element->kind = kind;
        element->name = scene().makeUniqueName(name);
        UiObject* raw = element.get();
        scene().add(std::move(element), under);
        return raw;
    };
    auto place = [](UiObject* ui, UiAnchor anchor, glm::vec2 offset, glm::vec2 size) {
        ui->anchor = anchor;
        ui->offset = offset;
        ui->size = size;
        return ui;
    };

    SceneObject* under = scene().find(parent);
    UiObject* created = nullptr;

    if (what == "Panel") {
        created = make(UiKind::Panel, "Panel", under);
        created->color = glm::vec4(0.0f, 0.0f, 0.0f, 0.55f);
    } else if (what == "Text") {
        created = make(UiKind::Text, "Text", under);
        created->text = "Text";
        created->size = glm::vec2(200.0f, 32.0f);
    } else if (what == "Image") {
        created = make(UiKind::Image, "Image", under);
        created->size = glm::vec2(96.0f);
    } else if (what == "Bar") {
        created = make(UiKind::Bar, "Bar", under);
        created->size = glm::vec2(240.0f, 16.0f);
        created->color = glm::vec4(0.75f, 0.2f, 0.2f, 1.0f);
        created->fill = 0.7f;
    } else if (what == "Button") {
        created = make(UiKind::Button, "Button", under);
        created->text = "Button";
        created->size = glm::vec2(200.0f, 48.0f);
        created->color = glm::vec4(0.2f, 0.24f, 0.3f, 0.95f);
        created->rounding = 4.0f;
    } else if (what == "HUD") {
        created = make(UiKind::Panel, "HUD", under);
        created->stretch = true;
        created->color = glm::vec4(0.0f);
        UiObject* health = place(make(UiKind::Bar, "Health Bar", created), UiAnchor::BottomLeft, {24.0f, -48.0f}, {260.0f, 14.0f});
        health->color = glm::vec4(0.72f, 0.18f, 0.18f, 1.0f);
        health->binding = UiBinding::Health;
        UiObject* stamina = place(make(UiKind::Bar, "Stamina Bar", created), UiAnchor::BottomLeft, {24.0f, -28.0f}, {260.0f, 8.0f});
        stamina->color = glm::vec4(0.8f, 0.7f, 0.25f, 1.0f);
        stamina->binding = UiBinding::Stamina;
        UiObject* title = place(make(UiKind::Text, "Score", created), UiAnchor::TopRight, {-24.0f, 20.0f}, {220.0f, 32.0f});
        title->text = "Score 0";
        title->align = 2;
    } else if (what == "Menu") {
        created = make(UiKind::Panel, "Menu", under);
        created->stretch = true;
        created->color = glm::vec4(0.0f, 0.0f, 0.0f, 0.6f);
        UiObject* box = place(make(UiKind::Panel, "Menu Box", created), UiAnchor::Center, {0.0f, 0.0f}, {360.0f, 300.0f});
        box->color = glm::vec4(0.09f, 0.1f, 0.12f, 0.96f);
        box->rounding = 6.0f;
        UiObject* heading = place(make(UiKind::Text, "Title", box), UiAnchor::Top, {0.0f, 24.0f}, {320.0f, 44.0f});
        heading->text = "Menu";
        heading->fontSize = 36.0f;
        heading->align = 1;
        const char* labels[] = {"Play", "Settings", "Quit"};
        for (int i = 0; i < 3; i++) {
            UiObject* button = place(make(UiKind::Button, labels[i], box), UiAnchor::Top, {0.0f, 100.0f + i * 64.0f}, {240.0f, 48.0f});
            button->text = labels[i];
            button->color = glm::vec4(0.2f, 0.24f, 0.3f, 0.95f);
            button->rounding = 4.0f;
        }
    } else {
        return nullptr;
    }

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

const std::vector<ScriptInfo>& EditorContext::availableScripts() {
    const auto now = std::chrono::steady_clock::now();
    if (now - scriptsScannedAt > std::chrono::milliseconds(1500) || scriptsScannedAt.time_since_epoch().count() == 0) {
        scriptsScannedAt = now;
        scriptList = ScriptBuilder::scan(project);
    }
    return scriptList;
}

void EditorContext::startPlaying(bool fullscreen) {
    // Scripts are compiled first; a scene whose scripts do not compile does not play.
    bool usesScripts = false;
    for (const auto& object : scene().getObjects()) usesScripts |= !object->script.empty();
    std::filesystem::path library;
    if (usesScripts) {
        setStatus("Compiling scripts...");
        std::string log;
        if (!ScriptBuilder::build(project, library, log)) {
            Log::error("Script", "The scripts did not compile:\n" + log);
            dialogs.error("The scripts did not compile", log.size() > 1500 ? log.substr(0, 1500) + "\n..." : log);
            return;
        }
        setStatus("");
    }
    play.setScriptLibrary(library);

    play.play();
    if (scene().getMainCamera()) {
        viewMode = ViewMode::Game;
    }
    fullscreenPreview = fullscreen && viewMode == ViewMode::Game;
    wantsMouseCapture = play.getGameStatus().hasPlayer;
}

void EditorContext::stopPlaying() {
    play.stop();
    viewMode = ViewMode::Scene;
    fullscreenPreview = false;
    wantsMouseCapture = false;
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
