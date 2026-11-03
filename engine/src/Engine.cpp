#include "Engine.h"

#include "Log.h"

#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <glm/gtc/matrix_transform.hpp>

// Глобальна камера
glm::vec3 cameraPos   = glm::vec3(0.0f, 2.0f,  6.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);

// FPS counter
static double g_fpsLastTime = 0.0;
static int    g_fpsFrames   = 0;

// Manual FPS limit (0 = unlimited)
static int targetFPS = 0;

Engine::Engine() {
    window = nullptr;
    width = 1920;
    height = 1080;
    input = std::make_unique<Input>();
    GInput = input.get();

    deltaTime = 0.0f;
    lastFrame = 0.0f;
}

Engine::~Engine() = default;

int Engine::init(int width, int height, const char* title) {
    this->width  = width;
    this->height = height;

    GlWindow::Config config;
    config.width = width;
    config.height = height;
    config.title = title;
    // VSync повністю вимкнено
    config.swapInterval = 0;

    glWindow = std::make_unique<GlWindow>(config);
    if (!glWindow->isValid()) {
        return -1;
    }
    window = glWindow->handle();

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);

    renderer = std::make_unique<Renderer>();

    lastFrame      = glfwGetTime();
    g_fpsLastTime  = glfwGetTime();
    g_fpsFrames    = 0;

    return 0;
}

void Engine::setScene(std::shared_ptr<Scene> scene) {
    currentScene = scene;
}

void Engine::run() {
    if (currentScene) {
        currentScene->load();
    }

    while (!glfwWindowShouldClose(window)) {

        double currentFrame = glfwGetTime();
        double rawDelta     = currentFrame - lastFrame;
        lastFrame           = currentFrame;

        deltaTime = std::clamp(rawDelta, 0.0, 0.05);

        // FPS counter
        g_fpsFrames++;
        if (currentFrame - g_fpsLastTime >= 1.0) {
            std::cout << "FPS: " << g_fpsFrames << std::endl;
            g_fpsFrames = 0;
            g_fpsLastTime = currentFrame;
        }

        input->update(window);
        processInput();
        update();
        render();

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Manual FPS limit
        if (targetFPS > 0) {
            double frameTime = 1.0 / targetFPS;
            double now = glfwGetTime();
            double sleepTime = frameTime - (now - currentFrame);

            if (sleepTime > 0) {
                std::this_thread::sleep_for(
                    std::chrono::duration<double>(sleepTime)
                );
            }
        }
    }
}

void Engine::processInput() {
    if (input->isKeyPressed(GLFW_KEY_ESCAPE))
        glfwSetWindowShouldClose(window, true);

    if (input->isKeyPressed(GLFW_KEY_1)) {
        targetFPS = 30;
        std::cout << "FPS limit: 30" << std::endl;
    }

    if (input->isKeyPressed(GLFW_KEY_2)) {
        targetFPS = 70;
        std::cout << "FPS limit: 70" << std::endl;
    }

    if (input->isKeyPressed(GLFW_KEY_0)) {
        targetFPS = 0;
        std::cout << "FPS limit: OFF" << std::endl;
    }
}


void Engine::update() {
    glm::vec3 front;
    front.x = cos(glm::radians(input->yaw)) * cos(glm::radians(input->pitch));
    front.y = sin(glm::radians(input->pitch));
    front.z = sin(glm::radians(input->yaw)) * cos(glm::radians(input->pitch));
    cameraFront = glm::normalize(front);

    if (currentScene)
        currentScene->update(deltaTime);
}

void Engine::render() {
    int displayW, displayH;
    glfwGetFramebufferSize(window, &displayW, &displayH);
    float aspectRatio = (displayH == 0) ? 1.0f : (float)displayW / displayH;

    if (!currentScene) {
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    CameraView camera;
    camera.projection = glm::perspective(glm::radians(input->fov), aspectRatio, 0.1f, 100.0f);
    camera.view       = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    camera.position   = cameraPos;

    currentScene->configureRender(renderSettings);
    renderer->render(*currentScene, camera, RenderOutput{0, displayW, displayH}, renderSettings);
}

void Engine::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void Engine::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) engine->input->handleMouse(xpos, ypos);
}

void Engine::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine) engine->input->handleScroll(xoffset, yoffset);
}