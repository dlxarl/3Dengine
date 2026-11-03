#include "SceneObject.h"

#include <algorithm>
#include <cmath>

SceneObject::SceneObject() {
    model = glm::mat4(1.0f);
    position = glm::vec3(0.0f);
    scale = glm::vec3(1.0f);
    rotation = glm::vec3(0.0f);
}

SceneObject::~SceneObject() {
    detachFromParent();
    for (SceneObject* child : children) {
        child->parent = nullptr;
    }
}

void SceneObject::setPosition(glm::vec3 pos) {
    position = pos;
    updateModelMatrix();
}

void SceneObject::rotate(float angle, glm::vec3 axis) {
    rotation += axis * angle;
    updateModelMatrix();
}

void SceneObject::setRotation(glm::vec3 eulerDegrees) {
    rotation = eulerDegrees;
    updateModelMatrix();
}

void SceneObject::setScale(glm::vec3 scaleVec) {
    scale = scaleVec;
    updateModelMatrix();
}

bool SceneObject::isDescendantOf(const SceneObject* ancestor) const {
    for (const SceneObject* node = parent; node != nullptr; node = node->parent) {
        if (node == ancestor) return true;
    }
    return false;
}

bool SceneObject::isVisibleInHierarchy() const {
    for (const SceneObject* node = this; node != nullptr; node = node->parent) {
        if (!node->visible) return false;
    }
    return true;
}

void SceneObject::detachFromParent() {
    if (!parent) return;
    auto& siblings = parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    parent = nullptr;
}

void SceneObject::setParent(SceneObject* newParent, int childIndex) {
    if (newParent == this || (newParent && newParent->isDescendantOf(this))) return;

    detachFromParent();
    parent = newParent;
    if (parent) {
        auto& siblings = parent->children;
        if (childIndex < 0 || childIndex > (int)siblings.size()) {
            siblings.push_back(this);
        } else {
            siblings.insert(siblings.begin() + childIndex, this);
        }
    }
    updateModelMatrix();
}

void SceneObject::setChildIndex(int childIndex) {
    if (!parent) return;
    auto& siblings = parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    childIndex = std::clamp(childIndex, 0, (int)siblings.size());
    siblings.insert(siblings.begin() + childIndex, this);
}

glm::mat4 SceneObject::getLocalMatrix() const {
    glm::mat4 local(1.0f);
    local = glm::translate(local, position);
    local = glm::rotate(local, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    local = glm::rotate(local, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    local = glm::rotate(local, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::scale(local, scale);
}

void SceneObject::updateModelMatrix() {
    model = parent ? parent->model * getLocalMatrix() : getLocalMatrix();
    for (SceneObject* child : children) {
        child->updateModelMatrix();
    }
}

glm::vec3 SceneObject::getWorldPosition() const {
    return parent ? glm::vec3(model[3]) : position;
}

glm::vec3 SceneObject::getWorldScale() const {
    if (!parent) return scale;
    return glm::vec3(glm::length(glm::vec3(model[0])),
                     glm::length(glm::vec3(model[1])),
                     glm::length(glm::vec3(model[2])));
}

void SceneObject::setWorldMatrix(const glm::mat4& world) {
    const glm::mat4 local = parent ? glm::inverse(parent->model) * world : world;
    decomposeTransform(local, position, rotation, scale);
    updateModelMatrix();
}

void decomposeTransform(const glm::mat4& matrix, glm::vec3& position, glm::vec3& eulerDegrees, glm::vec3& scale) {
    position = glm::vec3(matrix[3]);

    scale = glm::vec3(glm::length(glm::vec3(matrix[0])),
                      glm::length(glm::vec3(matrix[1])),
                      glm::length(glm::vec3(matrix[2])));

    glm::mat3 r(1.0f);
    for (int axis = 0; axis < 3; axis++) {
        r[axis] = scale[axis] > 0.0f ? glm::vec3(matrix[axis]) / scale[axis] : glm::vec3(0.0f);
    }

    // getLocalMatrix() composes Rx * Ry * Rz.
    const float x = std::atan2(r[2][1], r[2][2]);
    const float y = std::atan2(-r[2][0], std::sqrt(r[0][0] * r[0][0] + r[1][0] * r[1][0]));
    const float sinX = std::sin(x);
    const float cosX = std::cos(x);
    const float z = std::atan2(sinX * r[0][2] - cosX * r[0][1], cosX * r[1][1] - sinX * r[1][2]);

    eulerDegrees = glm::degrees(glm::vec3(-x, -y, -z));
}
