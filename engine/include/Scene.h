#pragma once
#include "Shader.h"
#include "Underwater.h"
#include <glm/glm.hpp>

struct RenderSettings;
class EffectRenderer;

class Scene {
public:
    virtual ~Scene() = default;
    virtual void load() = 0;
    virtual void update(float deltaTime) = 0;
    virtual void draw(Shader&, Shader&, const glm::mat4& view, const glm::mat4& proj) = 0;


    virtual void drawShadow(Shader& shadowShader) = 0;
    virtual void drawDepth(Shader& depthShader) = 0;
    virtual glm::vec3 getLightPos() const = 0;
    virtual glm::vec3 getLightColor() const { return glm::vec3(1.0f); }
    virtual glm::vec3 getClearColor() const { return glm::vec3(0.1f); }

    // Distances where fog begins and is complete, towards the clear color. An end of 0 is no fog.
    virtual glm::vec2 getFogRange() const { return glm::vec2(0.0f); }

    // Fills direction and returns true when the light is directional; see LightObject.
    virtual bool getLightDirection(glm::vec3&) const { return false; }

    // The light that reaches everywhere. Only used with a directional light.
    virtual glm::vec3 getAmbientColor() const { return glm::vec3(0.35f); }

    // Seconds of play, for anything that moves by itself, such as water.
    virtual float getAnimationTime() const { return 0.0f; }

    // The shadow pass. lightSpace lets a scene skip what cannot cast a shadow into view.
    virtual void drawShadowCasters(Shader& depthShader, const glm::mat4& /*lightSpace*/) { drawDepth(depthShader); }

    // Lights besides the main one, such as fires and lit windows: position and range, and
    // color, nearest to eye first. Fills up to most and returns how many there are.
    virtual int getPointLights(glm::vec3 /*eye*/, glm::vec4* /*positions*/, glm::vec3* /*colors*/, int /*most*/) const { return 0; }

    // Whether a sky is painted behind everything: the clear color at the horizon, deepening
    // overhead, with clouds, the sun or moon where the light comes from, and stars at night.
    virtual bool hasGradientSky() const { return false; }

    // Draws what is not geometry: water and particles. Called after the scene, against its depth.
    virtual void drawEffects(EffectRenderer&) {}

    // Whether a camera at eye is under water, and how that looks.
    virtual UnderwaterView getUnderwater(glm::vec3 /*eye*/) const { return UnderwaterView(); }

    // Lets a scene adjust the settings it is rendered with, before each frame.
    virtual void configureRender(RenderSettings&) {}
};