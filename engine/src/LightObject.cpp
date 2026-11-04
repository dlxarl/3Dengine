#include "LightObject.h"

#include <algorithm>
#include <cmath>

glm::vec3 kelvinToColor(float kelvin) {
    // Tanner Helland's fit of a black body's color, good between 1000 and 40000 Kelvin.
    const float t = std::clamp(kelvin, 1000.0f, 40000.0f) / 100.0f;
    float r, g, b;
    r = t <= 66.0f ? 255.0f : 329.698727446f * std::pow(t - 60.0f, -0.1332047592f);
    g = t <= 66.0f ? 99.4708025861f * std::log(t) - 161.1195681661f : 288.1221695283f * std::pow(t - 60.0f, -0.0755148492f);
    b = t >= 66.0f ? 255.0f : (t <= 19.0f ? 0.0f : 138.5177312231f * std::log(t - 10.0f) - 305.0447927307f);
    return glm::clamp(glm::vec3(r, g, b) / 255.0f, glm::vec3(0.0f), glm::vec3(1.0f));
}

glm::vec3 LightObject::effectiveColor(float time) const {
    glm::vec3 result = color * intensity;
    if (temperature > 0.0f) result *= kelvinToColor(temperature);
    if (flicker > 0.0f) {
        // Two incommensurate waves, so that it never settles into a pattern.
        const float wave = 0.5f + 0.5f * std::sin(time * flickerSpeed + std::sin(time * flickerSpeed * 0.37f) * 2.0f) *
                                      std::sin(time * flickerSpeed * 0.71f + 1.3f);
        result *= 1.0f - flicker * (1.0f - wave);
    }
    return result;
}
