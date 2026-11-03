#include "include/DemoPhysics.h"
#include "Cube.h"
#include "Plane.h"
#include "Texture.h"
#include "Player.h"
#include "Input.h"
#include "Physics.h"
#include "RenderSettings.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "Sphere.h"
#include "Cylinder.h"
extern glm::vec3 cameraFront;
extern glm::vec3 cameraUp;
extern glm::vec3 cameraPos;

// Активний об’єкт
static std::shared_ptr<Shape> g_controlledShape = nullptr;
static int g_controlledIndex = 0;

static std::string demoAsset(const char* relativePath) {
    return std::string(DEMO_ASSETS_DIR "/") + relativePath;
}

void DemoPhysics::load() {
    shapes.clear();

    skybox = std::make_unique<Skybox>(demoAsset("skybox/night.hdr"));

    auto grassTexture = std::make_shared<Texture>(demoAsset("textures/grass/albedo.jpg").c_str(), "texture_albedo");

    auto floor = std::make_shared<Plane>();
    floor->setPosition(glm::vec3(0.0f, -2.5f, 0.0f));
    floor->setScale(glm::vec3(40.0f, 0.1f, 40.0f));
    floor->setColor(glm::vec3(0.5f, 0.5f, 0.5f));
    floor->isStatic = true;
    floor->hasCollision = true;
    floor->addTexture(grassTexture);
    shapes.push_back(floor);

    auto fallingCube = std::make_shared<Cube>();
    fallingCube->setPosition(glm::vec3(0.5f, 5.0f, 0.0f));
    fallingCube->setColor(glm::vec3(1.0f, 0.5f, 0.0f));
    fallingCube->useGravity = true;
    fallingCube->hasCollision = true;
    auto woodTexture = std::make_shared<Texture>(demoAsset("textures/wood.jpg").c_str(), "texture_albedo");
    fallingCube->addTexture(woodTexture);
    shapes.push_back(fallingCube);

    auto floatingCube = std::make_shared<Cube>();
    floatingCube->setPosition(glm::vec3(-2.5f, 0.0f, 2.0f));
    floatingCube->setColor(glm::vec3(0.0f, 1.0f, 1.0f));
    floatingCube->useGravity = false;
    floatingCube->hasCollision = true;
    auto brickTexture = std::make_shared<Texture>(demoAsset("textures/bricks.png").c_str(), "texture_albedo");
    floatingCube->addTexture(brickTexture);
    shapes.push_back(floatingCube);

    auto floatingSphere = std::make_shared<Sphere>(1.0f);
    floatingSphere->setPosition(glm::vec3(2.5f, 1.0f, 2.0f));
    floatingSphere->setColor(glm::vec3(1.0f, 0.0f, 0.0f));
    floatingSphere->useGravity = false;
    floatingSphere->hasCollision = true;
    shapes.push_back(floatingSphere);

    auto cylinder = std::make_shared<Cylinder>(0.5f, 1.5f, 32);
    cylinder->setPosition(glm::vec3(0.0f, 0.0f, -3.0f));
    cylinder->setColor(glm::vec3(0.7f, 0.2f, 1.0f));
    cylinder->useGravity = false;
    cylinder->hasCollision = true;
    shapes.push_back(cylinder);

    // Активний об’єкт — перший у списку
    g_controlledIndex = 0;
    g_controlledShape = shapes[g_controlledIndex];

    lightPos = glm::vec3(40.0f, 5.0f, -5.0f);
    lightCube = std::make_unique<Cube>();
    lightCube->setPosition(lightPos);
    lightCube->setScale(glm::vec3(0.5f));

    player = std::make_shared<Player>(glm::vec3(0.0f, 0.5f, 2.0f));
    player->setGrounded(true);
}

