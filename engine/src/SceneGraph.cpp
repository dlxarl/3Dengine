#include "SceneGraph.h"
#include "AssetManager.h"
#include "Log.h"
#include "Physics.h"

#include <algorithm>

namespace {
    const glm::vec3 DEFAULT_LIGHT_POS(8.0f, 16.0f, 10.0f);
}

CameraView CameraObject::makeView(float aspectRatio) const {
    const glm::vec3 eye = glm::vec3(model[3]);
    const glm::vec3 forward = -glm::normalize(glm::vec3(model[2]));
    const glm::vec3 up = glm::normalize(glm::vec3(model[1]));

    CameraView camera;
    camera.position = eye;
    camera.view = glm::lookAt(eye, eye + forward, up);

    if (orthographic) {
        const float halfWidth = orthographicSize * aspectRatio;
        camera.projection = glm::ortho(-halfWidth, halfWidth, -orthographicSize, orthographicSize, nearPlane, farPlane);
    } else {
        camera.projection = glm::perspective(glm::radians(fieldOfView), aspectRatio, nearPlane, farPlane);
    }
    return camera;
}

SceneGraph::SceneGraph() = default;

void SceneGraph::update(float deltaTime) {
    Physics::step(shapes, deltaTime);
}

void SceneGraph::draw(Shader& lightingShader, Shader& /*lampShader*/, const glm::mat4& view, const glm::mat4& proj) {
    for (const auto& shape : shapes) {
        if (shape->isVisibleInHierarchy()) {
            shape->draw(lightingShader);
        }
    }

    if (skybox) {
        skybox->draw(view, proj);
    }
}

void SceneGraph::drawShadow(Shader& shadowShader) {
    drawDepth(shadowShader);
}

void SceneGraph::drawDepth(Shader& depthShader) {
    for (const auto& shape : shapes) {
        if (shape->castShadow && shape->isVisibleInHierarchy()) {
            shape->draw(depthShader);
        }
    }
}

glm::vec3 SceneGraph::getLightPos() const {
    const LightObject* light = getLight();
    return light ? light->getWorldPosition() : DEFAULT_LIGHT_POS;
}

glm::vec3 SceneGraph::getLightColor() const {
    const LightObject* light = getLight();
    return light ? light->color * light->intensity : glm::vec3(1.0f);
}

SceneObject* SceneGraph::add(std::shared_ptr<SceneObject> object, SceneObject* parent, int childIndex) {
    SceneObject* raw = object.get();

    if (raw->id == InvalidObjectId || lookup.count(raw->id) != 0) {
        raw->id = nextId++;
    }
    nextId = std::max(nextId, raw->id + 1);
    lookup[raw->id] = raw;

    if (auto shape = std::dynamic_pointer_cast<Shape>(object)) {
        shapes.push_back(std::move(shape));
    }
    objects.push_back(std::move(object));

    if (parent) {
        raw->setParent(parent, childIndex);
    } else if (childIndex < 0 || childIndex > (int)roots.size()) {
        roots.push_back(raw);
    } else {
        roots.insert(roots.begin() + childIndex, raw);
    }
    return raw;
}

void SceneGraph::remove(ObjectId id) {
    SceneObject* target = find(id);
    if (!target) return;

    std::vector<SceneObject*> doomed{target};
    for (size_t i = 0; i < doomed.size(); i++) {
        const auto& children = doomed[i]->getChildren();
        doomed.insert(doomed.end(), children.begin(), children.end());
    }

    roots.erase(std::remove(roots.begin(), roots.end(), target), roots.end());

    for (SceneObject* object : doomed) {
        lookup.erase(object->id);
    }

    auto isDoomed = [&](SceneObject* object) {
        return std::find(doomed.begin(), doomed.end(), object) != doomed.end();
    };
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
                                [&](const std::shared_ptr<Shape>& s) { return isDoomed(s.get()); }),
                 shapes.end());
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [&](const std::shared_ptr<SceneObject>& o) { return isDoomed(o.get()); }),
                  objects.end());
}

void SceneGraph::clear() {
    roots.clear();
    lookup.clear();
    shapes.clear();
    objects.clear();
    nextId = 1;
}

SceneObject* SceneGraph::find(ObjectId id) const {
    auto it = lookup.find(id);
    return it == lookup.end() ? nullptr : it->second;
}

bool SceneGraph::reparent(ObjectId id, ObjectId newParent, int childIndex) {
    SceneObject* object = find(id);
    SceneObject* parent = newParent == InvalidObjectId ? nullptr : find(newParent);

    if (!object || (newParent != InvalidObjectId && !parent)) return false;
    if (parent == object || (parent && parent->isDescendantOf(object))) return false;

    const SceneObject* oldParent = object->getParent();

    auto insertRoot = [&] {
        if (childIndex < 0 || childIndex > (int)roots.size()) {
            roots.push_back(object);
        } else {
            roots.insert(roots.begin() + childIndex, object);
        }
    };

    if (!oldParent) {
        roots.erase(std::remove(roots.begin(), roots.end(), object), roots.end());
    }

    // Reordering among siblings leaves the transform alone.
    if (parent == oldParent) {
        if (parent) {
            object->setChildIndex(childIndex < 0 ? (int)parent->getChildren().size() : childIndex);
        } else {
            insertRoot();
        }
        return true;
    }

    const glm::mat4 world = object->getWorldMatrix();
    object->setParent(parent, childIndex);
    if (!parent) insertRoot();
    object->setWorldMatrix(world);
    return true;
}

std::string SceneGraph::makeUniqueName(const std::string& baseName) const {
    auto inUse = [&](const std::string& name) {
        return std::any_of(objects.begin(), objects.end(),
                           [&](const std::shared_ptr<SceneObject>& o) { return o->name == name; });
    };

    if (!inUse(baseName)) return baseName;

    for (int suffix = 1;; suffix++) {
        std::string candidate = baseName + " (" + std::to_string(suffix) + ")";
        if (!inUse(candidate)) return candidate;
    }
}

LightObject* SceneGraph::getLight() const {
    for (const auto& object : objects) {
        if (object->getType() == ObjectType::Light) {
            return static_cast<LightObject*>(object.get());
        }
    }
    return nullptr;
}

CameraObject* SceneGraph::getMainCamera() const {
    for (const auto& object : objects) {
        if (object->getType() == ObjectType::Camera) {
            return static_cast<CameraObject*>(object.get());
        }
    }
    return nullptr;
}

void SceneGraph::setSkybox(const std::string& assetPath, AssetManager& assets) {
    if (assetPath == skyboxAsset) return;

    skyboxAsset = assetPath;
    skybox.reset();

    if (assetPath.empty()) return;

    const auto source = assets.resolve(assetPath);
    if (source.empty()) {
        Log::error("Assets", "Invalid skybox path: " + assetPath);
        return;
    }
    skybox = std::make_unique<Skybox>(source.u8string());
}
