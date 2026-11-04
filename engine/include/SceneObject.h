#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <string>
#include <vector>

using ObjectId = uint32_t;
constexpr ObjectId InvalidObjectId = 0;

enum class ObjectType { Empty, Cube, Plane, Sphere, Cylinder, Mesh, Light, Camera, Player, Particles, Fire, Water, AudioSource, Ui, Terrain };

// Anything that lives in a scene: a stable identity, a name, a transform and a place in the
// hierarchy. A plain SceneObject is an empty group node; Shape adds geometry.
class SceneObject {
public:
    SceneObject();
    virtual ~SceneObject();

    SceneObject(const SceneObject&) = delete;
    SceneObject& operator=(const SceneObject&) = delete;

    virtual ObjectType getType() const { return ObjectType::Empty; }

    void setPosition(glm::vec3 pos);
    void rotate(float angle, glm::vec3 axis);
    void setRotation(glm::vec3 eulerDegrees);
    void setScale(glm::vec3 scaleVec);

    glm::vec3 position;
    glm::vec3 scale;
    glm::vec3 rotation;

    ObjectId id = InvalidObjectId;
    std::string name;
    bool visible = true;

    // The class name of the script this object runs while the scene plays; empty for none.
    std::string script;

    SceneObject* getParent() const { return parent; }
    const std::vector<SceneObject*>& getChildren() const { return children; }
    bool isDescendantOf(const SceneObject* ancestor) const;
    bool isVisibleInHierarchy() const;

    // Attaches under newParent (nullptr detaches) at childIndex, or at the end when negative.
    void setParent(SceneObject* newParent, int childIndex = -1);

    // Moves this object within its parent's child list.
    void setChildIndex(int childIndex);

    glm::mat4 getLocalMatrix() const;
    const glm::mat4& getWorldMatrix() const { return model; }
    glm::vec3 getWorldPosition() const;
    glm::vec3 getWorldScale() const;

    // Sets the local transform so that the object ends up at this world transform.
    void setWorldMatrix(const glm::mat4& world);

protected:
    glm::mat4 model;

    void updateModelMatrix();

private:
    SceneObject* parent = nullptr;
    std::vector<SceneObject*> children;

    void detachFromParent();
};

// Splits a translate * rotate * scale matrix back into the engine's XYZ Euler convention.
void decomposeTransform(const glm::mat4& matrix, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale);
