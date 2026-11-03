#pragma once
#include "AssetManager.h"
#include "Project.h"
#include "SceneGraph.h"

#include <memory>
#include <string>

// The scene open in the Editor: the live SceneGraph plus where it is stored and whether
// it has unsaved changes.
class SceneDocument {
public:
    SceneDocument(Project& project, AssetManager& assets);

    SceneGraph& getScene() { return *scene; }

    const std::string& getAssetPath() const { return assetPath; }  // empty until first saved
    std::string getDisplayName() const;

    bool isDirty() const { return dirty; }
    void markDirty();

    // While true, edits are temporary (Play mode), so they do not mark the document.
    void setTrackingChanges(bool tracking) { trackingChanges = tracking; }

    // A new, unsaved scene with a camera and a light.
    void createNew();

    bool open(const std::string& newAssetPath, std::string* error = nullptr);
    bool save(std::string* error = nullptr);
    bool saveAs(const std::string& newAssetPath, std::string* error = nullptr);

    // Captures and restores the whole scene in memory, without touching the file.
    std::string snapshot() const;
    bool restore(const std::string& snapshotText, std::string* error = nullptr);

private:
    Project& project;
    AssetManager& assets;

    std::unique_ptr<SceneGraph> scene;
    std::string assetPath;
    bool dirty = false;
    bool trackingChanges = true;
};
