#pragma once
#include "Shape.h"
#include <vector>

class Sphere : public Shape {
public:
    Sphere(float radius = 1.0f, int sectorCount = 36, int stackCount = 18);
    ~Sphere() override;
    void draw(Shader& shader) override;
    ObjectType getType() const override { return ObjectType::Sphere; }

private:
    unsigned int EBO;
    int indexCount;
};