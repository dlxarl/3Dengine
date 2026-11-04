#pragma once
#include "EffectObjects.h"
#include "MeshShape.h"
#include "Shader.h"

#include <memory>
#include <vector>

// What every effect in a frame is drawn against.
struct EffectFrame {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
    glm::vec3 eye = glm::vec3(0.0f);
    glm::vec2 fogRange = glm::vec2(0.0f);
    glm::vec3 fogColor = glm::vec3(0.0f);
    glm::vec3 lightDirection = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 lightColor = glm::vec3(1.0f);
    glm::vec3 ambientColor = glm::vec3(0.3f);
    glm::vec3 horizonColor = glm::vec3(0.5f, 0.6f, 0.7f);
    float time = 0.0f;
};

// Draws water surfaces and particles on top of the scene, testing against its depth. Water
// goes first, then the particles, so that a fire behind a lake is hidden by it and a fire
// beside one is not.
class EffectRenderer {
public:
    EffectRenderer();
    ~EffectRenderer();

    EffectRenderer(const EffectRenderer&) = delete;
    EffectRenderer& operator=(const EffectRenderer&) = delete;

    void begin(const EffectFrame& frame);
    void drawWater(const WaterObject& water);
    // A mesh of the Water role, such as a river, given the look and the waves of a Water object.
    void drawWaterMesh(const MeshShape& mesh);
    void drawParticles(const ParticleSystem& system);
    void end();

private:
    std::unique_ptr<Shader> particleShader;
    std::unique_ptr<Shader> waterShader;
    std::unique_ptr<Shader> waterMeshShader;   // made when the first water mesh is drawn
    EffectFrame frame;
    std::vector<ParticleSystem::Instance> instances;

    unsigned int particleVao = 0, particleVbo = 0;
    size_t particleCapacity = 0;
    unsigned int waterVao = 0, waterVbo = 0, waterIbo = 0;
    int waterIndexCount = 0;

    void buildWaterGrid();
};
