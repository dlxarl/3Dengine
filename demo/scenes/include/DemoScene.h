#pragma once
#include "Scene.h"
#include "Shape.h"
#include "Skybox.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

class DemoScene : public Scene {
public:
    void load() override;
    void update(float deltaTime) override;
    void draw(Shader& lightingShader, Shader& lampShader, const glm::mat4& view, const glm::mat4& proj) override;
    void drawShadow(Shader& shadowShader) override;
    void drawDepth(Shader& depthShader) override;
    glm::vec3 getLightPos() const override { return lightPos; }
private:
    std::vector<std::shared_ptr<Shape>> shapes;
    std::unique_ptr<Shape> lightCube;
    glm::vec3 lightPos;
    std::unique_ptr<Skybox> skybox;
};