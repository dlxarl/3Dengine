#include "SceneSerializer.h"
#include "Log.h"
#include "MeshShape.h"
#include "ObjectFactory.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(GameRole, {
    {GameRole::None, "none"},
    {GameRole::Pickup, "pickup"},
    {GameRole::Goal, "goal"},
    {GameRole::Hazard, "hazard"},
})

namespace {
    const int SCENE_FORMAT_VERSION = 1;

    json vec3ToJson(const glm::vec3& v) {
        return json::array({v.x, v.y, v.z});
    }

    glm::vec3 vec3FromJson(const json& j, glm::vec3 fallback) {
        if (!j.is_array() || j.size() != 3) return fallback;
        glm::vec3 v(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)) return fallback;
        return v;
    }

    glm::vec3 vec3Field(const json& j, const char* key, glm::vec3 fallback) {
        auto it = j.find(key);
        return it == j.end() ? fallback : vec3FromJson(*it, fallback);
    }

    json objectToJson(const SceneObject& object) {
        json j;
        j["id"] = object.id;
        j["name"] = object.name;
        j["type"] = ObjectFactory::typeKey(object.getType());
        j["parent"] = object.getParent() ? object.getParent()->id : InvalidObjectId;
        j["visible"] = object.visible;
        j["position"] = vec3ToJson(object.position);
        j["rotation"] = vec3ToJson(object.rotation);
        j["scale"] = vec3ToJson(object.scale);

        if (const auto* shape = dynamic_cast<const Shape*>(&object)) {
            json s;
            s["color"] = vec3ToJson(shape->getColor());
            s["castShadow"] = shape->castShadow;
            s["tintTexture"] = shape->tintTexture;
            s["useGravity"] = shape->useGravity;
            s["isStatic"] = shape->isStatic;
            s["hasCollision"] = shape->hasCollision;
            s["gameRole"] = shape->gameRole;
            s["patrolOffset"] = vec3ToJson(shape->patrolOffset);
            s["patrolSeconds"] = shape->patrolSeconds;

            json textures = json::array();
            for (const auto& texture : shape->getTextures()) {
                if (!texture->assetPath.empty()) {
                    textures.push_back({{"type", texture->type}, {"path", texture->assetPath}});
                }
            }
            s["textures"] = std::move(textures);

            if (const auto* mesh = dynamic_cast<const MeshShape*>(shape)) {
                s["mesh"] = mesh->getAssetPath();
            }
            j["shape"] = std::move(s);
        } else if (const auto* light = dynamic_cast<const LightObject*>(&object)) {
            j["light"] = {{"color", vec3ToJson(light->color)}, {"intensity", light->intensity}};
        } else if (const auto* camera = dynamic_cast<const CameraObject*>(&object)) {
            j["camera"] = {{"fieldOfView", camera->fieldOfView},
                           {"nearPlane", camera->nearPlane},
                           {"farPlane", camera->farPlane},
                           {"orthographic", camera->orthographic},
                           {"orthographicSize", camera->orthographicSize},
                           {"firstPerson", camera->firstPerson}};
        }
        return j;
    }

    std::shared_ptr<SceneObject> objectFromJson(const json& j, AssetManager& assets) {
        ObjectType type;
        const std::string key = j.at("type").get<std::string>();
        if (!ObjectFactory::typeFromKey(key, type)) {
            Log::warn("Scene", "Skipping object of unknown type: " + key);
            return nullptr;
        }

        std::shared_ptr<SceneObject> object = ObjectFactory::create(type);
        object->name = j.value("name", std::string(ObjectFactory::typeName(type)));
        object->visible = j.value("visible", true);
        object->setPosition(vec3Field(j, "position", object->position));
        object->setRotation(vec3Field(j, "rotation", object->rotation));
        object->setScale(vec3Field(j, "scale", object->scale));

        if (auto* shape = dynamic_cast<Shape*>(object.get())) {
            const json s = j.value("shape", json::object());
            shape->setColor(vec3Field(s, "color", shape->getColor()));
            shape->castShadow = s.value("castShadow", shape->castShadow);
            shape->tintTexture = s.value("tintTexture", shape->tintTexture);
            shape->useGravity = s.value("useGravity", shape->useGravity);
            shape->isStatic = s.value("isStatic", shape->isStatic);
            shape->hasCollision = s.value("hasCollision", shape->hasCollision);
            shape->gameRole = s.value("gameRole", shape->gameRole);
            shape->patrolOffset = vec3Field(s, "patrolOffset", shape->patrolOffset);
            shape->patrolSeconds = s.value("patrolSeconds", shape->patrolSeconds);

            for (const json& texture : s.value("textures", json::array())) {
                const std::string textureType = texture.value("type", std::string("texture_albedo"));
                if (auto loaded = assets.getTexture(texture.at("path").get<std::string>(), textureType)) {
                    shape->setTexture(textureType, std::move(loaded));
                }
            }

            if (auto* mesh = dynamic_cast<MeshShape*>(shape)) {
                const std::string assetPath = s.value("mesh", std::string());
                mesh->setMesh(assets.getMesh(assetPath), assetPath);
            }
        } else if (auto* light = dynamic_cast<LightObject*>(object.get())) {
            const json l = j.value("light", json::object());
            light->color = vec3Field(l, "color", light->color);
            light->intensity = l.value("intensity", light->intensity);
        } else if (auto* camera = dynamic_cast<CameraObject*>(object.get())) {
            const json c = j.value("camera", json::object());
            camera->fieldOfView = c.value("fieldOfView", camera->fieldOfView);
            camera->nearPlane = c.value("nearPlane", camera->nearPlane);
            camera->farPlane = c.value("farPlane", camera->farPlane);
            camera->orthographic = c.value("orthographic", camera->orthographic);
            camera->orthographicSize = c.value("orthographicSize", camera->orthographicSize);
            camera->firstPerson = c.value("firstPerson", camera->firstPerson);
        }
        return object;
    }

    // Parents come before their children, and siblings keep their order.
    void appendSubtree(const SceneObject& object, json& out) {
        out.push_back(objectToJson(object));
        for (const SceneObject* child : object.getChildren()) {
            appendSubtree(*child, out);
        }
    }
}

