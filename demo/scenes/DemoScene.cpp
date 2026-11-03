#include "include/DemoScene.h"
#include "Cube.h"
#include "Plane.h"
#include "Sphere.h"
#include "Texture.h"
#include <GLFW/glfw3.h>

void DemoScene::load() {
    //skybox = std::make_unique<Skybox>("assets/textures/skybox/night.hdr");

    auto floor = std::make_shared<Plane>();
    floor->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    floor->setColor(glm::vec3(0.5f, 0.5f, 0.5f));
    shapes.push_back(floor);

    auto sphere = std::make_shared<Sphere>(1.0f);
    sphere->setPosition(glm::vec3(-1.5f, 0.0f, -2.0f));
    sphere->setColor(glm::vec3(1.0f, 0.0f, 0.0f));
    shapes.push_back(sphere);

    auto cube = std::make_shared<Cube>();
    cube->setPosition(glm::vec3(3.0f, -1.0f, 0.0f));
    auto woodTex = std::make_shared<Texture>(DEMO_ASSETS_DIR "/textures/wood.jpg", "texture_albedo");
    cube->addTexture(woodTex);
    shapes.push_back(cube);



    lightPos = glm::vec3(1.2f, 1.0f, 2.0f);
    lightCube = std::make_unique<Cube>();
    lightCube->setPosition(lightPos);
    lightCube->setScale(glm::vec3(0.2f));
}

void DemoScene::update(float /*deltaTime*/) {
    lightPos.x = 1.0f + sin(glfwGetTime()) * 2.0f;
    lightPos.z = sin(glfwGetTime() / 2.0f) * 1.0f;
    lightCube->setPosition(lightPos);
}

void DemoScene::draw(Shader& lightingShader, Shader& lampShader,
                     const glm::mat4& view, const glm::mat4& proj)
{
    lightingShader.setVec3("lightPos", lightPos);
    lightingShader.setVec3("lightColor", glm::vec3(1.0f));
    glDisable(GL_CULL_FACE);

    for (const auto& shape : shapes) {
        lightingShader.setVec3("objectColor", shape->getColor());
        shape->draw(lightingShader); // shape всередині ставить model
    }

    // Лампа (кубик світла)
    lampShader.use();
    lampShader.setMat4("projection", proj);
    lampShader.setMat4("view", view);
    lightCube->setPosition(lightPos);
    lightCube->draw(lampShader);

    // Skybox
    if (skybox) {
        skybox->draw(view, proj);
    }
}

void DemoScene::drawShadow(Shader& shadowShader)
{
    drawDepth(shadowShader);
}

void DemoScene::drawDepth(Shader& depthShader)
{
    depthShader.use();

    // Для тіней інколи корисно включити culling front face (боротьба з shadow acne)
    glDisable(GL_CULL_FACE);

    for (const auto& shape : shapes) {
        shape->draw(depthShader); // тільки геометрія, без лампи, без skybox
    }
}
