#include "SceneGraph.h"
#include "AssetManager.h"
#include "EffectRenderer.h"
#include "Frustum.h"
#include "Log.h"
#include "Physics.h"
#include "Player.h"

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

void SceneGraph::animate(float deltaTime) {
    animationTime += deltaTime;
    for (const auto& emitter : emitters) {
        if (emitter->isVisibleInHierarchy()) emitter->update(deltaTime);
    }
    for (const auto& fire : fires) {
        if (fire->isVisibleInHierarchy()) fire->update(deltaTime);
    }
}

void SceneGraph::drawEffects(EffectRenderer& renderer) {
    for (const auto& water : waters) {
        if (water->isVisibleInHierarchy()) renderer.drawWater(*water);
    }
    for (const auto& shape : shapes) {
        if (shape->gameRole != GameRole::Water || !shape->isVisibleInHierarchy()) continue;
        if (const auto* mesh = dynamic_cast<const MeshShape*>(shape.get())) renderer.drawWaterMesh(*mesh);
    }
    for (const auto& emitter : emitters) {
        if (emitter->isVisibleInHierarchy()) renderer.drawParticles(emitter->system);
    }
    for (const auto& fire : fires) {
        if (!fire->isVisibleInHierarchy()) continue;
        renderer.drawParticles(fire->smokeCloud);
        renderer.drawParticles(fire->flames);
        renderer.drawParticles(fire->sparkShower);
    }
}

UnderwaterView SceneGraph::getUnderwater(glm::vec3 eye) const {
    UnderwaterView view;
    for (const auto& water : waters) {
        if (!water->isVisibleInHierarchy() || !water->contains(glm::vec2(eye.x, eye.z)) || eye.y >= water->getLevel()) continue;
        view.active = true;
        view.depth = water->getLevel() - eye.y;
        view.tint = water->deepColor * 0.5f + water->shallowColor * 0.8f;
        return view;
    }
    for (const WaterArea& area : waterAreas) {
        if (eye.y >= area.level || eye.x < area.area.x || eye.x > area.area.z || eye.z < area.area.y || eye.z > area.area.w) continue;
        view.active = true;
        view.depth = area.level - eye.y;
        return view;
    }
    return view;
}

void SceneGraph::update(float deltaTime) {
    animate(deltaTime);
    Physics::simulate(shapes, deltaTime, ground.isValid() ? &ground : nullptr, &colliders);
}

bool SceneGraph::getLightDirection(glm::vec3& direction) const {
    const LightObject* light = getLight();
    if (!light || !light->isDirectional()) return false;
    direction = light->getDirection();
    return true;
}

void SceneGraph::draw(Shader& lightingShader, Shader& /*lampShader*/, const glm::mat4& view, const glm::mat4& proj) {
    const Frustum frustum(proj * view);
    const glm::vec3 eye = glm::vec3(glm::inverse(view)[3]);

    for (const auto& shape : shapes) {
        if (!shape->isVisibleInHierarchy() || shape->shadowOnly) continue;
        if (shape->gameRole == GameRole::Water && dynamic_cast<MeshShape*>(shape.get())) continue;   // drawn with the effects

        glm::vec3 center;
        float radius;
        shape->getBounds(center, radius);
        if (!frustum.containsSphere(center, radius)) continue;
        // Past the end of the fog, an object is the fog's color whether drawn or not.
        if (fogEnd > 0.0f && glm::length(center - eye) - radius > fogEnd) continue;

        shape->draw(lightingShader);
    }

    if (skybox) {
        skybox->draw(view, proj);
    }
}

void SceneGraph::drawShadow(Shader& shadowShader) {
    drawDepth(shadowShader);
}

void SceneGraph::drawShadowCasters(Shader& depthShader, const glm::mat4& lightSpace) {
    const Frustum frustum(lightSpace);
    for (const auto& shape : shapes) {
        if (!shape->castShadow || !shape->isVisibleInHierarchy()) continue;

        glm::vec3 center;
        float radius;
        shape->getBounds(center, radius);
        if (frustum.containsSphere(center, radius)) {
            shape->draw(depthShader);
        }
    }
}

void SceneGraph::drawDepth(Shader& depthShader) {
    for (const auto& shape : shapes) {
        if (shape->castShadow && shape->isVisibleInHierarchy()) {
            shape->draw(depthShader);
        }
    }
}

