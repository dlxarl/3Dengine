#pragma once
#include <glm/glm.hpp>
#include <string>

// Modern shades per pixel. PS1 and PS2 light per vertex; PS1 also maps textures affinely.
enum class RenderMode { Modern, PS1, PS2 };
enum class TextureFilter { Nearest, Bilinear, Trilinear };
enum class VertexPrecision { Off, High, Medium, Low };

struct RenderSettings {
    RenderMode mode = RenderMode::Modern;
    int internalHeight = 0;  // lines rendered before upscaling; 0 renders at output resolution
    TextureFilter textureFilter = TextureFilter::Trilinear;
    bool dithering = false;
    int colorBits = 8;       // bits per channel; 8 leaves the image unquantized
    VertexPrecision vertexPrecision = VertexPrecision::Off;

    bool lighting = true;
    bool shadows = true;
    bool motionBlur = false;

    // The look each mode stands for. Selecting a mode applies this; every field stays editable.
    static RenderSettings preset(RenderMode mode);

    glm::ivec2 internalSize(int outputWidth, int outputHeight) const;

    // Screen-space snapping grid, in cells; zero when vertex snapping is off.
    glm::vec2 vertexGrid(float aspectRatio) const;

    std::string toJson() const;
    static bool fromJson(const std::string& text, RenderSettings& settings, std::string* error = nullptr);
};
