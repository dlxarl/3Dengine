#include "Particles.h"

#include <algorithm>
#include <cmath>

namespace {
    const float MAX_STEP = 0.1f;   // a stall must not release a second's worth of particles at once

    glm::vec4 gradient(const ParticleSettings& s, float t) {
        return t < 0.5f ? glm::mix(s.colorStart, s.colorMid, t * 2.0f) : glm::mix(s.colorMid, s.colorEnd, (t - 0.5f) * 2.0f);
    }
}

float ParticleSystem::range(float low, float high) {
    return low >= high ? low : std::uniform_real_distribution<float>(low, high)(random);
}

void ParticleSystem::clear() {
    particles.clear();
    owed = 0.0f;
}

void ParticleSystem::spawn(const glm::mat4& emitter) {
    const glm::vec3 up = glm::normalize(glm::vec3(emitter[1]) + glm::vec3(1e-6f, 0.0f, 0.0f));
    const glm::vec3 side = glm::normalize(glm::cross(up, std::abs(up.x) < 0.9f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f)));
    const glm::vec3 front = glm::cross(up, side);

    // Evenly over the disc, and over the cone.
    const float discAngle = range(0.0f, 6.2831853f);
    const float discRadius = settings.radius * std::sqrt(range(0.0f, 1.0f));
    const float coneAngle = range(0.0f, 6.2831853f);
    const float tilt = glm::radians(settings.coneDegrees) * std::sqrt(range(0.0f, 1.0f));
    const glm::vec3 direction = up * std::cos(tilt) + (side * std::cos(coneAngle) + front * std::sin(coneAngle)) * std::sin(tilt);

    Particle p;
    p.position = glm::vec3(emitter[3]) + (side * std::cos(discAngle) + front * std::sin(discAngle)) * discRadius;
    p.velocity = direction * range(settings.speedMin, settings.speedMax);
    p.age = 0.0f;
    p.life = std::max(0.05f, range(settings.lifetimeMin, settings.lifetimeMax));
    p.rotation = range(0.0f, 6.2831853f);
    p.spin = range(-settings.spin, settings.spin);
    p.seed = range(0.0f, 1.0f);
    particles.push_back(p);
}

void ParticleSystem::burst(int count, const glm::mat4& emitter) {
    for (int i = 0; i < count && int(particles.size()) < settings.maxParticles; i++) spawn(emitter);
}

void ParticleSystem::update(float deltaTime, const glm::mat4& emitter) {
    deltaTime = std::clamp(deltaTime, 0.0f, MAX_STEP);

    for (size_t i = 0; i < particles.size();) {
        Particle& p = particles[i];
        p.age += deltaTime;
        if (p.age >= p.life) {
            p = particles.back();
            particles.pop_back();
            continue;
        }
        p.velocity += settings.gravity * deltaTime;
        if (settings.turbulence > 0.0f) {
            const float phase = p.age * 2.6f + p.seed * 40.0f;
            p.velocity += glm::vec3(std::sin(phase), 0.3f * std::sin(phase * 0.7f), std::cos(phase * 1.3f)) * settings.turbulence * deltaTime;
        }
        if (settings.drag > 0.0f) p.velocity *= std::exp(-settings.drag * deltaTime);
        p.position += p.velocity * deltaTime;
        p.rotation += p.spin * deltaTime;
        i++;
    }

    if (!playing || settings.rate <= 0.0f) return;
    owed += settings.rate * deltaTime;
    while (owed >= 1.0f) {
        owed -= 1.0f;
        if (int(particles.size()) < settings.maxParticles) spawn(emitter);
    }
}

void ParticleSystem::gather(std::vector<Instance>& out, glm::vec3 eye) const {
    out.clear();
    out.reserve(particles.size());
    for (const Particle& p : particles) {
        const float t = p.age / p.life;
        out.push_back({glm::vec4(p.position, glm::mix(settings.sizeStart, settings.sizeEnd, t)), gradient(settings, t),
                       glm::vec2(p.rotation, p.seed)});
    }
    if (!settings.additive) {
        std::sort(out.begin(), out.end(), [&](const Instance& a, const Instance& b) {
            return glm::dot(glm::vec3(a.positionSize) - eye, glm::vec3(a.positionSize) - eye) >
                   glm::dot(glm::vec3(b.positionSize) - eye, glm::vec3(b.positionSize) - eye);
        });
    }
}
