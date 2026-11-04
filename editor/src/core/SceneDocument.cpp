#include "SceneDocument.h"
#include "Log.h"
#include "ObjectFactory.h"
#include "SceneSerializer.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

SceneDocument::SceneDocument(Project& ownerProject, AssetManager& assetManager)
    : project(ownerProject), assets(assetManager), scene(std::make_unique<SceneGraph>()) {}

std::string SceneDocument::getDisplayName() const {
    if (assetPath.empty()) return "Untitled";
    return fs::u8path(assetPath).stem().u8string();
}

void SceneDocument::markDirty() {
    if (trackingChanges) {
        dirty = true;
        revisionCount++;
    }
}

void SceneDocument::createNew() {
    auto fresh = std::make_unique<SceneGraph>();

    auto camera = ObjectFactory::create(ObjectType::Camera);
    camera->name = "Main Camera";
    camera->setPosition(glm::vec3(0.0f, 2.0f, 8.0f));
    camera->setRotation(glm::vec3(-10.0f, 0.0f, 0.0f));
    fresh->add(camera);

    auto light = ObjectFactory::create(ObjectType::Light);
    light->name = "Light";
    light->setPosition(glm::vec3(8.0f, 16.0f, 10.0f));
    static_cast<LightObject*>(light.get())->type = LightType::Directional;
    light->setRotation(glm::vec3(-50.0f, 30.0f, 0.0f));
    fresh->add(light);

    scene = std::move(fresh);
    assetPath.clear();
    dirty = false;
    epochCount++;
}

bool SceneDocument::open(const std::string& newAssetPath, std::string* error) {
    const fs::path file = assets.resolve(newAssetPath);
    if (file.empty()) {
        if (error) *error = "Invalid scene path: " + newAssetPath;
        return false;
    }

    // Loaded on the side, so that a damaged file leaves the current scene untouched.
    auto loaded = std::make_unique<SceneGraph>();
    if (!SceneSerializer::loadFile(file, *loaded, assets, error)) return false;

    scene = std::move(loaded);
    assetPath = newAssetPath;
    dirty = false;
    epochCount++;

    project.startupScene = assetPath;
    project.saveSettings();

    Log::info("Scene", "Opened " + assetPath);
    return true;
}

bool SceneDocument::save(std::string* error) {
    if (assetPath.empty()) {
        if (error) *error = "This scene has no file yet.";
        return false;
    }
    return saveAs(assetPath, error);
}

bool SceneDocument::saveAs(const std::string& newAssetPath, std::string* error) {
    const fs::path file = assets.resolve(newAssetPath);
    if (file.empty()) {
        if (error) *error = "Invalid scene path: " + newAssetPath;
        return false;
    }

    std::error_code ec;
    fs::create_directories(file.parent_path(), ec);

    if (!SceneSerializer::saveFile(*scene, file, error)) return false;

    assetPath = newAssetPath;
    dirty = false;

    project.startupScene = assetPath;
    project.saveSettings();

    Log::info("Scene", "Saved " + assetPath);
    return true;
}

std::string SceneDocument::snapshot() const {
    return SceneSerializer::serialize(*scene);
}

bool SceneDocument::restore(const std::string& snapshotText, std::string* error) {
    auto restored = std::make_unique<SceneGraph>();
    if (!SceneSerializer::deserialize(snapshotText, *restored, assets, error)) return false;
    scene = std::move(restored);
    return true;
}
