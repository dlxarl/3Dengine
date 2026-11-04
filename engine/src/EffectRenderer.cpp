#include "EffectRenderer.h"
#include "Resources.h"

#include <glad/glad.h>

namespace {
    const int WATER_CELLS = 96;   // across one surface
}

EffectRenderer::EffectRenderer() {
    particleShader = std::make_unique<Shader>(Resources::shader("particle.vert").c_str(), Resources::shader("particle.frag").c_str());
    waterShader = std::make_unique<Shader>(Resources::shader("water.vert").c_str(), Resources::shader("water.frag").c_str());

    glGenVertexArrays(1, &particleVao);
    glGenBuffers(1, &particleVbo);
    glBindVertexArray(particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo);
    const int stride = sizeof(ParticleSystem::Instance);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleSystem::Instance, positionSize));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleSystem::Instance, color));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(ParticleSystem::Instance, rotationSeed));
    for (int i = 0; i < 3; i++) glVertexAttribDivisor(i, 1);
    glBindVertexArray(0);

    buildWaterGrid();
}

EffectRenderer::~EffectRenderer() {
    glDeleteVertexArrays(1, &particleVao);
    glDeleteBuffers(1, &particleVbo);
    glDeleteVertexArrays(1, &waterVao);
    glDeleteBuffers(1, &waterVbo);
    glDeleteBuffers(1, &waterIbo);
}

void EffectRenderer::buildWaterGrid() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const int n = WATER_CELLS;
    for (int z = 0; z <= n; z++) {
        for (int x = 0; x <= n; x++) {
            vertices.push_back(float(x) / n - 0.5f);
            vertices.push_back(float(z) / n - 0.5f);
        }
    }
    for (int z = 0; z < n; z++) {
        for (int x = 0; x < n; x++) {
            const unsigned int a = z * (n + 1) + x, b = a + 1, c = a + n + 1, d = c + 1;
            indices.insert(indices.end(), {a, c, b, b, c, d});
        }
    }
    waterIndexCount = int(indices.size());

    glGenVertexArrays(1, &waterVao);
    glGenBuffers(1, &waterVbo);
    glGenBuffers(1, &waterIbo);
    glBindVertexArray(waterVao);
    glBindBuffer(GL_ARRAY_BUFFER, waterVbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, waterIbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void EffectRenderer::begin(const EffectFrame& newFrame) {
    frame = newFrame;
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
}

void EffectRenderer::end() {
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}

void EffectRenderer::drawWater(const WaterObject& water) {
    const glm::vec3 scale = water.getWorldScale();
    const glm::vec2 size = water.size * glm::vec2(scale.x, scale.z);
    if (size.x <= 0.0f || size.y <= 0.0f) return;

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);

    waterShader->use();
    waterShader->setMat4("view", frame.view);
    waterShader->setMat4("projection", frame.projection);
    waterShader->setVec3("center", water.getWorldPosition());
    waterShader->setVec2("size", size);
    waterShader->setFloat("time", frame.time);
    waterShader->setFloat("waveHeight", water.waveHeight);
    waterShader->setFloat("waveLength", water.waveLength);
    waterShader->setFloat("waveSpeed", water.waveSpeed);
    waterShader->setVec2("flow", water.flow);
    waterShader->setVec3("eye", frame.eye);
    waterShader->setVec3("shallowColor", water.shallowColor);
    waterShader->setVec3("deepColor", water.deepColor);
    waterShader->setFloat("opacity", water.opacity);
    waterShader->setBool("isRound", water.round);
    waterShader->setBool("fromAbove", false);
    waterShader->setVec3("lightDirection", frame.lightDirection);
    waterShader->setVec3("lightColor", frame.lightColor);
    waterShader->setVec3("ambientColor", frame.ambientColor);
    waterShader->setVec3("horizonColor", frame.horizonColor);
    waterShader->setVec2("fogRange", frame.fogRange);
    waterShader->setVec3("fogColor", frame.fogColor);

    glBindVertexArray(waterVao);
    glDrawElements(GL_TRIANGLES, waterIndexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void EffectRenderer::drawWaterMesh(const MeshShape& mesh) {
    const auto& gpu = mesh.getMesh();
    if (!gpu) return;
    if (waterMeshShader == nullptr) {
        waterMeshShader = std::make_unique<Shader>(Resources::shader("water_mesh.vert").c_str(), Resources::shader("water.frag").c_str());
    }

    // The current runs along the mesh, from its first vertex to its last.
    const auto& triangles = gpu->getTriangles();
    glm::vec2 flow(0.0f);
    if (triangles.size() > 3) {
        const glm::mat4 world = mesh.getWorldMatrix();
        const glm::vec3 a = glm::vec3(world * glm::vec4(triangles.front(), 1.0f));
        const glm::vec3 b = glm::vec3(world * glm::vec4(triangles.back(), 1.0f));
        const glm::vec2 along(b.x - a.x, b.z - a.z);
        if (glm::length(along) > 1e-3f) flow = glm::normalize(along) * 1.4f;
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // pieces that meet must not cut into each other

    Shader& shader = *waterMeshShader;
    shader.use();
    shader.setMat4("model", mesh.getWorldMatrix());
    shader.setMat4("view", frame.view);
    shader.setMat4("projection", frame.projection);
    shader.setFloat("time", frame.time);
    shader.setFloat("waveHeight", std::max(mesh.waveHeight, 0.12f) * 1.6f);
    shader.setFloat("waveLength", 4.5f);
    shader.setFloat("waveSpeed", 0.9f);
    shader.setVec2("flow", flow);
    shader.setVec3("eye", frame.eye);
    shader.setVec3("shallowColor", glm::vec3(0.16f, 0.34f, 0.36f));
    shader.setVec3("deepColor", glm::vec3(0.04f, 0.13f, 0.16f));
    shader.setFloat("opacity", 0.78f);
    shader.setBool("isRound", false);
    shader.setBool("fromAbove", true);
    shader.setVec3("lightDirection", frame.lightDirection);
    shader.setVec3("lightColor", frame.lightColor);
    shader.setVec3("ambientColor", frame.ambientColor);
    shader.setVec3("horizonColor", frame.horizonColor);
    shader.setVec2("fogRange", frame.fogRange);
    shader.setVec3("fogColor", frame.fogColor);
    gpu->draw();
    glDepthMask(GL_TRUE);
}

void EffectRenderer::drawParticles(const ParticleSystem& system) {
    system.gather(instances, frame.eye);
    if (instances.empty()) return;

    const ParticleSettings& s = system.settings;
    glDepthMask(GL_FALSE);   // read the scene's depth, add none, or the discs would cut each other out
    if (s.additive) {
        glBlendFunc(GL_ONE, GL_ONE);
    } else {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    particleShader->use();
    particleShader->setMat4("view", frame.view);
    particleShader->setMat4("projection", frame.projection);
    particleShader->setFloat("softness", s.softness);
    particleShader->setFloat("flicker", s.flicker);
    particleShader->setFloat("time", frame.time);
    particleShader->setBool("additive", s.additive);
    particleShader->setVec2("fogRange", frame.fogRange);
    particleShader->setVec3("fogColor", frame.fogColor);

    glBindVertexArray(particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, particleVbo);
    const size_t bytes = instances.size() * sizeof(ParticleSystem::Instance);
    if (instances.size() > particleCapacity) {
        particleCapacity = instances.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, particleCapacity * sizeof(ParticleSystem::Instance), nullptr, GL_STREAM_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances.data());
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GLsizei(instances.size()));
    glBindVertexArray(0);
}
