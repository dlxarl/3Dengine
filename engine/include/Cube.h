#pragma once
#include "Shape.h"

class Cube : public Shape {
public:
    Cube();
    void draw(Shader& shader) override;
    ObjectType getType() const override { return ObjectType::Cube; }
};