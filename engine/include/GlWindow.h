#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

// Keeps GLFW initialized for as long as an instance is alive. Hold one for the whole
// process when windows come and go, so that closing the last one does not tear GLFW down.
class GlfwLibrary {
public:
    GlfwLibrary();
    ~GlfwLibrary();

    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;

    bool isReady() const { return ready; }

private:
    bool ready;
};

// A GLFW window with a current OpenGL 3.3 Core context and GLAD loaded.
class GlWindow {
public:
    struct Config {
        int width = 1280;
        int height = 720;
        std::string title;
        bool resizable = true;
        bool visible = true;
        bool maximized = false;
        int swapInterval = 1;
    };

    explicit GlWindow(const Config& config);
    ~GlWindow();

    GlWindow(const GlWindow&) = delete;
    GlWindow& operator=(const GlWindow&) = delete;

    GLFWwindow* handle() const { return window; }
    bool isValid() const { return window != nullptr; }

private:
    GlfwLibrary library;
    GLFWwindow* window = nullptr;
};
