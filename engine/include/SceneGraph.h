#pragma once
#include "CameraObject.h"
#include "Colliders.h"
#include "EffectObjects.h"
#include "GameDefinition.h"
#include "HeightField.h"
#include "LightObject.h"
#include "Scene.h"
#include "Shape.h"
#include "Skybox.h"
#include "WaterArea.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetManager;
class Player;

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
    glm::vec2 getFogRange() const override { return glm::vec2(fogStart, fogEnd); }
    bool getLightDirection(glm::vec3& direction) const override;
    glm::vec3 getAmbientColor() const override { return ambientColor; }
    float getAnimationTime() const override { return animationTime; }
    void drawShadowCasters(Shader& depthShader, const glm::mat4& lightSpace) override;
    int getLights(glm::vec3 eye, SceneLight* out, int most) const override;
    int getPointLights(glm::vec3 eye, glm::vec4* positions, glm::vec3* colors, int most) const override;
    bool hasGradientSky() const override { return gradientSky; }
    void drawEffects(EffectRenderer& renderer) override;
    UnderwaterView getUnderwater(glm::vec3 eye) const override;

    // Advances the clock of waves and glints and the particles, and nothing else. update() does it
    // too; the Editor calls this alone so that a fire burns in a scene that is not playing.
    void animate(float deltaTime);

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
    const std::vector<std::shared_ptr<ParticleEmitter>>& getEmitters() const { return emitters; }
    const std::vector<std::shared_ptr<FireObject>>& getFires() const { return fires; }
    const std::vector<std::shared_ptr<WaterObject>>& getWaters() const { return waters; }
    const std::vector<std::shared_ptr<AudioSourceObject>>& getAudioSources() const { return audioSources; }

    // Moves an object in the hierarchy, keeping where it is in the world.
    // Fails for moves that would create a cycle.
    bool reparent(ObjectId id, ObjectId newParent, int childIndex = -1);

    std::string makeUniqueName(const std::string& baseName) const;

    // The renderer uses a single light: the first one in the scene. Without any, it
    // falls back to a default white light so that a new scene is not pitch black.
    LightObject* getLight() const;
    CameraObject* getMainCamera() const;

    // The first Player in the scene, which the game rules drive.
    Player* getPlayer() const;

    ObjectId getNextId() const { return nextId; }
    void setNextId(ObjectId id) { nextId = id; }

    // Environment
    glm::vec3 clearColor = glm::vec3(0.1f);
    float fogStart = 0.0f;
    float fogEnd = 0.0f;
    glm::vec3 ambientColor = glm::vec3(0.35f);
    bool gradientSky = false;

    // Items, characters and quests; empty for scenes that are not games.
    GameDefinition game;

    // The terrain while a game runs, so that physics objects land on it. Built by GameSession.
    HeightField ground;

    // Water that is not a Water object, such as a river made of meshes, while a game runs. Built by
    // GameSession, so that the camera knows when it is under it.
    std::vector<WaterArea> waterAreas;

    // The solids while a game runs, found by place. Built by GameSession; cleared with the scene.
    ColliderGrid colliders;

    void setSkybox(const std::string& assetPath, AssetManager& assets);
    const std::string& getSkyboxAsset() const { return skyboxAsset; }

private:
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::vector<std::shared_ptr<Shape>> shapes;
    std::vector<std::shared_ptr<ParticleEmitter>> emitters;
    std::vector<std::shared_ptr<FireObject>> fires;
    std::vector<std::shared_ptr<WaterObject>> waters;
    std::vector<std::shared_ptr<AudioSourceObject>> audioSources;
    std::vector<SceneObject*> roots;
    std::unordered_map<ObjectId, SceneObject*> lookup;

    ObjectId nextId = 1;

    std::unique_ptr<Skybox> skybox;
    std::string skyboxAsset;

    float animationTime = 0.0f;
};
