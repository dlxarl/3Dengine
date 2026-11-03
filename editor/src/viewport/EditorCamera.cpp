#include "EditorCamera.h"

#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace {
    const float LOOK_SENSITIVITY = 0.2f;
    const float NEAR_PLANE = 0.05f;
    const float FAR_PLANE = 1000.0f;
    const float ORTHO_DEPTH = 1000.0f;
}

glm::vec3 EditorCamera::forward() const {
    const float yawRad = glm::radians(yaw);
    const float pitchRad = glm::radians(pitch);
    return glm::vec3(std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad), std::sin(yawRad) * std::cos(pitchRad));
}

glm::vec3 EditorCamera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 EditorCamera::up() const {
    return glm::cross(right(), forward());
}

glm::vec3 EditorCamera::getPosition() const {
    return pivot - forward() * distance;
}

CameraView EditorCamera::makeView(float aspectRatio) const {
    CameraView camera;
    camera.position = getPosition();
    camera.view = glm::lookAt(camera.position, pivot, glm::vec3(0.0f, 1.0f, 0.0f));

    if (orthographic) {
        const float halfWidth = orthographicSize * aspectRatio;
        camera.projection = glm::ortho(-halfWidth, halfWidth, -orthographicSize, orthographicSize,
                                       -ORTHO_DEPTH, ORTHO_DEPTH);
    } else {
        camera.projection = glm::perspective(glm::radians(fieldOfView), aspectRatio, NEAR_PLANE, FAR_PLANE);
    }
    return camera;
}

void EditorCamera::focus(glm::vec3 center, float radius) {
    radius = std::max(radius, 0.25f);
    pivot = center;
    distance = radius * 2.0f / std::tan(glm::radians(fieldOfView) * 0.5f);
    orthographicSize = radius * 1.6f;
}

void EditorCamera::pan(float deltaX, float deltaY, float viewportHeight) {
    const float visibleHeight = orthographic ? orthographicSize * 2.0f
                                             : 2.0f * distance * std::tan(glm::radians(fieldOfView) * 0.5f);
    const float unitsPerPixel = visibleHeight / std::max(viewportHeight, 1.0f);
    pivot += (right() * -deltaX + up() * deltaY) * unitsPerPixel;
}

void EditorCamera::zoom(float wheel) {
    // A trackpad sends values around 5 on every frame of a flick, where a mouse notch sends 1.
    // Limiting the step keeps one gesture from diving straight through the scene.
    const float factor = std::pow(0.94f, std::clamp(wheel, -1.5f, 1.5f));
    if (orthographic) {
        orthographicSize = std::clamp(orthographicSize * factor, 0.05f, 500.0f);
    } else {
        distance = std::clamp(distance * factor, 0.25f, 500.0f);
    }
}

bool EditorCamera::update(float deltaTime, bool viewportHovered, float viewportHeight) {
    const ImGuiIO& io = ImGui::GetIO();

    if (drag == Drag::None && viewportHovered) {
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            drag = Drag::Look;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            drag = Drag::Pan;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.KeyAlt) {
            drag = io.KeyShift ? Drag::Pan : Drag::Orbit;
        }
    }

    const bool buttonHeld = ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
                            ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    if (!buttonHeld) {
        drag = Drag::None;
    }

    switch (drag) {
        case Drag::Look: {
            // Turns on the spot: the pivot moves with the view, the eye stays where it is.
            const glm::vec3 eye = getPosition();
            yaw += io.MouseDelta.x * LOOK_SENSITIVITY;
            pitch = std::clamp(pitch - io.MouseDelta.y * LOOK_SENSITIVITY, -89.0f, 89.0f);

            glm::vec3 move(0.0f);
            if (ImGui::IsKeyDown(ImGuiKey_W)) move += forward();
            if (ImGui::IsKeyDown(ImGuiKey_S)) move -= forward();
            if (ImGui::IsKeyDown(ImGuiKey_D)) move += right();
            if (ImGui::IsKeyDown(ImGuiKey_A)) move -= right();
            if (ImGui::IsKeyDown(ImGuiKey_E)) move += glm::vec3(0.0f, 1.0f, 0.0f);
            if (ImGui::IsKeyDown(ImGuiKey_Q)) move -= glm::vec3(0.0f, 1.0f, 0.0f);

            // Scrolling while flying changes the speed instead of zooming.
            if (io.MouseWheel != 0.0f) {
                flySpeed = std::clamp(flySpeed * std::pow(1.15f, io.MouseWheel), 0.5f, 200.0f);
            }

            const float speed = flySpeed * (io.KeyShift ? 3.0f : 1.0f);
            pivot = eye + move * speed * deltaTime + forward() * distance;
            break;
        }
        case Drag::Orbit:
            yaw += io.MouseDelta.x * LOOK_SENSITIVITY;
            pitch = std::clamp(pitch - io.MouseDelta.y * LOOK_SENSITIVITY, -89.0f, 89.0f);
            break;
        case Drag::Pan:
            pan(io.MouseDelta.x, io.MouseDelta.y, viewportHeight);
            break;
        case Drag::None:
            if (viewportHovered && io.MouseWheel != 0.0f) {
                zoom(io.MouseWheel);
            }
            break;
    }

    return drag != Drag::None;
}
