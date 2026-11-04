#include "GameClock.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
    struct SkyKey {
        float hour;
        glm::vec3 sky;
        glm::vec3 sun;
        glm::vec3 ambient;
    };

    // Dawn and dusk are short and warm; the night is blue and dim, never black, so the world
    // stays playable.
    const SkyKey SKY[] = {
        {0.0f,  {0.06f, 0.09f, 0.20f}, {0.38f, 0.46f, 0.74f}, {0.20f, 0.24f, 0.38f}},
        {5.0f,  {0.07f, 0.10f, 0.21f}, {0.36f, 0.42f, 0.66f}, {0.19f, 0.22f, 0.35f}},
        {6.5f,  {0.52f, 0.42f, 0.42f}, {0.85f, 0.58f, 0.44f}, {0.28f, 0.26f, 0.30f}},
        {9.0f,  {0.36f, 0.40f, 0.45f}, {0.78f, 0.77f, 0.74f}, {0.26f, 0.29f, 0.34f}},
        {16.5f, {0.37f, 0.40f, 0.45f}, {0.80f, 0.74f, 0.66f}, {0.27f, 0.29f, 0.34f}},
        {19.0f, {0.50f, 0.36f, 0.34f}, {0.85f, 0.52f, 0.36f}, {0.27f, 0.24f, 0.28f}},
        {20.5f, {0.12f, 0.11f, 0.23f}, {0.32f, 0.36f, 0.56f}, {0.18f, 0.20f, 0.32f}},
        {24.0f, {0.06f, 0.09f, 0.20f}, {0.38f, 0.46f, 0.74f}, {0.20f, 0.24f, 0.38f}},
    };

    const float PI = 3.14159265f;
}

void GameClock::begin(const WorldDefinition& world) {
    dayMinutes = world.dayMinutes;
    hour = world.startHour;
    day = 1;
    fogRemembered = false;
}

void GameClock::update(float deltaTime) {
    if (!isRunning()) return;
    hour += deltaTime * 24.0f / (dayMinutes * 60.0f);
    while (hour >= 24.0f) {
        hour -= 24.0f;
        day++;
    }
}

bool GameClock::isNight() const {
    return isRunning() && (hour >= 20.5f || hour < 5.5f);
}

void GameClock::sleepUntil(float targetHour) {
    if (targetHour <= hour) day++;
    hour = targetHour;
}

void GameClock::apply(SceneGraph& scene) {
    if (!isRunning()) return;

    if (!fogRemembered) {
        dayFogStart = scene.fogStart;
        dayFogEnd = scene.fogEnd;
        fogRemembered = true;
    }

    const SkyKey* next = std::find_if(std::begin(SKY), std::end(SKY), [&](const SkyKey& key) { return key.hour > hour; });
    const SkyKey* previous = next - 1;
    const float blend = (hour - previous->hour) / (next->hour - previous->hour);

    const glm::vec3 sky = glm::mix(previous->sky, next->sky, blend);
    scene.clearColor = sky;
    scene.ambientColor = glm::mix(previous->ambient, next->ambient, blend);

    // Fog closes in as the sky darkens.
    const float brightness = std::clamp((sky.r + sky.g + sky.b) / 2.1f, 0.0f, 1.0f);
    const float reach = 0.6f + 0.4f * brightness;
    scene.fogStart = dayFogStart * reach;
    scene.fogEnd = dayFogEnd * reach;

    LightObject* light = scene.getLight();
    if (!light) return;

    // The sun crosses from east to west between six and six; the moon takes the other half.
    const bool day = hour >= 6.0f && hour < 18.0f;
    const float arc = (day ? hour - 6.0f : std::fmod(hour + 6.0f, 24.0f)) / 12.0f * PI;
    const glm::vec3 toLight = glm::normalize(glm::vec3(std::cos(arc), std::max(std::sin(arc), 0.28f), day ? 0.38f : -0.3f));

    light->directional = true;
    light->color = glm::mix(previous->sun, next->sun, blend);
    light->intensity = 1.0f;

    const glm::vec3 position = light->getWorldPosition();
    const glm::vec3 up = std::abs(toLight.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
    light->setWorldMatrix(glm::inverse(glm::lookAt(position, position - toLight, up)));
}
