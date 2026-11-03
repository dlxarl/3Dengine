#pragma once
#include "Shader.h"
#include <glm/glm.hpp>

struct RenderSettings;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void load() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw(Shader&, Shader&, const glm::mat4& view, const glm::mat4& proj) = 0;


    virtual void drawShadow(Shader& shadowShader) = 0;
    virtual void drawDepth(Shader& depthShader) = 0;
    virtual glm::vec3 getLightPos() const = 0;
    virtual glm::vec3 getLightColor() const { return glm::vec3(1.0f); }
    virtual glm::vec3 getClearColor() const { return glm::vec3(0.1f); }

    // Lets a scene adjust the settings it is rendered with, before each frame.
    virtual void configureRender(RenderSettings&) {}
};