#pragma once
#include "Shader.h"
#include "RayTrace.h"
#include "Underwater.h"
#include <glm/glm.hpp>

struct RenderSettings;
class EffectRenderer;

// One light as the renderer sees it.
struct SceneLight {
    enum Type { Directional = 0, Point = 1, Spot = 2 };
    int type = Point;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);   // the way the light travels
    glm::vec3 color = glm::vec3(1.0f);                      // with its intensity
    float range = 15.0f;
    float spotCosOuter = 0.7f;
    float spotCosInner = 0.9f;
    bool castShadows = false;
    float radius = 0.0f;
    float specular = 1.0f;
};

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

    // Every light that matters this frame: the sun (a directional light) first when there is one,
    // then the nearest to eye. Fills up to most and returns how many there are.
    virtual int getLights(glm::vec3 /*eye*/, SceneLight* /*out*/, int /*most*/) const { return 0; }

    // Lights besides the main one, such as fires and lit windows: position and range, and
    // color, nearest to eye first. Fills up to most and returns how many there are.
    virtual int getPointLights(glm::vec3 /*eye*/, glm::vec4* /*positions*/, glm::vec3* /*colors*/, int /*most*/) const { return 0; }

    // The still geometry the renderer may trace shadow rays against, or null for none. The data
    // is replaced when the geometry changes, so a renderer can tell by the pointer.
    virtual std::shared_ptr<const RayTraceData> getRayTraceData() { return nullptr; }

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