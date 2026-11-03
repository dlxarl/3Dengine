#include "Cylinder.h"
#include <vector>
#include <cmath>

Cylinder::Cylinder(float radius, float height, int segments) {
    build(radius, height, segments);
}

void Cylinder::build(float radius, float height, int segments) {
    std::vector<float> vertices;

    float halfH = height * 0.5f;

    // БІЧНА СТІНКА
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / segments * 2.0f * M_PI;
        float a1 = (float)(i + 1) / segments * 2.0f * M_PI;

        float x0 = cos(a0) * radius;
        float z0 = sin(a0) * radius;
        float x1 = cos(a1) * radius;
        float z1 = sin(a1) * radius;

        float nx0 = cos(a0);
        float nz0 = sin(a0);
        float nx1 = cos(a1);
        float nz1 = sin(a1);

        // Трикутник 1
        vertices.insert(vertices.end(), {
            x0, -halfH, z0,  nx0, 0, nz0,  (float)i/segments, 0.0f,
            x0,  halfH, z0,  nx0, 0, nz0,  (float)i/segments, 1.0f,
            x1,  halfH, z1,  nx1, 0, nz1,  (float)(i+1)/segments, 1.0f
        });

        // Трикутник 2
        vertices.insert(vertices.end(), {
            x0, -halfH, z0,  nx0, 0, nz0,  (float)i/segments, 0.0f,
            x1,  halfH, z1,  nx1, 0, nz1,  (float)(i+1)/segments, 1.0f,
            x1, -halfH, z1,  nx1, 0, nz1,  (float)(i+1)/segments, 0.0f
        });
    }

    // ВЕРХНЯ КРИШКА
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / segments * 2.0f * M_PI;
        float a1 = (float)(i + 1) / segments * 2.0f * M_PI;

        float x0 = cos(a0) * radius;
        float z0 = sin(a0) * radius;
        float x1 = cos(a1) * radius;
        float z1 = sin(a1) * radius;

        vertices.insert(vertices.end(), {
            0,  halfH, 0,   0,1,0,   0.5f, 0.5f,
            x1, halfH, z1,  0,1,0,   (x1/radius+1)*0.5f, (z1/radius+1)*0.5f,
            x0, halfH, z0,  0,1,0,   (x0/radius+1)*0.5f, (z0/radius+1)*0.5f
        });
    }

    //  НИЖНЯ КРИШКА
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / segments * 2.0f * M_PI;
        float a1 = (float)(i + 1) / segments * 2.0f * M_PI;

        float x0 = cos(a0) * radius;
        float z0 = sin(a0) * radius;
        float x1 = cos(a1) * radius;
        float z1 = sin(a1) * radius;

        vertices.insert(vertices.end(), {
            0, -halfH, 0,   0,-1,0,   0.5f, 0.5f,
            x0, -halfH, z0, 0,-1,0,   (x0/radius+1)*0.5f, (z0/radius+1)*0.5f,
            x1, -halfH, z1, 0,-1,0,   (x1/radius+1)*0.5f, (z1/radius+1)*0.5f
        });
    }

    vertexCount = vertices.size() / 8;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // layout: pos(3), normal(3), uv(2)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Cylinder::draw(Shader& shader) {
    bindMaterial(shader);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}
