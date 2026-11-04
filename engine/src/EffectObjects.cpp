#include "EffectObjects.h"

#include <cmath>

FireObject::FireObject() {
    configure();
}

void FireObject::configure() {
    if (size == lastSize && intensity == lastIntensity && smoke == lastSmoke && sparks == lastSparks) return;
    lastSize = size;
    lastIntensity = intensity;
    lastSmoke = smoke;
    lastSparks = sparks;

    const float s = std::max(0.05f, size);
    const float amount = std::max(0.0f, intensity);

    ParticleSettings& fire = flames.settings;
    fire.maxParticles = 160;
    fire.rate = 38.0f * amount * (0.5f + 0.5f * s);
    fire.lifetimeMin = 0.55f;
    fire.lifetimeMax = 1.0f;
    fire.speedMin = 0.7f * s;
    fire.speedMax = 1.4f * s;
    fire.coneDegrees = 8.0f;
    fire.radius = 0.12f * s;
    fire.gravity = glm::vec3(0.0f, 1.4f * s, 0.0f);
    fire.drag = 0.8f;
    fire.turbulence = 2.2f * s;
    fire.sizeStart = 0.42f * s;
    fire.sizeEnd = 0.04f * s;
    fire.spin = 1.5f;
    fire.colorStart = glm::vec4(1.0f, 0.7f, 0.25f, 0.5f);
    fire.colorMid = glm::vec4(1.0f, 0.3f, 0.05f, 0.4f);
    fire.colorEnd = glm::vec4(0.3f, 0.03f, 0.0f, 0.0f);
    fire.additive = true;
    fire.softness = 0.9f;
    fire.flicker = 0.7f;

    ParticleSettings& fumes = smokeCloud.settings;
    fumes.maxParticles = 120;
    fumes.rate = 7.0f * smoke * amount;
    fumes.lifetimeMin = 2.0f;
    fumes.lifetimeMax = 3.6f;
    fumes.speedMin = 0.4f * s;
    fumes.speedMax = 0.9f * s;
    fumes.coneDegrees = 22.0f;
    fumes.radius = 0.18f * s;
    fumes.gravity = glm::vec3(0.0f, 0.7f * s, 0.0f);
    fumes.drag = 0.4f;
    fumes.turbulence = 1.0f * s;
    fumes.sizeStart = 0.5f * s;
    fumes.sizeEnd = 1.9f * s;
    fumes.spin = 0.6f;
    fumes.colorStart = glm::vec4(0.12f, 0.11f, 0.10f, 0.0f);
    fumes.colorMid = glm::vec4(0.16f, 0.15f, 0.14f, 0.32f);
    fumes.colorEnd = glm::vec4(0.22f, 0.22f, 0.22f, 0.0f);
    fumes.additive = false;
    fumes.softness = 1.0f;

    ParticleSettings& glow = sparkShower.settings;
    glow.maxParticles = 80;
    glow.rate = 9.0f * sparks * amount;
    glow.lifetimeMin = 0.8f;
    glow.lifetimeMax = 1.7f;
    glow.speedMin = 1.2f * s;
    glow.speedMax = 3.2f * s;
    glow.coneDegrees = 30.0f;
    glow.radius = 0.15f * s;
    glow.gravity = glm::vec3(0.0f, -1.2f, 0.0f);
    glow.drag = 0.3f;
    glow.turbulence = 2.0f;
    glow.sizeStart = 0.045f * s;
    glow.sizeEnd = 0.015f * s;
    glow.colorStart = glm::vec4(1.0f, 0.9f, 0.5f, 1.0f);
    glow.colorMid = glm::vec4(1.0f, 0.5f, 0.1f, 0.8f);
    glow.colorEnd = glm::vec4(0.8f, 0.15f, 0.0f, 0.0f);
    glow.additive = true;
    glow.softness = 0.3f;
}

void FireObject::update(float deltaTime) {
    configure();
    flames.playing = smokeCloud.playing = sparkShower.playing = burning;

    const float s = std::max(0.05f, size);
    flames.update(deltaTime, model);
    sparkShower.update(deltaTime, glm::translate(model, glm::vec3(0.0f, 0.3f * s, 0.0f)));
    smokeCloud.update(deltaTime, glm::translate(model, glm::vec3(0.0f, 0.8f * s, 0.0f)));
}

float FireObject::lightLevel(float time) const {
    if (!burning) return 0.0f;
    const float phase = time * 9.0f + float(id) * 1.7f;
    const float wobble = 0.5f * std::sin(phase) + 0.3f * std::sin(phase * 2.3f + 1.0f) + 0.2f * std::sin(phase * 5.1f + 2.0f);
    return std::max(0.2f, 1.0f + wobble * flicker * 0.5f);
}

bool WaterObject::contains(glm::vec2 point) const {
    const glm::vec3 center = getWorldPosition();
    const glm::vec3 scaled = getWorldScale();
    const glm::vec2 half = 0.5f * size * glm::vec2(scaled.x, scaled.z);
    const glm::vec2 offset = point - glm::vec2(center.x, center.z);
    if (round) return half.x > 0.0f && half.y > 0.0f && glm::dot(offset / half, offset / half) < 1.0f;
    return std::abs(offset.x) < half.x && std::abs(offset.y) < half.y;
}
