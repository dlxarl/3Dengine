#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include "Shader.h"

class Skybox {
public:
    Skybox(const std::string& hdrPath);
    ~Skybox();

    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    unsigned int VAO, VBO;
    unsigned int textureID;
    Shader shader;
};