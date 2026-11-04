#pragma once
#include "SceneObject.h"

#include <glm/glm.hpp>

enum class LightType {
    Point,        // shines every way from its position: a lantern, a neon sign, a candle
    Spot,         // a cone from its position along its forward (-Z) axis: a torch, a street lamp
    Directional,  // the same direction everywhere, along its forward axis: the sun or the moon
};

// A source of light. A scene may have as many as it likes; the renderer uses the sun (the first
// directional light) and the nearest others, up to a limit.
class LightObject : public SceneObject {
public:
    ObjectType getType() const override { return ObjectType::Light; }

    LightType type = LightType::Point;
    bool isDirectional() const { return type == LightType::Directional; }

    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float temperature = 0.0f;       // Kelvin, tinting the color: 2000 is a candle, 6500 is daylight. 0 uses the color as it is.

    float range = 15.0f;            // metres at which a point or spot light has faded out
    float spotAngle = 45.0f;        // degrees from the axis to the edge of the cone
    float spotSoftness = 0.3f;      // 0 a hard edge, 1 fades from the axis

    bool castShadows = true;
    float radius = 0.0f;            // the size of the source in metres: larger gives softer, blurrier shadows
    float specular = 1.0f;          // how bright its highlights are, relative to its light

    float flicker = 0.0f;           // 0 steady; up to 1 for a neon sign that stutters or a candle that wavers
    float flickerSpeed = 8.0f;

    glm::vec3 getDirection() const { return -glm::normalize(glm::vec3(model[2])); }

    // The color the light actually has: color, temperature, intensity and flicker, at a time.
    glm::vec3 effectiveColor(float time) const;
};

// The color of a black body at a temperature, normalized so that its brightest channel is 1.
glm::vec3 kelvinToColor(float kelvin);
