// aureon_shot: renders one frame of a project's scene without opening the Editor, and writes a PNG.
//
//   aureon_shot --project DIR --out shot.png [--scene Assets/Scenes/X.scene] [--width 1280] [--height 720]
//               [--eye x,y,z --target x,y,z] [--fov 60] [--hour 12] [--frames 30]
//
// Without --eye the scene's Main Camera is used. The scene is simulated for --frames steps first
// (the game's rules, when the scene has a player with game controls), so that things settle.
#include "AssetManager.h"
#include "GameSession.h"
#include "GlWindow.h"
#include "Player.h"
#include "Project.h"
#include "RenderTarget.h"
#include "Renderer.h"
#include "SceneSerializer.h"

#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
    bool parseVec3(const char* text, glm::vec3& out) {
        return std::sscanf(text, "%f,%f,%f", &out.x, &out.y, &out.z) == 3;
    }
}

int main(int argc, char** argv) {
    std::string projectPath, scenePath, outPath = "shot.png";
    int width = 1280, height = 720, frames = 30;
    float fov = 60.0f, hour = -1.0f;
    glm::vec3 eye(0.0f), target(0.0f);
    bool freeCamera = false;

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
        if (arg == "--project") projectPath = next();
        else if (arg == "--scene") scenePath = next();
        else if (arg == "--out") outPath = next();
        else if (arg == "--width") width = std::atoi(next());
        else if (arg == "--height") height = std::atoi(next());
        else if (arg == "--frames") frames = std::atoi(next());
        else if (arg == "--fov") fov = (float)std::atof(next());
        else if (arg == "--hour") hour = (float)std::atof(next());
        else if (arg == "--eye") freeCamera = parseVec3(next(), eye) || freeCamera;
        else if (arg == "--target") parseVec3(next(), target);
        else {
            std::fprintf(stderr, "Unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (projectPath.empty()) {
        std::fprintf(stderr, "Usage: aureon_shot --project DIR [--scene PATH] --out FILE.png [--eye x,y,z --target x,y,z]\n");
        return 2;
    }

    GlfwLibrary glfw;
    GlWindow::Config config;
    config.width = 64;
    config.height = 64;
    config.title = "aureon_shot";
    config.visible = false;
    GlWindow window(config);
    if (!window.isValid()) {
        std::fprintf(stderr, "Cannot create an OpenGL context.\n");
        return 1;
    }

    std::string error;
    auto project = Project::open(fs::u8path(projectPath), &error);
    if (!project) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    AssetManager assets;
    assets.setProject(project->getRoot(), project->libraryDir());

    SceneGraph scene;
    const std::string sceneAsset = scenePath.empty() ? project->startupScene : scenePath;
    if (!SceneSerializer::loadFile(project->getRoot() / fs::u8path(sceneAsset), scene, assets, &error)) {
        std::fprintf(stderr, "Cannot load %s: %s\n", sceneAsset.c_str(), error.c_str());
        return 1;
    }
    if (hour >= 0.0f) scene.game.world.startHour = hour;

    GameSession game;
    game.begin(scene);
    const GameInput idle;
    for (int i = 0; i < frames; i++) {
        game.update(scene, idle, 1.0f / 60.0f);
        scene.update(1.0f / 60.0f);
    }

    CameraView view;
    if (freeCamera) {
        view.position = eye;
        view.view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));
        view.projection = glm::perspective(glm::radians(fov), (float)width / height, 0.1f, 500.0f);
    } else if (CameraObject* camera = scene.getMainCamera()) {
        view = camera->makeView((float)width / height);
    } else {
        std::fprintf(stderr, "The scene has no Main Camera; pass --eye and --target.\n");
        return 1;
    }

    Renderer renderer(4096);
    RenderTarget output;
    output.resize(width, height);
    for (int i = 0; i < 3; i++) {
        renderer.render(scene, view, RenderOutput{output.framebuffer(), width, height}, project->graphics);
    }

    std::vector<unsigned char> pixels((size_t)width * height * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, output.framebuffer());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    stbi_flip_vertically_on_write(1);
    if (!stbi_write_png(outPath.c_str(), width, height, 3, pixels.data(), width * 3)) {
        std::fprintf(stderr, "Cannot write %s\n", outPath.c_str());
        return 1;
    }
    std::printf("%s %dx%d\n", outPath.c_str(), width, height);
    return 0;
}
