#include "RenderSettings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>

using json = nlohmann::json;

NLOHMANN_JSON_SERIALIZE_ENUM(RenderMode, {
    {RenderMode::Modern, "modern"},
    {RenderMode::PS1, "ps1"},
    {RenderMode::PS2, "ps2"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(TextureFilter, {
    {TextureFilter::Nearest, "nearest"},
    {TextureFilter::Bilinear, "bilinear"},
    {TextureFilter::Trilinear, "trilinear"},
    {TextureFilter::NearestMipmapped, "nearestMipmapped"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(ToneMapper, {
    {ToneMapper::None, "none"},
    {ToneMapper::Reinhard, "reinhard"},
    {ToneMapper::Neutral, "neutral"},
    {ToneMapper::ACES, "aces"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(VertexPrecision, {
    {VertexPrecision::Off, "off"},
    {VertexPrecision::High, "high"},
    {VertexPrecision::Medium, "medium"},
    {VertexPrecision::Low, "low"},
})

namespace {
    json colorJson(const glm::vec3& c) { return json::array({c.r, c.g, c.b}); }

    glm::vec3 colorFrom(const json& j, const char* key, glm::vec3 fallback) {
        if (!j.contains(key) || !j[key].is_array() || j[key].size() != 3) return fallback;
        glm::vec3 c;
        for (int i = 0; i < 3; i++) c[i] = std::clamp(j[key][i].get<float>(), 0.0f, 4.0f);
        return c;
    }

    float number(const json& j, const char* key, float fallback, float low, float high) {
        return std::clamp(j.value(key, fallback), low, high);
    }

    json postJson(const PostSettings& p) {
        json j;
        j["toneMapper"] = p.toneMapper;
        j["colorAdjustments"] = p.colorAdjustments;
        j["exposure"] = p.exposure;
        j["contrast"] = p.contrast;
        j["colorFilter"] = colorJson(p.colorFilter);
        j["whiteBalance"] = p.whiteBalance;
        j["temperature"] = p.temperature;
        j["tint"] = p.tint;
        j["splitToning"] = p.splitToning;
        j["shadowsTint"] = colorJson(p.shadowsTint);
        j["highlightsTint"] = colorJson(p.highlightsTint);
        j["splitBalance"] = p.splitBalance;
        j["bloom"] = p.bloom;
        j["bloomThreshold"] = p.bloomThreshold;
        j["bloomSoftKnee"] = p.bloomSoftKnee;
        j["bloomIntensity"] = p.bloomIntensity;
        j["bloomScatter"] = p.bloomScatter;
        j["depthOfField"] = p.depthOfField;
        j["focalDistance"] = p.focalDistance;
        j["focalRange"] = p.focalRange;
        j["maxBlurRadius"] = p.maxBlurRadius;
        j["vignetteSmoothness"] = p.vignetteSmoothness;
        j["vignetteRoundness"] = p.vignetteRoundness;
        j["motionBlurStrength"] = p.motionBlurStrength;
        j["lensDistortion"] = p.lensDistortion;
        j["distortion"] = p.distortion;
        j["ssao"] = p.ssao;
        j["ssaoRadius"] = p.ssaoRadius;
        j["ssaoBias"] = p.ssaoBias;
        j["ssaoIntensity"] = p.ssaoIntensity;
        j["ssaoSamples"] = p.ssaoSamples;
        j["fxaa"] = p.fxaa;
        return j;
    }

    PostSettings postFrom(const json& j) {
        PostSettings p;
        p.toneMapper = j.value("toneMapper", p.toneMapper);
        p.colorAdjustments = j.value("colorAdjustments", p.colorAdjustments);
        p.exposure = number(j, "exposure", p.exposure, -5.0f, 5.0f);
        p.contrast = number(j, "contrast", p.contrast, -1.0f, 1.0f);
        p.colorFilter = colorFrom(j, "colorFilter", p.colorFilter);
        p.whiteBalance = j.value("whiteBalance", p.whiteBalance);
        p.temperature = number(j, "temperature", p.temperature, -1.0f, 1.0f);
        p.tint = number(j, "tint", p.tint, -1.0f, 1.0f);
        p.splitToning = j.value("splitToning", p.splitToning);
        p.shadowsTint = colorFrom(j, "shadowsTint", p.shadowsTint);
        p.highlightsTint = colorFrom(j, "highlightsTint", p.highlightsTint);
        p.splitBalance = number(j, "splitBalance", p.splitBalance, -1.0f, 1.0f);
        p.bloom = j.value("bloom", p.bloom);
        p.bloomThreshold = number(j, "bloomThreshold", p.bloomThreshold, 0.0f, 10.0f);
        p.bloomSoftKnee = number(j, "bloomSoftKnee", p.bloomSoftKnee, 0.0f, 1.0f);
        p.bloomIntensity = number(j, "bloomIntensity", p.bloomIntensity, 0.0f, 5.0f);
        p.bloomScatter = number(j, "bloomScatter", p.bloomScatter, 0.0f, 1.0f);
        p.depthOfField = j.value("depthOfField", p.depthOfField);
        p.focalDistance = number(j, "focalDistance", p.focalDistance, 0.1f, 1000.0f);
        p.focalRange = number(j, "focalRange", p.focalRange, 0.1f, 1000.0f);
        p.maxBlurRadius = number(j, "maxBlurRadius", p.maxBlurRadius, 0.5f, 16.0f);
        p.vignetteSmoothness = number(j, "vignetteSmoothness", p.vignetteSmoothness, 0.01f, 1.0f);
        p.vignetteRoundness = number(j, "vignetteRoundness", p.vignetteRoundness, 0.0f, 1.0f);
        p.motionBlurStrength = number(j, "motionBlurStrength", p.motionBlurStrength, 0.0f, 4.0f);
        p.lensDistortion = j.value("lensDistortion", p.lensDistortion);
        p.distortion = number(j, "distortion", p.distortion, -1.0f, 1.0f);
        p.ssao = j.value("ssao", p.ssao);
        p.ssaoRadius = number(j, "ssaoRadius", p.ssaoRadius, 0.05f, 5.0f);
        p.ssaoBias = number(j, "ssaoBias", p.ssaoBias, 0.0f, 0.5f);
        p.ssaoIntensity = number(j, "ssaoIntensity", p.ssaoIntensity, 0.0f, 2.0f);
        p.ssaoSamples = j.value("ssaoSamples", p.ssaoSamples) >= 32 ? 32 : 16;
        p.fxaa = j.value("fxaa", p.fxaa);
        return p;
    }
}

RenderSettings RenderSettings::preset(RenderMode mode) {
    RenderSettings settings;
    settings.mode = mode;

    switch (mode) {
        case RenderMode::PS1:
            settings.internalHeight = 240;
            settings.textureFilter = TextureFilter::Nearest;
            settings.dithering = true;
            settings.colorBits = 5;
            settings.vertexPrecision = VertexPrecision::Medium;
            settings.shadows = true;
            settings.vignette = 0.25f;
            settings.scanlines = 0.0f;
            settings.crtCurvature = 0.0f;
            settings.chromaticAberration = 0.06f;
            settings.filmGrain = 0.10f;
            settings.bloom = 0.15f;
            break;
        case RenderMode::PS2:
            settings.internalHeight = 448;
            settings.textureFilter = TextureFilter::Bilinear;
            settings.dithering = true;
            settings.colorBits = 6;
            settings.vertexPrecision = VertexPrecision::High;
            settings.shadows = true;
            settings.vignette = 0.22f;
            settings.scanlines = 0.0f;
            settings.crtCurvature = 0.0f;
            settings.chromaticAberration = 0.04f;
            settings.filmGrain = 0.08f;
            settings.bloom = 0.12f;
            break;
        case RenderMode::Modern:
            break;
    }
    return settings;
}

glm::ivec2 RenderSettings::internalSize(int outputWidth, int outputHeight) const {
    if (internalHeight <= 0 || internalHeight >= outputHeight || outputHeight <= 0) {
        return glm::ivec2(outputWidth, outputHeight);
    }
    const float aspect = (float)outputWidth / (float)outputHeight;
    return glm::ivec2(std::max(1, (int)std::lround(internalHeight * aspect)), internalHeight);
}

glm::vec2 RenderSettings::vertexGrid(float aspectRatio) const {
    float lines = 0.0f;
    switch (vertexPrecision) {
        case VertexPrecision::High:   lines = 480.0f; break;
        case VertexPrecision::Medium: lines = 240.0f; break;
        case VertexPrecision::Low:    lines = 120.0f; break;
        case VertexPrecision::Off:    break;
    }
    return glm::vec2(lines * aspectRatio, lines);
}

std::string RenderSettings::toJson() const {
    json j;
    j["version"] = 1;
    j["renderMode"] = mode;
    j["internalHeight"] = internalHeight;
    j["textureFilter"] = textureFilter;
    j["dithering"] = dithering;
    j["colorBits"] = colorBits;
    j["vertexPrecision"] = vertexPrecision;
    j["vignette"] = vignette;
    j["saturation"] = saturation;
    j["scanlines"] = scanlines;
    j["crtCurvature"] = crtCurvature;
    j["chromaticAberration"] = chromaticAberration;
    j["filmGrain"] = filmGrain;
    j["bloom"] = bloom;
    j["lighting"] = lighting;
    j["shadows"] = shadows;
    j["motionBlur"] = motionBlur;
    j["post"] = postJson(post);
    return j.dump(4);
}

bool RenderSettings::fromJson(const std::string& text, RenderSettings& settings, std::string* error) {
    try {
        const json j = json::parse(text);
        RenderSettings parsed;
        parsed.mode = j.value("renderMode", parsed.mode);
        parsed.internalHeight = std::max(0, j.value("internalHeight", parsed.internalHeight));
        parsed.textureFilter = j.value("textureFilter", parsed.textureFilter);
        parsed.dithering = j.value("dithering", parsed.dithering);
        parsed.colorBits = std::clamp(j.value("colorBits", parsed.colorBits), 1, 8);
        parsed.vertexPrecision = j.value("vertexPrecision", parsed.vertexPrecision);
        parsed.vignette = std::clamp(j.value("vignette", parsed.vignette), 0.0f, 1.0f);
        parsed.saturation = std::clamp(j.value("saturation", parsed.saturation), 0.0f, 2.0f);
        parsed.scanlines = std::clamp(j.value("scanlines", parsed.scanlines), 0.0f, 1.0f);
        parsed.crtCurvature = std::clamp(j.value("crtCurvature", parsed.crtCurvature), 0.0f, 1.0f);
        parsed.chromaticAberration = std::clamp(j.value("chromaticAberration", parsed.chromaticAberration), 0.0f, 1.0f);
        parsed.filmGrain = std::clamp(j.value("filmGrain", parsed.filmGrain), 0.0f, 1.0f);
        parsed.bloom = std::clamp(j.value("bloom", parsed.bloom), 0.0f, 1.0f);
        parsed.lighting = j.value("lighting", parsed.lighting);
        parsed.shadows = j.value("shadows", parsed.shadows);
        parsed.motionBlur = j.value("motionBlur", parsed.motionBlur);
        if (j.contains("post") && j["post"].is_object()) parsed.post = postFrom(j["post"]);
        settings = parsed;
        return true;
    } catch (const json::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}
