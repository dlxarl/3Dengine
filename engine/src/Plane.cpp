#include "Plane.h"

Plane::Plane() {
    float vertices[] = {
         0.5f, 0.0f,  0.5f,    0.0f, 1.0f, 0.0f,   10.0f, 0.0f,
        -0.5f, 0.0f,  0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 0.0f,
        -0.5f, 0.0f, -0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 10.0f,

         0.5f, 0.0f,  0.5f,    0.0f, 1.0f, 0.0f,   10.0f, 0.0f,
        -0.5f, 0.0f, -0.5f,    0.0f, 1.0f, 0.0f,    0.0f, 10.0f,
         0.5f, 0.0f, -0.5f,    0.0f, 1.0f, 0.0f,   10.0f, 10.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Plane::draw(Shader& shader) {
    bindMaterial(shader);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}