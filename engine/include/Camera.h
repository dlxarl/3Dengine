#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// What the renderer needs to know about a viewpoint.
struct CameraView {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
};
