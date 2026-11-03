#pragma once
#include "SceneObject.h"
#include <memory>
#include <string>

// Creates scene objects with the defaults new objects start from.
namespace ObjectFactory {
    // A Mesh starts out empty; give it an asset with MeshShape::setMesh().
    std::shared_ptr<SceneObject> create(ObjectType type);

    const char* typeName(ObjectType type);      // the display and object-naming form: "Cube"
    const char* typeKey(ObjectType type);       // the stable form in scene files: "cube"
    bool typeFromKey(const std::string& key, ObjectType& type);
}
