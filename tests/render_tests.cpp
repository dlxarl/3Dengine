// Tests that need an OpenGL context. Each render setting is exercised on the real pipeline,
// so that a control in the Render Settings panel cannot quietly stop doing anything.
#include "AssetManager.h"
#include "EditorCamera.h"
#include "GameSession.h"
#include "GlWindow.h"
#include "MeshShape.h"
#include "ObjectFactory.h"
#include "Physics.h"
#include "PlayController.h"
#include "Player.h"
#include "RenderTarget.h"
#include "Renderer.h"
#include "Resources.h"
#include "SceneDocument.h"
#include "SceneGraph.h"
#include "SceneSerializer.h"
#include "TestSupport.h"
#include "ViewportRenderer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <set>
#include <vector>

namespace fs = std::filesystem;

namespace {
    const int WIDTH = 320;
    const int HEIGHT = 240;

    using Image = std::vector<unsigned char>;

    // An 8x8 checkerboard as an uncompressed TGA, the simplest format stb_image reads.
    void writeCheckerTexture(const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);

        unsigned char header[18] = {};
        header[2] = 2;
        header[12] = 8;
        header[14] = 8;
        header[16] = 24;
        file.write(reinterpret_cast<const char*>(header), sizeof(header));

        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                const unsigned char value = ((x + y) & 1) ? 240 : 30;
                const unsigned char pixel[3] = {value, value, value};
                file.write(reinterpret_cast<const char*>(pixel), 3);
            }
        }
    }

    Shape* addShape(SceneGraph& scene, ObjectType type, glm::vec3 position, glm::vec3 scale, glm::vec3 color) {
        auto object = ObjectFactory::create(type);
        object->name = ObjectFactory::typeName(type);
        object->setPosition(position);
        object->setScale(scale);
        auto* shape = static_cast<Shape*>(scene.add(object));
        shape->setColor(color);
        return shape;
    }

    // A tilted textured floor, which shows filtering and affine warping, and objects that
    // catch light and throw shadows.
    void buildScene(SceneGraph& scene, AssetManager& assets) {
        Shape* floor = addShape(scene, ObjectType::Plane, glm::vec3(0.0f), glm::vec3(16.0f, 0.1f, 16.0f), glm::vec3(1.0f));
        floor->setTexture("texture_albedo", assets.getTexture("Assets/Textures/checker.tga"));

        addShape(scene, ObjectType::Cube, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.5f), glm::vec3(0.9f, 0.4f, 0.2f));
        addShape(scene, ObjectType::Sphere, glm::vec3(2.5f, 1.0f, 1.0f), glm::vec3(1.8f), glm::vec3(0.2f, 0.5f, 0.9f));

        auto light = ObjectFactory::create(ObjectType::Light);
        light->setPosition(glm::vec3(6.0f, 10.0f, 4.0f));
        scene.add(light);
    }

    CameraView makeCamera(glm::vec3 eye) {
        CameraView camera;
        camera.position = eye;
        camera.view = glm::lookAt(eye, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        camera.projection = glm::perspective(glm::radians(50.0f), (float)WIDTH / HEIGHT, 0.1f, 200.0f);
        return camera;
    }

    Image readImage(unsigned int framebuffer) {
        Image pixels(WIDTH * HEIGHT * 4);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return pixels;
    }

    int differingPixels(const Image& a, const Image& b) {
        int count = 0;
        for (size_t i = 0; i < a.size(); i += 4) {
            if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) count++;
        }
        return count;
    }

    int distinctChannelValues(const Image& image) {
        std::set<unsigned char> values;
        for (size_t i = 0; i < image.size(); i += 4) {
            values.insert(image[i]);
            values.insert(image[i + 1]);
            values.insert(image[i + 2]);
        }
        return (int)values.size();
    }

    int identicalAdjacentRows(const Image& image) {
        int identical = 0;
        for (int y = 0; y + 1 < HEIGHT; y++) {
            const unsigned char* a = image.data() + (size_t)y * WIDTH * 4;
            const unsigned char* b = a + WIDTH * 4;
            if (std::equal(a, a + WIDTH * 4, b)) identical++;
        }
        return identical;
    }

    class RenderFixture {
    public:
        RenderFixture(AssetManager& assets) : renderer(1024) {
            target.resize(WIDTH, HEIGHT);
            buildScene(scene, assets);
        }

        Image render(const RenderSettings& settings, glm::vec3 eye = glm::vec3(5.0f, 5.0f, 9.0f)) {
            renderer.render(scene, makeCamera(eye), RenderOutput{target.framebuffer(), WIDTH, HEIGHT}, settings);
            return readImage(target.framebuffer());
        }

        SceneGraph scene;
        Renderer renderer;
        RenderTarget target;
    };

    void testRenderSettings(AssetManager& assets) {
        RenderFixture fixture(assets);
        const RenderSettings base;
        const Image reference = fixture.render(base);

        CHECK(distinctChannelValues(reference) > 32);

        // Deterministic, or every comparison below would prove nothing.
        CHECK(differingPixels(reference, fixture.render(base)) == 0);

        // Internal resolution: rows repeat when a low-resolution image is upscaled.
        RenderSettings lowRes = base;
        lowRes.internalHeight = 60;
        const Image lowResImage = fixture.render(lowRes);
        CHECK(differingPixels(reference, lowResImage) > 500);
        CHECK(identicalAdjacentRows(lowResImage) > identicalAdjacentRows(reference) + 100);

        // Texture filtering
        RenderSettings nearest = base;
        nearest.textureFilter = TextureFilter::Nearest;
        RenderSettings bilinear = base;
        bilinear.textureFilter = TextureFilter::Bilinear;
        const Image nearestImage = fixture.render(nearest);
        CHECK(differingPixels(reference, nearestImage) > 200);
        CHECK(differingPixels(nearestImage, fixture.render(bilinear)) > 200);

        // Color quantization: 3 bits per channel can only produce 8 values.
        RenderSettings quantized = base;
        quantized.colorBits = 3;
        const Image quantizedImage = fixture.render(quantized);
        CHECK(distinctChannelValues(quantizedImage) <= 8);
        CHECK(differingPixels(reference, quantizedImage) > 1000);

        // Dithering
        RenderSettings dithered = quantized;
        dithered.dithering = true;
        const Image ditheredImage = fixture.render(dithered);
        CHECK(differingPixels(quantizedImage, ditheredImage) > 1000);
        CHECK(distinctChannelValues(ditheredImage) <= 8);

        // Dithering does nothing without quantization, which is why the panel disables it.
        RenderSettings ditherOnly = base;
        ditherOnly.dithering = true;
        CHECK(differingPixels(reference, fixture.render(ditherOnly)) == 0);

        // Vertex precision
        RenderSettings snapped = base;
        snapped.vertexPrecision = VertexPrecision::Low;
        CHECK(differingPixels(reference, fixture.render(snapped)) > 200);

        // Rendering mode: per-vertex lighting, and affine against perspective-correct textures.
        RenderSettings ps2 = base;
        ps2.mode = RenderMode::PS2;
        RenderSettings ps1 = base;
        ps1.mode = RenderMode::PS1;
        const Image ps2Image = fixture.render(ps2);
        CHECK(differingPixels(reference, ps2Image) > 500);
        CHECK(differingPixels(ps2Image, fixture.render(ps1)) > 500);

        // Lighting and shadows
        RenderSettings unlit = base;
        unlit.lighting = false;
        CHECK(differingPixels(reference, fixture.render(unlit)) > 1000);

        RenderSettings noShadows = base;
        noShadows.shadows = false;
        CHECK(differingPixels(reference, fixture.render(noShadows)) > 100);

        // Motion blur needs movement between frames.
        const glm::vec3 eyeA(5.0f, 5.0f, 9.0f);
        const glm::vec3 eyeB(5.6f, 5.0f, 8.6f);
        fixture.render(base, eyeA);
        const Image sharp = fixture.render(base, eyeB);
        RenderSettings blurred = base;
        blurred.motionBlur = true;
        fixture.render(blurred, eyeA);
        CHECK(differingPixels(sharp, fixture.render(blurred, eyeB)) > 200);

        // A disabled shadow pass must not leave the scene black, and the presets must render.
        for (RenderMode mode : {RenderMode::PS1, RenderMode::PS2, RenderMode::Modern}) {
            CHECK(distinctChannelValues(fixture.render(RenderSettings::preset(mode))) > 3);
        }
    }

    void testViewportRenderer(AssetManager& assets) {
        SceneGraph scene;
        buildScene(scene, assets);

        ViewportRenderer viewport;
        viewport.resize(WIDTH, HEIGHT);

        const CameraView camera = makeCamera(glm::vec3(0.0f, 1.0f, 9.0f));

        Shape* cube = nullptr;
        for (const auto& shape : scene.getShapes()) {
            if (shape->getType() == ObjectType::Cube) cube = shape.get();
        }
        CHECK(cube != nullptr);
        if (!cube) return;

        // The cube sits in the middle of this view; well above it there is only background.
        CHECK(viewport.pick(scene, camera, WIDTH / 2, HEIGHT / 2) == cube->id);
        CHECK(viewport.pick(scene, camera, WIDTH / 2, 8) == InvalidObjectId);
        CHECK(viewport.pick(scene, camera, -1, 0) == InvalidObjectId);
        CHECK(viewport.pick(scene, camera, WIDTH, HEIGHT) == InvalidObjectId);

        cube->visible = false;
        CHECK(viewport.pick(scene, camera, WIDTH / 2, HEIGHT / 2) != cube->id);
        cube->visible = true;

        // The editor camera frames the origin in both projections.
        EditorCamera editorCamera;
        const float aspect = (float)WIDTH / HEIGHT;
        CHECK(viewport.pick(scene, editorCamera.makeView(aspect), WIDTH / 2, HEIGHT / 2) == cube->id);
        editorCamera.orthographic = true;
        const CameraView orthographic = editorCamera.makeView(aspect);
        CHECK(orthographic.projection[3][3] == 1.0f);
        CHECK(viewport.pick(scene, orthographic, WIDTH / 2, HEIGHT / 2) == cube->id);

        const RenderSettings settings;
        viewport.renderScene(scene, camera, settings, InvalidObjectId);

        // The image is drawn in a panel with blending, so its alpha has to stay opaque.
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        Image pixels(WIDTH * HEIGHT * 4);
        glBindTexture(GL_TEXTURE_2D, viewport.texture());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        bool opaque = true;
        for (size_t i = 3; i < pixels.size(); i += 4) {
            opaque = opaque && pixels[i] == 255;
        }
        CHECK(opaque);

        // The outline adds the accent color around the selection, and nothing else moves.
        viewport.renderScene(scene, camera, settings, cube->id);
        Image outlined(WIDTH * HEIGHT * 4);
        glBindTexture(GL_TEXTURE_2D, viewport.texture());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, outlined.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        const int changed = differingPixels(pixels, outlined);
        CHECK(changed > 50);
        CHECK(changed < WIDTH * HEIGHT / 8);
    }

    void testHierarchy() {
        SceneGraph scene;

        auto makeEmpty = [&](glm::vec3 position) {
            auto object = ObjectFactory::create(ObjectType::Empty);
            object->setPosition(position);
            return scene.add(object);
        };

        SceneObject* a = makeEmpty(glm::vec3(10.0f, 0.0f, 0.0f));
        SceneObject* b = makeEmpty(glm::vec3(0.0f, 5.0f, 0.0f));
        SceneObject* c = makeEmpty(glm::vec3(1.0f, 1.0f, 1.0f));

        CHECK(a->id != b->id);
        CHECK(scene.getRoots().size() == 3);

        // Reparenting keeps the object where it is.
        CHECK(scene.reparent(b->id, a->id));
        CHECK(glm::length(b->getWorldPosition() - glm::vec3(0.0f, 5.0f, 0.0f)) < 0.0001f);
        CHECK(glm::length(b->position - glm::vec3(-10.0f, 5.0f, 0.0f)) < 0.0001f);
        CHECK(scene.getRoots().size() == 2);

        // Children follow their parent, through more than one level.
        CHECK(scene.reparent(c->id, b->id));
        a->setPosition(glm::vec3(20.0f, 0.0f, 0.0f));
        CHECK(glm::length(c->getWorldPosition() - glm::vec3(11.0f, 1.0f, 1.0f)) < 0.0001f);

        // Cycles are refused.
        CHECK(!scene.reparent(a->id, c->id));
        CHECK(!scene.reparent(a->id, a->id));
        CHECK(a->getParent() == nullptr);

        // Reordering among siblings leaves the transform untouched.
        SceneObject* d = makeEmpty(glm::vec3(0.0f));
        CHECK(scene.getRoots().back() == d);
        CHECK(scene.reparent(d->id, InvalidObjectId, 0));
        CHECK(scene.getRoots().front() == d);

        CHECK(scene.makeUniqueName("Cube") == "Cube");

        // Removing a parent removes its subtree.
        const ObjectId bId = b->id;
        const ObjectId cId = c->id;
        scene.remove(a->id);
        CHECK(scene.find(bId) == nullptr);
        CHECK(scene.find(cId) == nullptr);
        CHECK(scene.getObjects().size() == 1);
        CHECK(scene.getRoots().size() == 1);
    }

    void testTransformDecomposition() {
        const glm::vec3 cases[] = {{0, 0, 0}, {10, 20, 30}, {-45, 80, 170}, {90, 0, 0}, {0, 45, 0}, {120, -30, 60}};
        for (const glm::vec3& rotation : cases) {
            auto object = ObjectFactory::create(ObjectType::Empty);
            object->setPosition(glm::vec3(1.0f, 2.0f, 3.0f));
            object->setRotation(rotation);
            object->setScale(glm::vec3(2.0f, 3.0f, 0.5f));

            const glm::mat4 original = object->getWorldMatrix();
            object->setWorldMatrix(original);

            bool same = true;
            for (int column = 0; column < 4; column++) {
                same = same && glm::length(original[column] - object->getWorldMatrix()[column]) < 0.001f;
            }
            CHECK(same);
        }
    }

    void testSerialization(AssetManager& assets) {
        SceneGraph scene;
        buildScene(scene, assets);
        scene.clearColor = glm::vec3(0.2f, 0.3f, 0.4f);

        SceneObject* cube = scene.getShapes()[1].get();
        cube->name = "Кубик";
        cube->visible = false;
        cube->setRotation(glm::vec3(10.0f, 20.0f, 30.0f));
        static_cast<Shape*>(cube)->castShadow = false;
        CHECK(scene.reparent(cube->id, scene.getShapes()[0]->id));

        const std::string first = SceneSerializer::serialize(scene);

        SceneGraph loaded;
        std::string error;
        CHECK(SceneSerializer::deserialize(first, loaded, assets, &error));
        CHECK(loaded.getObjects().size() == scene.getObjects().size());

        // Saving what was loaded gives the identical file, so reloading loses nothing.
        CHECK(SceneSerializer::serialize(loaded) == first);

        SceneObject* restored = loaded.find(cube->id);
        CHECK(restored != nullptr);
        if (restored) {
            CHECK(restored->name == "Кубик");
            CHECK(!restored->visible);
            CHECK(restored->getParent() != nullptr);
            CHECK(glm::length(restored->getWorldPosition() - cube->getWorldPosition()) < 0.0001f);
        }
        CHECK(loaded.getShapes()[0]->getTexture("texture_albedo") != nullptr);
        CHECK(loaded.getShapes()[0]->getTexture("texture_albedo")->assetPath == "Assets/Textures/checker.tga");
        CHECK(glm::length(loaded.clearColor - scene.clearColor) < 0.0001f);

        // New objects never reuse a saved id.
        SceneObject* extra = loaded.add(ObjectFactory::create(ObjectType::Empty));
        CHECK(scene.find(extra->id) == nullptr);

        // Damaged files are refused without crashing.
        for (const char* broken : {"", "{", "{\"version\":1}", "{\"version\":99,\"objects\":[]}",
                                   R"({"version":1,"objects":[{"id":1,"type":"cube"},{"id":1,"type":"cube"}]})",
                                   R"({"version":1,"objects":[{"id":0,"type":"cube"}]})"}) {
            SceneGraph target;
            CHECK(!SceneSerializer::deserialize(broken, target, assets, &error));
            CHECK(target.getObjects().empty());
        }

        // Unknown object types are skipped, and a parent cycle is broken, not looped on.
        SceneGraph tolerant;
        CHECK(SceneSerializer::deserialize(
            R"({"version":1,"objects":[{"id":1,"type":"hologram"},{"id":2,"type":"cube"},
                {"id":3,"type":"empty","parent":4},{"id":4,"type":"empty","parent":3}]})",
            tolerant, assets, &error));
        CHECK(tolerant.getObjects().size() == 3);
    }

    void testDuplicate(AssetManager& assets) {
        SceneGraph scene;
        buildScene(scene, assets);

        SceneObject* parent = scene.getShapes()[1].get();
        SceneObject* child = scene.getShapes()[2].get();
        CHECK(scene.reparent(child->id, parent->id));
        const size_t before = scene.getObjects().size();

        SceneObject* copy = SceneSerializer::duplicate(scene, parent->id, assets);
        CHECK(copy != nullptr);
        if (!copy) return;

        CHECK(copy->id != parent->id);
        CHECK(copy->name == parent->name + " (1)");
        CHECK(scene.getObjects().size() == before + 2);
        CHECK(copy->getChildren().size() == 1);
        CHECK(copy->getChildren()[0]->id != child->id);
        CHECK(glm::length(copy->getWorldPosition() - parent->getWorldPosition()) < 0.0001f);

        // Sits right after the original.
        const auto& roots = scene.getRoots();
        const auto original = std::find(roots.begin(), roots.end(), parent);
        CHECK(original != roots.end() && *(original + 1) == copy);
    }

    void testMeshAssets(AssetManager& assets, const fs::path& root) {
        {
            fs::create_directories(root / "Assets/Models");
            std::ofstream file(root / "Assets/Models/tri.obj");
            file << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        }

        CHECK(assets.getMesh("Assets/Models/tri.obj") != nullptr);
        CHECK(assets.getMesh("Assets/Models/missing.obj") == nullptr);
        CHECK(assets.getMesh("../escape.obj") == nullptr);

        // The parsed result lands in the cache, and survives the source being re-read.
        bool cached = false;
        for (const auto& entry : fs::directory_iterator(root / "Library/Meshes")) {
            cached = cached || entry.path().extension() == ".mesh";
        }
        CHECK(cached);

        assets.invalidate("Assets/Models/tri.obj");
        CHECK(assets.getMesh("Assets/Models/tri.obj") != nullptr);

        auto object = std::make_shared<MeshShape>();
        object->setMesh(assets.getMesh("Assets/Models/tri.obj"), "Assets/Models/tri.obj");
        SceneGraph scene;
        scene.add(object);

        SceneGraph loaded;
        std::string error;
        CHECK(SceneSerializer::deserialize(SceneSerializer::serialize(scene), loaded, assets, &error));
        auto* restored = dynamic_cast<MeshShape*>(loaded.getShapes()[0].get());
        CHECK(restored && restored->getAssetPath() == "Assets/Models/tri.obj");
    }

    void testPhysics() {
        SceneGraph scene;
        Shape* floor = addShape(scene, ObjectType::Plane, glm::vec3(0.0f), glm::vec3(10.0f, 0.1f, 10.0f), glm::vec3(1.0f));
        Shape* box = addShape(scene, ObjectType::Cube, glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
        box->useGravity = true;

        for (int i = 0; i < 240; i++) {
            scene.update(1.0f / 120.0f);
        }

        CHECK(std::abs(box->position.y - 0.55f) < 0.1f);
        CHECK(box->position.y >= 0.55f);
        CHECK(floor->position.y == 0.0f);

        // A static object ignores gravity.
        box->isStatic = true;
        box->setPosition(glm::vec3(0.0f, 5.0f, 0.0f));
        scene.update(0.1f);
        CHECK(box->position.y == 5.0f);
    }

    // A flat strip along -Z with the player at its near end, and a camera to follow it.
    struct GameLevel {
        SceneGraph scene;
        Player* player = nullptr;

        GameLevel() {
            addShape(scene, ObjectType::Cube, glm::vec3(0.0f, -0.5f, -10.0f), glm::vec3(10.0f, 1.0f, 30.0f), glm::vec3(1.0f))->isStatic = true;

            auto playerObject = ObjectFactory::create(ObjectType::Player);
            playerObject->setPosition(glm::vec3(0.0f, 1.2f, 0.0f));
            player = static_cast<Player*>(scene.add(playerObject));

            scene.add(ObjectFactory::create(ObjectType::Camera));
        }

        Shape* addRole(GameRole role, glm::vec3 position) {
            Shape* shape = addShape(scene, ObjectType::Cube, position, glm::vec3(1.0f), glm::vec3(1.0f));
            shape->gameRole = role;
            shape->isStatic = true;
            return shape;
        }

        void run(GameSession& game, const GameInput& input, float seconds) {
            const float step = 1.0f / 120.0f;
            for (float time = 0.0f; time < seconds; time += step) {
                game.update(scene, input, step);
                scene.update(step);
            }
        }
    };

    void testGameRules() {
        GameInput forward;
        forward.moveForward = 1.0f;

        {
            // A scene without a player is simply not a game.
            SceneGraph scene;
            GameSession game;
            game.begin(scene);
            CHECK(!game.getStatus().hasPlayer);
            game.update(scene, forward, 0.1f);
        }

        // The goal stays shut until every pickup is in, and pickups do not block the way.
        {
            GameLevel level;
            Shape* pickup = level.addRole(GameRole::Pickup, glm::vec3(0.0f, 1.0f, -3.0f));
            Shape* goal = level.addRole(GameRole::Goal, glm::vec3(0.0f, 1.0f, -8.0f));
            level.addRole(GameRole::Pickup, glm::vec3(3.0f, 1.0f, 0.0f));

            GameSession game;
            game.begin(level.scene);
            CHECK(game.getStatus().hasPlayer);
            CHECK(game.getStatus().total == 2);

            level.run(game, GameInput(), 0.5f);
            const float grounded = level.player->position.y;
            CHECK(grounded > 0.9f && grounded < 1.2f);

            // Walks through the first pickup and up to the goal, which stays locked.
            level.run(game, forward, 1.2f);
            CHECK(game.getStatus().collected == 1);
            CHECK(!pickup->visible);
            CHECK(game.getStatus().goalLocked);
            CHECK(!game.getStatus().won);

            // A second touch does not count twice, and the other pickup opens the goal.
            GameInput right;
            right.moveRight = 1.0f;
            level.player->setPosition(glm::vec3(0.0f, 1.1f, 0.0f));
            level.run(game, right, 1.0f);
            CHECK(game.getStatus().collected == 2);

            level.player->setPosition(glm::vec3(0.0f, 1.1f, -6.0f));
            level.run(game, forward, 0.5f);
            CHECK(game.getStatus().won);

            // A finished run freezes the player and the clock.
            const glm::vec3 finished = level.player->position;
            const float time = game.getStatus().seconds;
            level.run(game, forward, 0.5f);
            CHECK(level.player->position == finished);
            CHECK(game.getStatus().seconds == time);
            (void)goal;
        }

        // A goal alone is reachable at once: zero pickups means nothing to wait for.
        {
            GameLevel level;
            level.addRole(GameRole::Goal, glm::vec3(0.0f, 1.0f, -3.0f));
            GameSession game;
            game.begin(level.scene);
            level.run(game, forward, 1.0f);
            CHECK(game.getStatus().won);
        }

        // Hazards and long falls send the player back, keeping what was collected.
        {
            GameLevel level;
            level.addRole(GameRole::Pickup, glm::vec3(0.0f, 1.0f, -2.0f));
            level.addRole(GameRole::Hazard, glm::vec3(0.0f, 1.0f, -6.0f));

            GameSession game;
            game.begin(level.scene);
            level.run(game, forward, 1.2f);
            CHECK(game.getStatus().falls == 1);
            CHECK(game.getStatus().collected == 1);
            CHECK(level.player->position.z > -3.0f);

            GameInput right;
            right.moveRight = 1.0f;
            level.run(game, right, 2.2f);
            CHECK(game.getStatus().falls >= 2);
            CHECK(std::abs(level.player->position.x) < 4.0f);
        }

        // Jumping needs ground, and clears about 1.5 units.
        {
            GameLevel level;
            GameSession game;
            game.begin(level.scene);
            level.run(game, GameInput(), 0.5f);
            const float ground = level.player->position.y;

            GameInput jump;
            jump.jump = true;
            float highest = ground;
            for (int i = 0; i < 60; i++) {
                level.run(game, i < 6 ? jump : GameInput(), 1.0f / 120.0f);
                highest = std::max(highest, level.player->position.y);
            }
            CHECK(highest > ground + 1.0f);
            CHECK(highest < ground + 2.0f);

            // Walking off a ledge leaves a moment to jump, but not a jump in mid-air.
            GameLevel ledge;
            GameSession ledgeGame;
            ledgeGame.begin(ledge.scene);
            ledge.player->setPosition(glm::vec3(0.0f, 4.0f, 0.0f));
            ledge.run(ledgeGame, GameInput(), 0.2f);
            const float falling = ledge.player->velocity.y;
            CHECK(falling < 0.0f);
            ledge.run(ledgeGame, jump, 1.0f / 120.0f);
            CHECK(ledge.player->velocity.y < 0.0f);
        }

        // A patrol swings around its start and stays within its range.
        {
            GameLevel level;
            Shape* hazard = level.addRole(GameRole::Hazard, glm::vec3(20.0f, 1.0f, 0.0f));
            hazard->patrolOffset = glm::vec3(0.0f, 0.0f, 4.0f);
            hazard->patrolSeconds = 2.0f;

            GameSession game;
            game.begin(level.scene);
            level.run(game, GameInput(), 0.5f);
            CHECK(std::abs(hazard->position.z - 4.0f) < 0.1f);
            level.run(game, GameInput(), 1.0f);
            CHECK(std::abs(hazard->position.z + 4.0f) < 0.1f);
            CHECK(hazard->position.x == 20.0f);
        }

        // The camera trails the player, and moving is relative to where it looks.
        {
            GameLevel level;
            GameSession game;
            game.begin(level.scene);
            level.run(game, GameInput(), 0.2f);

            CameraObject* camera = level.scene.getMainCamera();
            CHECK(camera->getWorldPosition().z > level.player->position.z + 5.0f);
            CHECK(camera->getWorldPosition().y > level.player->position.y + 2.0f);

            GameInput turned = forward;
            turned.turnDegrees = 90.0f;
            game.update(level.scene, turned, 1.0f / 120.0f);
            level.run(game, forward, 0.5f);
            CHECK(level.player->position.x > 1.0f);
            CHECK(std::abs(level.player->position.z) < 0.5f);
        }

        // First person: the camera is at the eyes, the body only casts a shadow, and looking
        // decides where walking goes, without tilting into the ground.
        {
            GameLevel level;
            level.scene.getMainCamera()->firstPerson = true;
            GameSession game;
            game.begin(level.scene);
            CHECK(game.getStatus().firstPerson);
            CHECK(level.player->shadowOnly);
            level.run(game, GameInput(), 0.5f);

            CameraObject* camera = level.scene.getMainCamera();
            CHECK(glm::length(camera->getWorldPosition() - level.player->getCameraPosition()) < 0.05f);

            GameInput look;
            look.turnDegrees = 90.0f;
            look.lookUpDegrees = -200.0f;
            game.update(level.scene, look, 1.0f / 120.0f);
            const glm::vec3 lookDirection = -glm::normalize(glm::vec3(camera->getWorldMatrix()[2]));
            CHECK(lookDirection.y < -0.98f);
            CHECK(lookDirection.y > -1.0f);

            level.run(game, forward, 0.5f);
            CHECK(level.player->position.x > 2.0f);
            CHECK(std::abs(level.player->position.z) < 0.1f);

            game.setFirstPerson(level.scene, false);
            CHECK(!level.player->shadowOnly);
            CHECK(glm::length(camera->getWorldPosition() - level.player->getCameraPosition()) > 5.0f);
        }

        // Physics leaves the player to its own movement.
        {
            GameLevel level;
            level.player->velocity = glm::vec3(1.0f, 0.0f, 0.0f);
            const glm::vec3 before = level.player->position;
            level.scene.update(0.1f);
            CHECK(level.player->position == before);
        }
    }

    void testGameSerialization(AssetManager& assets) {
        GameLevel level;
        Shape* hazard = level.addRole(GameRole::Hazard, glm::vec3(1.0f));
        hazard->patrolOffset = glm::vec3(0.0f, 2.0f, 0.0f);
        hazard->patrolSeconds = 4.5f;

        SceneGraph loaded;
        std::string error;
        CHECK(SceneSerializer::deserialize(SceneSerializer::serialize(level.scene), loaded, assets, &error));
        CHECK(loaded.getPlayer() != nullptr);

        Shape* restored = loaded.findAs<Shape>(hazard->id);
        CHECK(restored && restored->gameRole == GameRole::Hazard);
        CHECK(restored && restored->patrolSeconds == 4.5f);
        CHECK(restored && restored->patrolOffset == glm::vec3(0.0f, 2.0f, 0.0f));
        CHECK(!loaded.getMainCamera()->firstPerson);
    }

    void testPlayRestart(Project& project, AssetManager& assets) {
        SceneDocument document(project, assets);
        document.createNew();

        SceneGraph& scene = document.getScene();
        auto playerObject = ObjectFactory::create(ObjectType::Player);
        playerObject->setPosition(glm::vec3(0.0f, 10.0f, 0.0f));
        const ObjectId playerId = scene.add(playerObject)->id;

        auto pickupObject = ObjectFactory::create(ObjectType::Cube);
        pickupObject->setPosition(glm::vec3(0.0f, 9.0f, 0.0f));
        Shape* pickup = static_cast<Shape*>(scene.add(pickupObject));
        pickup->gameRole = GameRole::Pickup;
        pickup->isStatic = true;
        const ObjectId pickupId = pickup->id;

        PlayController play(document);
        play.play();
        for (int i = 0; i < 30; i++) {
            play.update(1.0f / 60.0f, GameInput());
        }
        CHECK(play.getGameStatus().collected == 1);

        // Restarting puts everything back and stays in Play mode.
        play.restart();
        CHECK(play.isPlaying());
        CHECK(play.getGameStatus().collected == 0);
        CHECK(play.getGameStatus().seconds == 0.0f);
        CHECK(document.getScene().find(playerId)->position.y == 10.0f);
        CHECK(document.getScene().find(pickupId)->visible);

        play.stop();
        CHECK(document.getScene().findAs<Shape>(pickupId)->hasCollision);
    }

    void testPlayMode(Project& project, AssetManager& assets) {
        SceneDocument document(project, assets);
        document.createNew();

        Shape* box = addShape(document.getScene(), ObjectType::Cube, glm::vec3(0.0f, 5.0f, 0.0f), glm::vec3(1.0f), glm::vec3(1.0f));
        box->useGravity = true;
        const ObjectId id = box->id;

        PlayController play(document);
        play.play();
        CHECK(play.isPlaying());
        CHECK(!document.isDirty());

        // Paused means frozen.
        play.update(0.05f, GameInput());
        const float afterStep = document.getScene().find(id)->position.y;
        CHECK(afterStep < 5.0f);
        play.togglePause();
        CHECK(play.isPaused());
        play.update(0.05f, GameInput());
        CHECK(document.getScene().find(id)->position.y == afterStep);
        play.togglePause();

        // Nothing done during play counts as an edit, or survives it.
        document.getScene().find(id)->name = "Changed";
        document.markDirty();
        CHECK(!document.isDirty());

        play.stop();
        CHECK(play.isStopped());
        SceneObject* restored = document.getScene().find(id);
        CHECK(restored != nullptr);
        if (restored) {
            CHECK(restored->position.y == 5.0f);
            CHECK(restored->name == "Cube");
        }

        // Edits count again afterwards.
        document.markDirty();
        CHECK(document.isDirty());

        // A failed open keeps the scene that was open.
        std::string error;
        const size_t objects = document.getScene().getObjects().size();
        CHECK(!document.open("Assets/Scenes/DoesNotExist.scene", &error));
        CHECK(document.getScene().getObjects().size() == objects);

        CHECK(document.saveAs("Assets/Scenes/Saved Копія.scene", &error));
        CHECK(!document.isDirty());
        CHECK(project.startupScene == "Assets/Scenes/Saved Копія.scene");
        CHECK(fs::exists(project.getRoot() / fs::u8path("Assets/Scenes/Saved Копія.scene")));
    }
}

int main() {
    GlWindow::Config config;
    config.width = 64;
    config.height = 64;
    config.title = "render tests";
    config.visible = false;
    GlWindow window(config);

    // A machine without a display cannot run these; 77 tells CTest it is a skip, not a failure.
    if (!window.isValid()) {
        std::printf("render_tests: no OpenGL context available, skipping\n");
        return 77;
    }

    TempDir temp;
    ProjectCreateOptions options;
    options.name = "Render Tests";
    options.location = temp.path;
    auto project = Project::create(options);
    if (!project) return 1;

    writeCheckerTexture(project->assetsDir() / "Textures" / "checker.tga");

    {
        AssetManager assets;
        assets.setProject(project->getRoot(), project->libraryDir());

        testRenderSettings(assets);
        testViewportRenderer(assets);
        testHierarchy();
        testTransformDecomposition();
        testSerialization(assets);
        testDuplicate(assets);
        testMeshAssets(assets, project->getRoot());
        testPhysics();
        testGameRules();
        testGameSerialization(assets);
        testPlayRestart(*project, assets);
        testPlayMode(*project, assets);
    }

    return testSummary("render_tests");
}
