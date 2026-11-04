#include "ObjectFactory.h"
#include "CameraObject.h"
#include "Cube.h"
#include "Cylinder.h"
#include "EffectObjects.h"
#include "LightObject.h"
#include "MeshShape.h"
#include "Plane.h"
#include "Player.h"
#include "UiObject.h"
#include "Sphere.h"

#include <glm/gtc/matrix_transform.hpp>

namespace {
    struct TypeInfo {
        ObjectType type;
        const char* name;
        const char* key;
    };

    const TypeInfo typeTable[] = {
        {ObjectType::Empty, "Empty", "empty"},
        {ObjectType::Cube, "Cube", "cube"},
        {ObjectType::Plane, "Plane", "plane"},
        {ObjectType::Sphere, "Sphere", "sphere"},
        {ObjectType::Cylinder, "Cylinder", "cylinder"},
        {ObjectType::Mesh, "Mesh", "mesh"},
        {ObjectType::Light, "Light", "light"},
        {ObjectType::Camera, "Camera", "camera"},
        {ObjectType::Player, "Player", "player"},
        {ObjectType::Particles, "Particles", "particles"},
        {ObjectType::Fire, "Fire", "fire"},
        {ObjectType::Water, "Water", "water"},
        {ObjectType::AudioSource, "Audio Source", "audioSource"},
        {ObjectType::Ui, "2D Element", "ui"},
    };
}

std::shared_ptr<SceneObject> ObjectFactory::create(ObjectType type) {
    std::shared_ptr<Shape> shape;

    switch (type) {
        case ObjectType::Empty:    return std::make_shared<SceneObject>();
        case ObjectType::Light:    return std::make_shared<LightObject>();
        case ObjectType::Camera:   return std::make_shared<CameraObject>();
        case ObjectType::Particles: return std::make_shared<ParticleEmitter>();
        case ObjectType::Fire:     return std::make_shared<FireObject>();
        case ObjectType::Water:    return std::make_shared<WaterObject>();
        case ObjectType::AudioSource: return std::make_shared<AudioSourceObject>();
        case ObjectType::Ui: return std::make_shared<UiObject>();
        case ObjectType::Mesh:     shape = std::make_shared<MeshShape>(); break;
        case ObjectType::Cube:     shape = std::make_shared<Cube>(); break;
        case ObjectType::Plane:    shape = std::make_shared<Plane>(); break;
        // Sized to the unit box that collision checks assume.
        case ObjectType::Sphere:   shape = std::make_shared<Sphere>(0.5f); break;
        case ObjectType::Cylinder: shape = std::make_shared<Cylinder>(0.5f, 1.0f, 32); break;
        case ObjectType::Player: {
            // A new player does nothing by itself: movement and the rest come from scripts.
            auto player = std::make_shared<Player>(glm::vec3(0.0f));
            player->gameControls = false;
            shape = std::move(player);
            break;
        }
    }

    shape->setColor(glm::vec3(0.8f));
    shape->tintTexture = true;

    if (type == ObjectType::Plane) {
        shape->setScale(glm::vec3(10.0f, 0.1f, 10.0f));
        shape->isStatic = true;
    }
    return shape;
}

const char* ObjectFactory::typeName(ObjectType type) {
    for (const TypeInfo& info : typeTable) {
        if (info.type == type) return info.name;
    }
    return "Object";
}

const char* ObjectFactory::typeKey(ObjectType type) {
    for (const TypeInfo& info : typeTable) {
        if (info.type == type) return info.key;
    }
    return "empty";
}

bool ObjectFactory::typeFromKey(const std::string& key, ObjectType& type) {
    for (const TypeInfo& info : typeTable) {
        if (key == info.key) {
            type = info.type;
            return true;
        }
    }
    return false;
}
