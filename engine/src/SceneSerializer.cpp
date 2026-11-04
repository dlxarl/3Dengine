#include "SceneSerializer.h"
#include "EffectObjects.h"
#include "Log.h"
#include "MeshShape.h"
#include "ObjectFactory.h"
#include "Player.h"
#include "Terrain.h"
#include "UiObject.h"

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
    {GameRole::Ground, "ground"},
    {GameRole::Villager, "villager"},
    {GameRole::Bandit, "bandit"},
    {GameRole::Horse, "horse"},
    {GameRole::PlayerBody, "playerBody"},
    {GameRole::Sword, "sword"},
    {GameRole::Bow, "bow"},
    {GameRole::Arrow, "arrow"},
    {GameRole::Item, "item"},
    {GameRole::Chest, "chest"},
    {GameRole::Bed, "bed"},
    {GameRole::Sign, "sign"},
    {GameRole::Water, "water"},
    {GameRole::Deer, "deer"},
    {GameRole::Wolf, "wolf"},
    {GameRole::Livestock, "livestock"},
    {GameRole::Limb, "limb"},
    {GameRole::NightGlow, "nightGlow"},
    {GameRole::Boar, "boar"},
    {GameRole::Bear, "bear"},
    {GameRole::Rabbit, "rabbit"},
    {GameRole::Bird, "bird"},
    {GameRole::Dragon, "dragon"},
    {GameRole::Fireball, "fireball"},
})

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ItemStack, item, count)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ItemDefinition, id, name, kind, value, heal, damage)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(QuestDefinition, id, title, summary, giver, turnIn, after, offer,
                                                progress, complete, objective, target, count, rewardGold,
                                                rewardXp, rewardItems)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(TradeOffer, item, price, count)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CharacterDefinition, name, lines, stays, sells, buys)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(WorldDefinition, dayMinutes, startHour, startGold, startItems)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(GameDefinition, world, items, quests, characters)

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

    json vec4ToJson(const glm::vec4& v) {
        return json::array({v.x, v.y, v.z, v.w});
    }

    glm::vec4 vec4Field(const json& j, const char* key, glm::vec4 fallback) {
        auto it = j.find(key);
        if (it == j.end() || !it->is_array() || it->size() != 4) return fallback;
        const glm::vec4 v((*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>(), (*it)[3].get<float>());
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.w) ? v : fallback;
    }

    glm::vec2 vec2Field(const json& j, const char* key, glm::vec2 fallback) {
        auto it = j.find(key);
        if (it == j.end() || !it->is_array() || it->size() != 2) return fallback;
        const glm::vec2 v((*it)[0].get<float>(), (*it)[1].get<float>());
        return std::isfinite(v.x) && std::isfinite(v.y) ? v : fallback;
    }

    json particlesToJson(const ParticleSettings& p) {
        return {{"maxParticles", p.maxParticles}, {"rate", p.rate}, {"lifetimeMin", p.lifetimeMin},
                {"lifetimeMax", p.lifetimeMax}, {"speedMin", p.speedMin}, {"speedMax", p.speedMax},
                {"coneDegrees", p.coneDegrees}, {"radius", p.radius}, {"gravity", vec3ToJson(p.gravity)},
                {"drag", p.drag}, {"turbulence", p.turbulence}, {"sizeStart", p.sizeStart}, {"sizeEnd", p.sizeEnd},
                {"spin", p.spin}, {"colorStart", vec4ToJson(p.colorStart)}, {"colorMid", vec4ToJson(p.colorMid)},
                {"colorEnd", vec4ToJson(p.colorEnd)}, {"additive", p.additive}, {"softness", p.softness},
                {"flicker", p.flicker}};
    }

    void particlesFromJson(const json& j, ParticleSettings& p) {
        p.maxParticles = std::clamp(j.value("maxParticles", p.maxParticles), 1, 5000);
        p.rate = std::max(0.0f, j.value("rate", p.rate));
        p.lifetimeMin = std::max(0.05f, j.value("lifetimeMin", p.lifetimeMin));
        p.lifetimeMax = std::max(p.lifetimeMin, j.value("lifetimeMax", p.lifetimeMax));
        p.speedMin = j.value("speedMin", p.speedMin);
        p.speedMax = std::max(p.speedMin, j.value("speedMax", p.speedMax));
        p.coneDegrees = std::clamp(j.value("coneDegrees", p.coneDegrees), 0.0f, 180.0f);
        p.radius = std::max(0.0f, j.value("radius", p.radius));
        p.gravity = vec3Field(j, "gravity", p.gravity);
        p.drag = std::max(0.0f, j.value("drag", p.drag));
        p.turbulence = std::max(0.0f, j.value("turbulence", p.turbulence));
        p.sizeStart = std::max(0.0f, j.value("sizeStart", p.sizeStart));
        p.sizeEnd = std::max(0.0f, j.value("sizeEnd", p.sizeEnd));
        p.spin = j.value("spin", p.spin);
        p.colorStart = vec4Field(j, "colorStart", p.colorStart);
        p.colorMid = vec4Field(j, "colorMid", p.colorMid);
        p.colorEnd = vec4Field(j, "colorEnd", p.colorEnd);
        p.additive = j.value("additive", p.additive);
        p.softness = std::clamp(j.value("softness", p.softness), 0.0f, 1.0f);
        p.flicker = std::clamp(j.value("flicker", p.flicker), 0.0f, 1.0f);
    }

    json objectToJson(const SceneObject& object) {
        json j;
        j["id"] = object.id;
        j["name"] = object.name;
        j["type"] = ObjectFactory::typeKey(object.getType());
        j["parent"] = object.getParent() ? object.getParent()->id : InvalidObjectId;
        j["visible"] = object.visible;
        if (!object.script.empty()) j["script"] = object.script;
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
            if (!shape->gameParameter.empty()) s["gameParameter"] = shape->gameParameter;
            if (shape->limbSwing != 0.0f) s["limbSwing"] = shape->limbSwing;
            if (shape->limbPhase != 0.0f) s["limbPhase"] = shape->limbPhase;
            if (shape->emissive) s["emissive"] = true;
            if (shape->waveHeight != 0.0f) s["waveHeight"] = shape->waveHeight;
            if (shape->sway != 0.0f) s["sway"] = shape->sway;
            if (shape->limbAxis != 0) s["limbAxis"] = shape->limbAxis;
            if (shape->lightRange != 0.0f) {
                s["lightRange"] = shape->lightRange;
                s["lightColor"] = vec3ToJson(shape->lightColor);
            }

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
            if (const auto* player = dynamic_cast<const Player*>(shape)) s["gameControls"] = player->gameControls;
            if (const auto* terrain = dynamic_cast<const TerrainObject*>(shape)) {
                json heights = json::array();
                for (float h : terrain->heights) heights.push_back(std::round(h * 1000.0f) / 1000.0f);
                s["terrain"] = {{"cells", terrain->cells}, {"cellSize", terrain->cellSize},
                                {"atlasColumns", terrain->atlasColumns}, {"atlasRows", terrain->atlasRows},
                                {"heights", std::move(heights)}, {"tiles", json(terrain->tiles)}};
            }
            j["shape"] = std::move(s);
        } else if (const auto* light = dynamic_cast<const LightObject*>(&object)) {
            j["light"] = {{"color", vec3ToJson(light->color)}, {"intensity", light->intensity},
                          {"directional", light->directional}};
        } else if (const auto* emitter = dynamic_cast<const ParticleEmitter*>(&object)) {
            j["particles"] = particlesToJson(emitter->system.settings);
            j["particles"]["playing"] = emitter->system.playing;
        } else if (const auto* fire = dynamic_cast<const FireObject*>(&object)) {
            j["fire"] = {{"size", fire->size}, {"intensity", fire->intensity}, {"smoke", fire->smoke},
                         {"sparks", fire->sparks}, {"lightRange", fire->lightRange},
                         {"lightColor", vec3ToJson(fire->lightColor)}, {"flicker", fire->flicker},
                         {"burning", fire->burning}, {"damage", fire->damage}};
        } else if (const auto* water = dynamic_cast<const WaterObject*>(&object)) {
            j["water"] = {{"size", json::array({water->size.x, water->size.y})}, {"round", water->round},
                          {"waveHeight", water->waveHeight}, {"waveLength", water->waveLength},
                          {"waveSpeed", water->waveSpeed}, {"flow", json::array({water->flow.x, water->flow.y})},
                          {"shallowColor", vec3ToJson(water->shallowColor)}, {"deepColor", vec3ToJson(water->deepColor)},
                          {"opacity", water->opacity}};
        } else if (const auto* source = dynamic_cast<const AudioSourceObject*>(&object)) {
            j["audio"] = {{"clip", source->clip}, {"volume", source->volume}, {"pitch", source->pitch},
                          {"pitchVariation", source->pitchVariation}, {"loop", source->loop}, {"spatial", source->spatial},
                          {"nearDistance", source->nearDistance}, {"farDistance", source->farDistance},
                          {"fadeIn", source->fadeIn}, {"startDelay", source->startDelay},
                          {"repeatMin", source->repeatMin}, {"repeatMax", source->repeatMax}, {"muted", source->muted}};
        } else if (const auto* ui = dynamic_cast<const UiObject*>(&object)) {
            j["ui"] = {{"kind", (int)ui->kind}, {"anchor", (int)ui->anchor},
                       {"offset", json::array({ui->offset.x, ui->offset.y})}, {"size", json::array({ui->size.x, ui->size.y})},
                       {"stretch", ui->stretch}, {"color", vec4ToJson(ui->color)}, {"background", vec4ToJson(ui->background)},
                       {"rounding", ui->rounding}, {"text", ui->text}, {"fontSize", ui->fontSize}, {"align", ui->align},
                       {"image", ui->image}, {"fill", ui->fill}, {"binding", (int)ui->binding}};
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
        object->script = j.value("script", std::string());
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
            shape->gameParameter = s.value("gameParameter", shape->gameParameter);
            shape->limbSwing = s.value("limbSwing", shape->limbSwing);
            shape->limbPhase = s.value("limbPhase", shape->limbPhase);
            shape->emissive = s.value("emissive", shape->emissive);
            shape->waveHeight = s.value("waveHeight", shape->waveHeight);
            shape->sway = s.value("sway", shape->sway);
            shape->limbAxis = std::clamp(s.value("limbAxis", shape->limbAxis), 0, 2);
            shape->lightRange = s.value("lightRange", shape->lightRange);
            shape->lightColor = vec3Field(s, "lightColor", shape->lightColor);

            for (const json& texture : s.value("textures", json::array())) {
                const std::string textureType = texture.value("type", std::string("texture_albedo"));
                if (auto loaded = assets.getTexture(texture.at("path").get<std::string>(), textureType)) {
                    shape->setTexture(textureType, std::move(loaded));
                }
            }

            // Scenes from before this option were all driven by the game rules.
            if (auto* player = dynamic_cast<Player*>(shape)) player->gameControls = s.value("gameControls", true);

            if (auto* terrain = dynamic_cast<TerrainObject*>(shape)) {
                const json t = s.value("terrain", json::object());
                terrain->reset(t.value("cells", terrain->cells), t.value("cellSize", terrain->cellSize));
                terrain->atlasColumns = std::clamp(t.value("atlasColumns", 1), 1, 64);
                terrain->atlasRows = std::clamp(t.value("atlasRows", 1), 1, 64);
                const std::vector<float> heights = t.value("heights", std::vector<float>());
                if (heights.size() == terrain->heights.size()) terrain->heights = heights;
                const std::vector<uint8_t> tiles = t.value("tiles", std::vector<uint8_t>());
                if (tiles.size() == terrain->tiles.size()) terrain->tiles = tiles;
            } else if (auto* mesh = dynamic_cast<MeshShape*>(shape)) {
                const std::string assetPath = s.value("mesh", std::string());
                mesh->setMesh(assets.getMesh(assetPath), assetPath);
            }
        } else if (auto* light = dynamic_cast<LightObject*>(object.get())) {
            const json l = j.value("light", json::object());
            light->color = vec3Field(l, "color", light->color);
            light->intensity = l.value("intensity", light->intensity);
            light->directional = l.value("directional", light->directional);
        } else if (auto* emitter = dynamic_cast<ParticleEmitter*>(object.get())) {
            const json p = j.value("particles", json::object());
            particlesFromJson(p, emitter->system.settings);
            emitter->system.playing = p.value("playing", true);
        } else if (auto* fire = dynamic_cast<FireObject*>(object.get())) {
            const json f = j.value("fire", json::object());
            fire->size = std::clamp(f.value("size", fire->size), 0.05f, 50.0f);
            fire->intensity = std::clamp(f.value("intensity", fire->intensity), 0.0f, 10.0f);
            fire->smoke = std::clamp(f.value("smoke", fire->smoke), 0.0f, 5.0f);
            fire->sparks = std::clamp(f.value("sparks", fire->sparks), 0.0f, 5.0f);
            fire->lightRange = std::max(0.0f, f.value("lightRange", fire->lightRange));
            fire->lightColor = vec3Field(f, "lightColor", fire->lightColor);
            fire->flicker = std::clamp(f.value("flicker", fire->flicker), 0.0f, 1.0f);
            fire->burning = f.value("burning", fire->burning);
            fire->damage = std::clamp(f.value("damage", fire->damage), 0.0f, 200.0f);
            fire->configure();
        } else if (auto* water = dynamic_cast<WaterObject*>(object.get())) {
            const json w = j.value("water", json::object());
            water->size = glm::max(vec2Field(w, "size", water->size), glm::vec2(0.1f));
            water->round = w.value("round", water->round);
            water->waveHeight = std::max(0.0f, w.value("waveHeight", water->waveHeight));
            water->waveLength = std::max(0.2f, w.value("waveLength", water->waveLength));
            water->waveSpeed = w.value("waveSpeed", water->waveSpeed);
            water->flow = vec2Field(w, "flow", water->flow);
            water->shallowColor = vec3Field(w, "shallowColor", water->shallowColor);
            water->deepColor = vec3Field(w, "deepColor", water->deepColor);
            water->opacity = std::clamp(w.value("opacity", water->opacity), 0.0f, 1.0f);
        } else if (auto* source = dynamic_cast<AudioSourceObject*>(object.get())) {
            const json a = j.value("audio", json::object());
            source->clip = a.value("clip", source->clip);
            source->volume = std::clamp(a.value("volume", source->volume), 0.0f, 2.0f);
            source->pitch = std::clamp(a.value("pitch", source->pitch), 0.25f, 4.0f);
            source->pitchVariation = std::clamp(a.value("pitchVariation", source->pitchVariation), 0.0f, 0.5f);
            source->loop = a.value("loop", source->loop);
            source->spatial = a.value("spatial", source->spatial);
            source->nearDistance = std::max(0.1f, a.value("nearDistance", source->nearDistance));
            source->farDistance = std::max(source->nearDistance + 0.1f, a.value("farDistance", source->farDistance));
            source->fadeIn = std::clamp(a.value("fadeIn", source->fadeIn), 0.0f, 60.0f);
            source->startDelay = std::clamp(a.value("startDelay", source->startDelay), 0.0f, 3600.0f);
            source->repeatMin = std::clamp(a.value("repeatMin", source->repeatMin), 0.0f, 3600.0f);
            source->repeatMax = std::clamp(a.value("repeatMax", source->repeatMax), source->repeatMin, 3600.0f);
            source->muted = a.value("muted", source->muted);
        } else if (auto* ui = dynamic_cast<UiObject*>(object.get())) {
            const json u = j.value("ui", json::object());
            ui->kind = (UiKind)std::clamp(u.value("kind", (int)ui->kind), 0, (int)UiKind::Button);
            ui->anchor = (UiAnchor)std::clamp(u.value("anchor", (int)ui->anchor), 0, (int)UiAnchor::BottomRight);
            ui->offset = vec2Field(u, "offset", ui->offset);
            ui->size = glm::max(vec2Field(u, "size", ui->size), glm::vec2(0.0f));
            ui->stretch = u.value("stretch", ui->stretch);
            ui->color = vec4Field(u, "color", ui->color);
            ui->background = vec4Field(u, "background", ui->background);
            ui->rounding = std::max(0.0f, u.value("rounding", ui->rounding));
            ui->text = u.value("text", ui->text);
            ui->fontSize = std::clamp(u.value("fontSize", ui->fontSize), 4.0f, 400.0f);
            ui->align = std::clamp(u.value("align", ui->align), 0, 2);
            ui->image = u.value("image", ui->image);
            ui->fill = std::clamp(u.value("fill", ui->fill), 0.0f, 1.0f);
            ui->binding = (UiBinding)std::clamp(u.value("binding", (int)ui->binding), 0, (int)UiBinding::Stamina);
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
    j["environment"] = {{"clearColor", vec3ToJson(scene.clearColor)}, {"skybox", scene.getSkyboxAsset()},
                        {"fogStart", scene.fogStart}, {"fogEnd", scene.fogEnd},
                        {"ambientColor", vec3ToJson(scene.ambientColor)}, {"gradientSky", scene.gradientSky}};
    if (!scene.game.isEmpty()) {
        j["game"] = scene.game;
    }
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
        scene.fogStart = environment.value("fogStart", scene.fogStart);
        scene.fogEnd = environment.value("fogEnd", scene.fogEnd);
        scene.ambientColor = vec3Field(environment, "ambientColor", scene.ambientColor);
        scene.gradientSky = environment.value("gradientSky", false);

        if (j.contains("game")) {
            j.at("game").get_to(scene.game);
        }
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
