#include "Engine.h"
#include "scenes/include/DemoPhysics.h"
#include "scenes/include/DemoScene.h"
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char** argv) {
    Engine engine;

    if (engine.init(1920, 1080, "3D Engine") != 0) {
        std::cout << "Failed to initialize engine" << std::endl;
        return -1;
    }

    // DemoPhysics runs by default; pass --basic for the rendering-only DemoScene.
    std::shared_ptr<Scene> scene;
    if (argc > 1 && std::string(argv[1]) == "--basic") {
        scene = std::make_shared<DemoScene>();
    } else {
        scene = std::make_shared<DemoPhysics>();
    }

    engine.setScene(scene);
    engine.run();

    return 0;
}
