#pragma once
#include "Shape.h"

class Plane : public Shape {
public:
    Plane();
    void draw(Shader& shader) override;
    ObjectType getType() const override { return ObjectType::Plane; }
};