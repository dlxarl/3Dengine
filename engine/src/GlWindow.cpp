#include "GlWindow.h"
#include "Log.h"

namespace {
    int libraryUsers = 0;
}

GlfwLibrary::GlfwLibrary() {
    if (libraryUsers == 0) {
        glfwSetErrorCallback([](int code, const char* description) {
            Log::error("GLFW", std::to_string(code) + ": " + description);
        });
        ready = glfwInit() == GLFW_TRUE;
        if (!ready) {
            Log::error("GLFW", "Failed to initialize GLFW");
            return;
        }
    } else {
        ready = true;
    }
    libraryUsers++;
}

GlfwLibrary::~GlfwLibrary() {
    if (!ready) return;
    libraryUsers--;
    if (libraryUsers == 0) {
        glfwTerminate();
    }
}

GlWindow::GlWindow(const Config& config) {
    if (!library.isReady()) return;

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    // macOS only provides forward-compatible core contexts; window creation fails without it.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);

    window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (!window) {
        Log::error("GLFW", "Failed to create window");
        return;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(config.swapInterval);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Log::error("GLFW", "Failed to initialize GLAD");
        glfwDestroyWindow(window);
        window = nullptr;
    }
}

GlWindow::~GlWindow() {
    if (window) {
        glfwDestroyWindow(window);
    }
}
