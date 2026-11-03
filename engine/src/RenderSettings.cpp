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
})

NLOHMANN_JSON_SERIALIZE_ENUM(VertexPrecision, {
    {VertexPrecision::Off, "off"},
    {VertexPrecision::High, "high"},
    {VertexPrecision::Medium, "medium"},
    {VertexPrecision::Low, "low"},
})

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
            settings.shadows = false;
            break;
        case RenderMode::PS2:
            settings.internalHeight = 448;
            settings.textureFilter = TextureFilter::Bilinear;
            settings.dithering = true;
            settings.colorBits = 6;
            settings.vertexPrecision = VertexPrecision::High;
            settings.shadows = false;
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
    j["lighting"] = lighting;
    j["shadows"] = shadows;
    j["motionBlur"] = motionBlur;
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
        parsed.lighting = j.value("lighting", parsed.lighting);
        parsed.shadows = j.value("shadows", parsed.shadows);
        parsed.motionBlur = j.value("motionBlur", parsed.motionBlur);
        settings = parsed;
        return true;
    } catch (const json::exception& e) {
        if (error) *error = e.what();
        return false;
    }
}
