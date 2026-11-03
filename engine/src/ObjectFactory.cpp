#include "ObjectFactory.h"
#include "CameraObject.h"
#include "Cube.h"
#include "Cylinder.h"
#include "LightObject.h"
#include "MeshShape.h"
#include "Plane.h"
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
    };
}

std::shared_ptr<SceneObject> ObjectFactory::create(ObjectType type) {
    std::shared_ptr<Shape> shape;

    switch (type) {
        case ObjectType::Empty:    return std::make_shared<SceneObject>();
        case ObjectType::Light:    return std::make_shared<LightObject>();
        case ObjectType::Camera:   return std::make_shared<CameraObject>();
        case ObjectType::Mesh:     shape = std::make_shared<MeshShape>(); break;
        case ObjectType::Cube:     shape = std::make_shared<Cube>(); break;
        case ObjectType::Plane:    shape = std::make_shared<Plane>(); break;
        // Sized to the unit box that collision checks assume.
        case ObjectType::Sphere:   shape = std::make_shared<Sphere>(0.5f); break;
        case ObjectType::Cylinder: shape = std::make_shared<Cylinder>(0.5f, 1.0f, 32); break;
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
