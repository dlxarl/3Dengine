#pragma once
#include "CameraObject.h"
#include "LightObject.h"
#include "Scene.h"
#include "Shape.h"
#include "Skybox.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetManager;

// A data-driven scene: a tree of addressable objects, built at runtime or loaded from a file.
// Unlike the hand-written demo scenes, everything in it can be listed, found and changed.
class SceneGraph : public Scene {
public:
    SceneGraph();

    // Scene
    void load() override {}
    void update(float deltaTime) override;
    void draw(Shader& lightingShader, Shader& lampShader, const glm::mat4& view, const glm::mat4& proj) override;
    void drawShadow(Shader& shadowShader) override;
    void drawDepth(Shader& depthShader) override;
    glm::vec3 getLightPos() const override;
    glm::vec3 getLightColor() const override;
    glm::vec3 getClearColor() const override { return clearColor; }

    // Takes ownership and gives the object an id, unless it arrives with an unused one.
    SceneObject* add(std::shared_ptr<SceneObject> object, SceneObject* parent = nullptr, int childIndex = -1);

    // Removes the object and everything below it.
    void remove(ObjectId id);
    void clear();

    SceneObject* find(ObjectId id) const;
    template <typename T> T* findAs(ObjectId id) const { return dynamic_cast<T*>(find(id)); }

    const std::vector<std::shared_ptr<SceneObject>>& getObjects() const { return objects; }
    const std::vector<std::shared_ptr<Shape>>& getShapes() const { return shapes; }
    const std::vector<SceneObject*>& getRoots() const { return roots; }

    // Moves an object in the hierarchy, keeping where it is in the world.
    // Fails for moves that would create a cycle.
    bool reparent(ObjectId id, ObjectId newParent, int childIndex = -1);

    std::string makeUniqueName(const std::string& baseName) const;

    // The renderer uses a single light: the first one in the scene. Without any, it
    // falls back to a default white light so that a new scene is not pitch black.
    LightObject* getLight() const;
    CameraObject* getMainCamera() const;

    ObjectId getNextId() const { return nextId; }
    void setNextId(ObjectId id) { nextId = id; }

    // Environment
    glm::vec3 clearColor = glm::vec3(0.1f);
    void setSkybox(const std::string& assetPath, AssetManager& assets);
    const std::string& getSkyboxAsset() const { return skyboxAsset; }

private:
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::vector<std::shared_ptr<Shape>> shapes;
    std::vector<SceneObject*> roots;
    std::unordered_map<ObjectId, SceneObject*> lookup;

    ObjectId nextId = 1;

    std::unique_ptr<Skybox> skybox;
    std::string skyboxAsset;
};