std::string SceneSerializer::serialize(const SceneGraph& scene) {
    json objects = json::array();
    for (const SceneObject* root : scene.getRoots()) {
        appendSubtree(*root, objects);
    }

    json j;
    j["version"] = SCENE_FORMAT_VERSION;
    j["nextObjectId"] = scene.getNextId();
    j["environment"] = {{"clearColor", vec3ToJson(scene.clearColor)}, {"skybox", scene.getSkyboxAsset()}};
    j["objects"] = std::move(objects);
    return j.dump(2);
}

bool SceneSerializer::deserialize(const std::string& text, SceneGraph& scene, AssetManager& assets, std::string* error) {
    try {
        const json j = json::parse(text);

        const int version = j.value("version", 0);
        if (version < 1 || version > SCENE_FORMAT_VERSION) {
            throw std::runtime_error("Unsupported scene format version " + std::to_string(version));
        }

        const json& objects = j.at("objects");

        std::unordered_map<ObjectId, SceneObject*> byFileId;
        std::vector<std::pair<SceneObject*, ObjectId>> pendingParents;

        for (const json& entry : objects) {
            const ObjectId id = entry.at("id").get<ObjectId>();
            if (id == InvalidObjectId || byFileId.count(id) != 0) {
                throw std::runtime_error("Duplicate or invalid object id " + std::to_string(id));
            }

            std::shared_ptr<SceneObject> object = objectFromJson(entry, assets);
            if (!object) continue;

            object->id = id;
            SceneObject* added = scene.add(std::move(object));
            byFileId[id] = added;
            pendingParents.emplace_back(added, entry.value("parent", InvalidObjectId));
        }

        for (const auto& [object, parentId] : pendingParents) {
            if (parentId == InvalidObjectId) continue;

            auto parent = byFileId.find(parentId);
            if (parent == byFileId.end()) {
                Log::warn("Scene", "Object '" + object->name + "' refers to a missing parent; keeping it at the root");
                continue;
            }

            // The file stores local transforms, which reparent() must not adjust.
            const glm::vec3 position = object->position;
            const glm::vec3 rotation = object->rotation;
            const glm::vec3 objectScale = object->scale;
            if (!scene.reparent(object->id, parentId)) {
                Log::warn("Scene", "Object '" + object->name + "' has a cyclic parent; keeping it at the root");
                continue;
            }
            object->setPosition(position);
            object->setRotation(rotation);
            object->setScale(objectScale);
        }

        scene.setNextId(std::max(scene.getNextId(), j.value("nextObjectId", ObjectId(1))));

        const json environment = j.value("environment", json::object());
        scene.clearColor = vec3Field(environment, "clearColor", scene.clearColor);
        scene.setSkybox(environment.value("skybox", std::string()), assets);
        return true;
    } catch (const std::exception& e) {
        scene.clear();
        if (error) *error = e.what();
        return false;
    }
}

bool SceneSerializer::saveFile(const SceneGraph& scene, const fs::path& path, std::string* error) {
    // Written beside the destination and moved over it, so a failed save cannot truncate a scene.
    fs::path temporary = path;
    temporary += ".tmp";

    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << serialize(scene);
        if (!file.good()) {
            if (error) *error = "Cannot write " + temporary.u8string();
            return false;
        }
    }

    std::error_code ec;
    fs::rename(temporary, path, ec);
    if (ec) {
        fs::remove(temporary, ec);
        if (error) *error = "Cannot replace " + path.u8string();
        return false;
    }
    return true;
}

bool SceneSerializer::loadFile(const fs::path& path, SceneGraph& scene, AssetManager& assets, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        if (error) *error = "Cannot open " + path.u8string();
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return deserialize(buffer.str(), scene, assets, error);
}

SceneObject* SceneSerializer::duplicate(SceneGraph& scene, ObjectId id, AssetManager& assets) {
    SceneObject* source = scene.find(id);
    if (!source) return nullptr;

    json entries = json::array();
    appendSubtree(*source, entries);

    SceneObject* parent = source->getParent();
    const auto& siblings = parent ? parent->getChildren() : scene.getRoots();
    const int sourceIndex = (int)(std::find(siblings.begin(), siblings.end(), source) - siblings.begin());

    std::unordered_map<ObjectId, SceneObject*> copies;
    SceneObject* rootCopy = nullptr;

    for (const json& entry : entries) {
        std::shared_ptr<SceneObject> copy = objectFromJson(entry, assets);
        if (!copy) continue;

        const ObjectId originalId = entry.at("id").get<ObjectId>();
        const ObjectId originalParent = entry.value("parent", InvalidObjectId);

        if (!rootCopy) {
            copy->name = scene.makeUniqueName(copy->name);
            rootCopy = scene.add(std::move(copy), parent, sourceIndex + 1);
            copies[originalId] = rootCopy;
        } else {
            auto copiedParent = copies.find(originalParent);
            SceneObject* attachTo = copiedParent != copies.end() ? copiedParent->second : rootCopy;
            copies[originalId] = scene.add(std::move(copy), attachTo);
        }
    }
    return rootCopy;
}
