#pragma once
#include <glm/glm.hpp>
#include <string>

// Modern shades per pixel. PS1 and PS2 light per vertex; PS1 also maps textures affinely.
enum class RenderMode { Modern, PS1, PS2 };
enum class TextureFilter { Nearest, Bilinear, Trilinear, NearestMipmapped };
enum class VertexPrecision { Off, High, Medium, Low };
enum class ToneMapper { None, Reinhard, Neutral, ACES };

// The cinematic post-processing stack. Every effect is off by default, and an effect that is
// off costs nothing: its passes and buffers are never touched.
struct PostSettings {
    // Tone mapping and color grading, in the scene's linear-ish HDR range.
    ToneMapper toneMapper = ToneMapper::None;
    bool colorAdjustments = false;
    float exposure = 0.0f;                 // in stops: color * 2^exposure
    float contrast = 0.0f;                 // -1 to 1, around mid grey
    glm::vec3 colorFilter{1.0f};           // multiplies the image
    bool whiteBalance = false;
    float temperature = 0.0f;              // -1 cool to 1 warm
    float tint = 0.0f;                     // -1 green to 1 magenta
    bool splitToning = false;
    glm::vec3 shadowsTint{0.5f};           // 0.5 grey leaves the shadows alone
    glm::vec3 highlightsTint{0.5f};
    float splitBalance = 0.0f;             // -1 more shadows to 1 more highlights

    // Camera and optics.
    bool bloom = false;
    float bloomThreshold = 1.0f;
    float bloomSoftKnee = 0.5f;            // 0 to 1
    float bloomIntensity = 0.5f;
    float bloomScatter = 0.7f;             // 0 to 1, how far the glow spreads
    bool depthOfField = false;
    float focalDistance = 10.0f;           // metres to the sharpest point
    float focalRange = 8.0f;               // metres that stay sharp around it
    float maxBlurRadius = 6.0f;            // in pixels of the internal resolution
    float vignetteSmoothness = 0.5f;       // 0.01 to 1; 0.5 is the classic falloff
    float vignetteRoundness = 0.0f;        // 0 follows the screen's shape, 1 is a circle
    float motionBlurStrength = 1.0f;       // scales the blur Motion Blur turns on

    // Lens.
    bool lensDistortion = false;
    float distortion = 0.0f;               // -1 barrel to 1 pincushion

    // Screen space.
    bool ssao = false;
    float ssaoRadius = 0.6f;               // metres
    float ssaoBias = 0.03f;
    float ssaoIntensity = 1.0f;            // 0 to 2
    int ssaoSamples = 16;                  // 16 or 32

    // Anti-aliasing.
    bool fxaa = false;

    bool needsHdr() const { return toneMapper != ToneMapper::None || bloom; }
    bool needsGrade() const { return needsHdr() || colorAdjustments || whiteBalance || splitToning; }
};

struct RenderSettings {
    RenderMode mode = RenderMode::Modern;
    int internalHeight = 0;  // lines rendered before upscaling; 0 renders at output resolution
    TextureFilter textureFilter = TextureFilter::Trilinear;
    bool dithering = false;
    int colorBits = 8;       // bits per channel; 8 leaves the image unquantized
    VertexPrecision vertexPrecision = VertexPrecision::Off;
    float vignette = 0.0f;     // darkening towards the corners, 0 to 1
    float saturation = 1.0f;   // 0 is grey, 1 leaves colors as they are, above 1 strengthens them
    float scanlines = 0.0f;    // CRT scanlines, 0 to 1
    float crtCurvature = 0.0f; // CRT monitor curvature, 0 to 1
    float chromaticAberration = 0.0f; // Color fringing at screen edges, 0 to 1
    float filmGrain = 0.0f;    // Analog noise, 0 to 1
    float bloom = 0.0f;        // CRT phosphor glow, 0 to 1

    bool lighting = true;
    bool shadows = true;

    // Shadows from rays traced in the fragment shader against the scene's still geometry, with
    // soft edges from lights that have a size. Costs more the more lights have shadows on.
    bool rayTracedShadows = false;
    bool rayTracedSun = false;      // the sun as well, which adds shadows beyond the shadow map's reach
    int raySamples = 4;             // rays per pixel per light with a size, 1 to 8
    bool motionBlur = false;

    PostSettings post;

    // The look each mode stands for. Selecting a mode applies this; every field stays editable.
    static RenderSettings preset(RenderMode mode);

    glm::ivec2 internalSize(int outputWidth, int outputHeight) const;

    // Screen-space snapping grid, in cells; zero when vertex snapping is off.
    glm::vec2 vertexGrid(float aspectRatio) const;

    std::string toJson() const;
    static bool fromJson(const std::string& text, RenderSettings& settings, std::string* error = nullptr);
};
