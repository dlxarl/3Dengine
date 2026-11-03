#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <unordered_map>

class Input {
public:
    Input();

    void update(GLFWwindow* window);

    bool isKeyDown(int key);
    bool isKeyPressed(int key);
    bool isKeyReleased(int key);

    void handleMouse(double xpos, double ypos);
    void handleScroll(double xoffset, double yoffset);

    float yaw;
    float pitch;
    float fov;

private:
    std::unordered_map<int, bool> currentKeys;
    std::unordered_map<int, bool> previousKeys;

    float lastX;
    float lastY;
    bool firstMouse;
};

extern Input* GInput;