#include "Input.h"

Input* GInput = nullptr;

Input::Input() {
    yaw = -90.0f;
    pitch = 0.0f;
    fov = 45.0f;

    lastX = 400.0f;
    lastY = 300.0f;
    firstMouse = true;


}

void Input::update(GLFWwindow* window) {
    previousKeys = currentKeys;

    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; key++) {
        currentKeys[key] = (glfwGetKey(window, key) == GLFW_PRESS);
    }
}

bool Input::isKeyDown(int key) {
    return currentKeys[key];
}

bool Input::isKeyPressed(int key) {
    return currentKeys[key] && !previousKeys[key];
}

bool Input::isKeyReleased(int key) {
    return !currentKeys[key] && previousKeys[key];
}

void Input::handleMouse(double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void Input::handleScroll(double xoffset, double yoffset) {
    fov -= (float)yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 45.0f) fov = 45.0f;
}