int SceneGraph::getLights(glm::vec3 eye, SceneLight* out, int most) const {
    struct Candidate {
        float distance;
        SceneLight light;
    };
    std::vector<Candidate> found;
    const LightObject* sun = getLight();

    for (const auto& object : objects) {
        if (object->getType() != ObjectType::Light || !object->isVisibleInHierarchy()) continue;
        const auto* source = static_cast<const LightObject*>(object.get());

        SceneLight light;
        light.type = source->type == LightType::Directional ? SceneLight::Directional
                     : source->type == LightType::Spot ? SceneLight::Spot : SceneLight::Point;
        light.position = source->getWorldPosition();
        light.direction = source->getDirection();
        light.color = source->effectiveColor(animationTime);
        light.range = std::max(source->range, 0.1f);
        const float outer = glm::radians(std::clamp(source->spotAngle, 1.0f, 89.0f));
        light.spotCosOuter = std::cos(outer);
        light.spotCosInner = std::cos(outer * (1.0f - std::clamp(source->spotSoftness, 0.0f, 1.0f)));
        light.castShadows = source->castShadows;
        light.radius = std::max(source->radius, 0.0f);
        light.specular = source->specular;

        // The sun goes first; the rest by how near they are.
        float distance = source == sun && source->isDirectional() ? -1.0f : 0.0f;
        if (!source->isDirectional()) {
            distance = glm::length(light.position - eye) - light.range;
            if (fogEnd > 0.0f && distance > fogEnd) continue;
        } else if (source != sun) {
            distance = 0.0f;
        }
        found.push_back({distance, light});
    }

    // Fires and lit windows shine like point lights, without shadows.
    glm::vec4 positions[16];
    glm::vec3 colors[16];
    const int extra = getPointLights(eye, positions, colors, 16);
    for (int i = 0; i < extra; i++) {
        SceneLight light;
        light.type = SceneLight::Point;
        light.position = glm::vec3(positions[i]);
        light.range = positions[i].w;
        // A fire at night outshines the moon, and wavers a little.
        const float flicker = 0.88f + 0.12f * std::sin(animationTime * 9.0f + i * 2.3f) * std::sin(animationTime * 5.7f + (float)i);
        light.color = colors[i] * 1.8f * flicker;
        light.castShadows = false;
        found.push_back({glm::length(light.position - eye) - light.range, light});
    }

    const int count = std::min(most, (int)found.size());
    std::partial_sort(found.begin(), found.begin() + count, found.end(),
                      [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    for (int i = 0; i < count; i++) out[i] = found[i].light;
    return count;
}

int SceneGraph::getPointLights(glm::vec3 eye, glm::vec4* positions, glm::vec3* colors, int most) const {
    struct Candidate {
        float distance;
        glm::vec4 position;
        glm::vec3 color;
    };
    std::vector<Candidate> found;
    for (const auto& shape : shapes) {
        if (!shape->emissive || shape->lightRange <= 0.0f || !shape->isVisibleInHierarchy()) continue;
        const glm::vec3 at = shape->getWorldPosition();
        const float distance = glm::length(at - eye) - shape->lightRange;
        if (fogEnd > 0.0f && distance > fogEnd) continue;
        found.push_back({distance, glm::vec4(at, shape->lightRange), shape->lightColor});
    }
    for (const auto& fire : fires) {
        if (fire->lightRange <= 0.0f || !fire->isVisibleInHierarchy() || !fire->burning) continue;
        const glm::vec3 at = fire->getWorldPosition() + glm::vec3(0.0f, 0.5f * fire->size, 0.0f);
        const float distance = glm::length(at - eye) - fire->lightRange;
        if (fogEnd > 0.0f && distance > fogEnd) continue;
        const float level = fire->lightLevel(animationTime);
        found.push_back({distance, glm::vec4(at, fire->lightRange * (0.85f + 0.15f * level)), fire->lightColor * level});
    }
    const int count = std::min(most, (int)found.size());
    std::partial_sort(found.begin(), found.begin() + count, found.end(),
                      [](const Candidate& a, const Candidate& b) { return a.distance < b.distance; });
    for (int i = 0; i < count; i++) {
        positions[i] = found[i].position;
        colors[i] = found[i].color;
    }
    return count;
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
    } else if (auto emitter = std::dynamic_pointer_cast<ParticleEmitter>(object)) {
        emitters.push_back(std::move(emitter));
    } else if (auto fire = std::dynamic_pointer_cast<FireObject>(object)) {
        fires.push_back(std::move(fire));
    } else if (auto water = std::dynamic_pointer_cast<WaterObject>(object)) {
        waters.push_back(std::move(water));
    } else if (auto source = std::dynamic_pointer_cast<AudioSourceObject>(object)) {
        audioSources.push_back(std::move(source));
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
    colliders.clear();

    for (SceneObject* object : doomed) {
        lookup.erase(object->id);
    }

    auto isDoomed = [&](SceneObject* object) {
        return std::find(doomed.begin(), doomed.end(), object) != doomed.end();
    };
    shapes.erase(std::remove_if(shapes.begin(), shapes.end(),
                                [&](const std::shared_ptr<Shape>& s) { return isDoomed(s.get()); }),
                 shapes.end());
    emitters.erase(std::remove_if(emitters.begin(), emitters.end(),
                                  [&](const std::shared_ptr<ParticleEmitter>& e) { return isDoomed(e.get()); }),
                   emitters.end());
    fires.erase(std::remove_if(fires.begin(), fires.end(), [&](const std::shared_ptr<FireObject>& f) { return isDoomed(f.get()); }),
                fires.end());
    waters.erase(std::remove_if(waters.begin(), waters.end(), [&](const std::shared_ptr<WaterObject>& w) { return isDoomed(w.get()); }),
                 waters.end());
    audioSources.erase(std::remove_if(audioSources.begin(), audioSources.end(),
                                      [&](const std::shared_ptr<AudioSourceObject>& a) { return isDoomed(a.get()); }),
                       audioSources.end());
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [&](const std::shared_ptr<SceneObject>& o) { return isDoomed(o.get()); }),
                  objects.end());
}

void SceneGraph::clear() {
    colliders.clear();
    roots.clear();
    lookup.clear();
    shapes.clear();
    emitters.clear();
    fires.clear();
    waters.clear();
    audioSources.clear();
    waterAreas.clear();
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
    // The sun, if there is one; otherwise the first light.
    LightObject* first = nullptr;
    for (const auto& object : objects) {
        if (object->getType() != ObjectType::Light) continue;
        auto* light = static_cast<LightObject*>(object.get());
        if (light->isDirectional()) return light;
        if (!first) first = light;
    }
    return first;
}

CameraObject* SceneGraph::getMainCamera() const {
    for (const auto& object : objects) {
        if (object->getType() == ObjectType::Camera) {
            return static_cast<CameraObject*>(object.get());
        }
    }
    return nullptr;
}

Player* SceneGraph::getPlayer() const {
    for (const auto& shape : shapes) {
        if (shape->getType() == ObjectType::Player) {
            return static_cast<Player*>(shape.get());
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