void DemoPhysics::update(float deltaTime) {

    // Перезавантаження сцени — T
    if (GInput->isKeyPressed(GLFW_KEY_T)) {
        load();
        return;
    }

    // Перемикання об’єкта — N
    if (GInput->isKeyPressed(GLFW_KEY_N)) {
        g_controlledIndex++;
        if (g_controlledIndex >= shapes.size())
            g_controlledIndex = 0;

        g_controlledShape = shapes[g_controlledIndex];
        std::cout << "Selected object #" << g_controlledIndex << std::endl;
    }

    // Освітлення — L
    if (GInput->isKeyPressed(GLFW_KEY_L)) {
        enableLighting = !enableLighting;
        std::cout << "Lighting: " << (enableLighting ? "ON" : "OFF") << std::endl;
    }

    // Тіні — K
    if (GInput->isKeyPressed(GLFW_KEY_K)) {
        enableShadows = !enableShadows;
        std::cout << "Shadows: " << (enableShadows ? "ON" : "OFF") << std::endl;
    }

    // Постпроцесинг — M
    if (GInput->isKeyPressed(GLFW_KEY_M)) {
        enablePostProcessing = !enablePostProcessing;
        std::cout << "PostProcessing: " << (enablePostProcessing ? "ON" : "OFF") << std::endl;
    }

    // ФІЗИКА
    Physics::step(shapes, deltaTime);

    // РУХ ОБ’ЄКТА
    if (g_controlledShape) {
        glm::vec3 pos = g_controlledShape->position;
        float moveSpeed = 3.0f * deltaTime;

        if (GInput->isKeyDown(GLFW_KEY_UP))    pos.z -= moveSpeed;
        if (GInput->isKeyDown(GLFW_KEY_DOWN))  pos.z += moveSpeed;
        if (GInput->isKeyDown(GLFW_KEY_LEFT))  pos.x -= moveSpeed;
        if (GInput->isKeyDown(GLFW_KEY_RIGHT)) pos.x += moveSpeed;

        if (GInput->isKeyDown(GLFW_KEY_KP_ADD) || GInput->isKeyDown(GLFW_KEY_EQUAL))
            pos.y += moveSpeed;

        if (GInput->isKeyDown(GLFW_KEY_KP_SUBTRACT) || GInput->isKeyDown(GLFW_KEY_MINUS))
            pos.y -= moveSpeed;

        g_controlledShape->setPosition(pos);
    }

    // ТРАНСФОРМАЦІЇ
    if (g_controlledShape) {

        if (GInput->isKeyDown(GLFW_KEY_R))
            g_controlledShape->rotate(90.0f * deltaTime, glm::vec3(0, 1, 0));

        if (GInput->isKeyDown(GLFW_KEY_I)) {
            glm::vec3 sc = g_controlledShape->scale;
            sc += glm::vec3(1.0f) * deltaTime;
            g_controlledShape->setScale(sc);
        }

        if (GInput->isKeyDown(GLFW_KEY_O)) {
            glm::vec3 sc = g_controlledShape->scale;
            sc -= glm::vec3(1.0f) * deltaTime;
            sc = glm::max(sc, glm::vec3(0.1f));
            g_controlledShape->setScale(sc);
        }
    }

    // РУХ ГРАВЦЯ
    glm::vec3 moveDir = glm::vec3(0.0f);
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
    glm::vec3 flatRight = glm::normalize(glm::cross(flatFront, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (GInput->isKeyDown(GLFW_KEY_W)) moveDir += flatFront;
    if (GInput->isKeyDown(GLFW_KEY_S)) moveDir -= flatFront;
    if (GInput->isKeyDown(GLFW_KEY_A)) moveDir -= flatRight;
    if (GInput->isKeyDown(GLFW_KEY_D)) moveDir += flatRight;

    if (glm::length(moveDir) > 0) moveDir = glm::normalize(moveDir);

    player->runSpeed = (GInput->isKeyDown(GLFW_KEY_LEFT_SHIFT)) ? 8.0f : 6.0f;

    player->setCrouch(GInput->isKeyDown(GLFW_KEY_C));

    if (GInput->isKeyDown(GLFW_KEY_SPACE))
        player->jump();

    player->move(moveDir);
    player->update(deltaTime, shapes);

    cameraPos = player->getCameraPosition();
}


void DemoPhysics::renderScene(Shader& shader) {
    for (const auto& shape : shapes) {
        shader.setVec3("objectColor", shape->getColor());
        shape->draw(shader);
    }
}

void DemoPhysics::drawShadow(Shader& shadowShader) {
    renderScene(shadowShader);
}

glm::vec3 DemoPhysics::getLightPos() const {
    return lightPos;
}

void DemoPhysics::configureRender(RenderSettings& settings) {
    settings.lighting = enableLighting;
    settings.shadows = enableShadows;
    settings.motionBlur = enablePostProcessing;
}

void DemoPhysics::draw(Shader& lightingShader, Shader& lampShader, const glm::mat4& view, const glm::mat4& proj) {
    renderScene(lightingShader);

    if (skybox) {
        skybox->draw(view, proj);
    }
}

void DemoPhysics::drawDepth(Shader& depthShader) {
    renderScene(depthShader);
}
