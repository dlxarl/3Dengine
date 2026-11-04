#include "AssetImporter.h"
#include "AudioEdit.h"
#include "EditMesh.h"
#include "RayTrace.h"
#include "AssetManager.h"
#include "ObjLoader.h"
#include "Project.h"
#include "RecentProjects.h"
#include "SceneSerializer.h"
#include "TestSupport.h"
#include "UserPaths.h"

#include <fstream>

namespace fs = std::filesystem;

namespace {
    void writeFile(const fs::path& path, const std::string& text) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        file << text;
    }

    // Smallest valid 1x1 PNG, used to exercise real format validation.
    const unsigned char PNG_1X1[] = {
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
        0x89, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
        0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99, 0x3D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
        0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
    };

    const char* const CUBE_FACE_OBJ =
        "# a quad and a triangle\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/1/1\n"
        "f -4 -3 -2\n";

    void testNameValidation() {
        CHECK(validateProjectName("MyGame").empty());
        CHECK(validateProjectName("My Game 2").empty());
        CHECK(validateProjectName("Гра").empty());
        CHECK(!validateProjectName("").empty());
        CHECK(!validateProjectName("a/b").empty());
        CHECK(!validateProjectName("a:b").empty());
        CHECK(!validateProjectName(".hidden").empty());
        CHECK(!validateProjectName(" padded").empty());
        CHECK(!validateProjectName(std::string(65, 'x')).empty());
    }

    void testCreateProducesExpectedLayout() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Test Game";
        options.location = temp.path / "with space" / "Проекти";

        std::string error;
        auto project = Project::create(options, &error);
        CHECK(project.has_value());
        if (!project) {
            std::printf("  create failed: %s\n", error.c_str());
            return;
        }

        const fs::path root = project->getRoot();
        for (const char* dir : {"Assets/Models", "Assets/Textures", "Assets/Materials", "Assets/Scenes",
                                "Assets/Audio", "Assets/Scripts", "ProjectSettings", "Packages", "Library"}) {
            CHECK(fs::is_directory(root / dir));
        }
        CHECK(fs::is_regular_file(root / "Project.json"));
        CHECK(fs::is_regular_file(root / "ProjectSettings/Project.json"));
        CHECK(fs::is_regular_file(root / "ProjectSettings/Graphics.json"));
        CHECK(fs::is_regular_file(root / "Assets/Scenes/Main.scene"));

        CHECK(Project::validate(root).isValid());

        auto reopened = Project::open(root, &error);
        CHECK(reopened.has_value());
        if (!reopened) return;
        CHECK(reopened->getInfo().name == "Test Game");
        CHECK(reopened->getInfo().id == project->getInfo().id);
        CHECK(reopened->getInfo().id.size() == 32);
        CHECK(reopened->startupScene == "Assets/Scenes/Main.scene");
    }

    void testCreateRefusals() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Taken";
        options.location = temp.path;

        writeFile(temp.path / "Taken" / "keep.txt", "data");
        std::string error;
        CHECK(!Project::create(options, &error).has_value());
        CHECK(!error.empty());
        CHECK(fs::exists(temp.path / "Taken" / "keep.txt"));

        options.name = "Fresh";
        options.location = "relative/path";
        CHECK(!validateCreateOptions(options).empty());

        // A file standing where the folder should go.
        writeFile(temp.path / "afile", "x");
        options.location = temp.path / "afile";
        CHECK(!validateCreateOptions(options).empty());

        // An empty folder may be adopted.
        fs::create_directories(temp.path / "Adopted");
        options.name = "Adopted";
        options.location = temp.path;
        CHECK(Project::create(options, &error).has_value());

        options.name = "BadTemplate";
        options.templateId = "does-not-exist";
        CHECK(!Project::create(options, &error).has_value());
        CHECK(!fs::exists(temp.path / "BadTemplate"));
    }

    void testValidationReportsProblems() {
        TempDir temp;
        CHECK(!Project::validate(temp.path / "missing").isValid());
        CHECK(!Project::validate(temp.path).isValid());

        const fs::path broken = temp.path / "Broken";
        writeFile(broken / "Project.json", "{ not json");
        CHECK(!Project::validate(broken).isValid());

        const fs::path future = temp.path / "Future";
        fs::create_directories(future / "Assets");
        fs::create_directories(future / "ProjectSettings");
        writeFile(future / "Project.json", R"({"name":"F","formatVersion":999})");
        CHECK(!Project::validate(future).isValid());

        const fs::path noAssets = temp.path / "NoAssets";
        fs::create_directories(noAssets / "ProjectSettings");
        writeFile(noAssets / "Project.json", R"({"name":"N","formatVersion":1})");
        const auto validation = Project::validate(noAssets);
        CHECK(!validation.isValid());
        CHECK(validation.errors.size() == 1);
    }

    void testOpenRepairsRegenerableFolders() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Repair";
        options.location = temp.path;
        auto project = Project::create(options);
        CHECK(project.has_value());
        if (!project) return;

        fs::remove_all(project->libraryDir());
        CHECK(Project::validate(project->getRoot()).isValid());

        // Damaged settings fall back to defaults instead of blocking the project.
        writeFile(project->settingsDir() / "Graphics.json", "{{{");
        auto reopened = Project::open(project->getRoot());
        CHECK(reopened.has_value());
        CHECK(fs::is_directory(project->libraryDir()));
    }

    void testProjectIsRelocatable() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Mover";
        options.location = temp.path / "before";
        auto project = Project::create(options);
        CHECK(project.has_value());
        if (!project) return;

        fs::create_directories(temp.path / "after");
        fs::rename(project->getRoot(), temp.path / "after" / "Moved");

        auto moved = Project::open(temp.path / "after" / "Moved");
        CHECK(moved.has_value());
        if (!moved) return;

        AssetManager assets;
        assets.setProject(moved->getRoot(), moved->libraryDir());

        SceneGraph scene;
        std::string error;
        CHECK(SceneSerializer::loadFile(moved->getRoot() / fs::u8path(moved->startupScene), scene, assets, &error));
        CHECK(scene.getLight() != nullptr);
        CHECK(scene.getMainCamera() != nullptr);
    }

    void testGraphicsSettingsPersist() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Graphics";
        options.location = temp.path;
        auto project = Project::create(options);
        CHECK(project.has_value());
        if (!project) return;

        project->graphics = RenderSettings::preset(RenderMode::PS1);
        project->graphics.colorBits = 3;
        CHECK(project->saveSettings());

        auto reopened = Project::open(project->getRoot());
        CHECK(reopened.has_value());
        if (!reopened) return;
        CHECK(reopened->graphics.mode == RenderMode::PS1);
        CHECK(reopened->graphics.colorBits == 3);
        CHECK(reopened->graphics.internalHeight == 240);
        CHECK(reopened->graphics.textureFilter == TextureFilter::Nearest);
        CHECK(reopened->graphics.vertexPrecision == VertexPrecision::Medium);
        CHECK(reopened->graphics.dithering);
    }

    void testRecentProjects() {
        TempDir temp;
        const fs::path file = temp.path / "nested" / "recent.json";

        RecentProjects recent(file);
        recent.touch("/a/One", "One");
        recent.touch("/a/Two", "Two");
        recent.touch("/a/One", "One renamed");
        recent.setLastLocation("/a");
        CHECK(recent.getEntries().size() == 2);
        CHECK(recent.getEntries()[0].name == "One renamed");
        CHECK(recent.save());

        RecentProjects loaded(file);
        loaded.load();
        CHECK(loaded.getEntries().size() == 2);
        CHECK(loaded.getLastLocation() == "/a");

        loaded.remove("/a/One");
        CHECK(loaded.getEntries().size() == 1);

        for (int i = 0; i < 50; i++) loaded.touch("/p/" + std::to_string(i), "P");
        CHECK(loaded.getEntries().size() <= 30);

        writeFile(file, "garbage");
        RecentProjects damaged(file);
        damaged.load();
        CHECK(damaged.getEntries().empty());
    }

    void testAssetPaths() {
        TempDir temp;
        fs::create_directories(temp.path / "Project" / "Assets");
        AssetManager assets;
        assets.setProject(temp.path / "Project", temp.path / "Project" / "Library");

        CHECK(assets.resolve("Assets/a.png") == (temp.path / "Project/Assets/a.png").lexically_normal());
        CHECK(assets.resolve("../outside.png").empty());
        CHECK(assets.resolve("Assets/../../outside.png").empty());
        CHECK(assets.resolve("/etc/passwd").empty());
        CHECK(assets.resolve("").empty());

        CHECK(assets.toAssetPath(temp.path / "Project/Assets/Textures/a b.png") == "Assets/Textures/a b.png");
        CHECK(assets.toAssetPath(temp.path / "Other/a.png").empty());
        CHECK(assets.toAssetPath(temp.path / "Project").empty());
    }

    void testImport() {
        TempDir temp;
        ProjectCreateOptions options;
        options.name = "Importer";
        options.location = temp.path / "projects";
        auto project = Project::create(options);
        CHECK(project.has_value());
        if (!project) return;

        const fs::path outside = temp.path / "outside";
        fs::create_directories(outside);
        {
            std::ofstream png(outside / "stone.PNG", std::ios::binary);
            png.write(reinterpret_cast<const char*>(PNG_1X1), sizeof(PNG_1X1));
        }
        writeFile(outside / "fake.png", "this is not an image");
        writeFile(outside / "model.obj", CUBE_FACE_OBJ);
        writeFile(outside / "model.mtl", "newmtl x\n");
        writeFile(outside / "empty.obj", "# nothing\n");
        writeFile(outside / "notes.txt", "hello");

        // The Assets root sorts by kind.
        ImportResult texture = importAsset(*project, outside / "stone.PNG", project->assetsDir());
        CHECK(texture.ok());
        CHECK(texture.imported == project->assetsDir() / "Textures" / "stone.PNG");
        CHECK(fs::exists(outside / "stone.PNG"));

        // A second copy does not overwrite the first.
        ImportResult second = importAsset(*project, outside / "stone.PNG", project->assetsDir());
        CHECK(second.ok());
        CHECK(second.imported.filename() == "stone (1).PNG");

        // A chosen folder is respected.
        const fs::path custom = project->assetsDir() / "Models" / "Props";
        fs::create_directories(custom);
        ImportResult model = importAsset(*project, outside / "model.obj", custom);
        CHECK(model.ok());
        CHECK(model.imported == custom / "model.obj");
        CHECK(fs::exists(custom / "model.mtl"));

        CHECK(!importAsset(*project, outside / "fake.png", project->assetsDir()).ok());
        CHECK(!importAsset(*project, outside / "empty.obj", project->assetsDir()).ok());
        CHECK(!importAsset(*project, outside / "notes.txt", project->assetsDir()).ok());
        CHECK(!importAsset(*project, outside / "missing.png", project->assetsDir()).ok());
        CHECK(!importAsset(*project, model.imported, project->assetsDir()).ok());
    }

    void testObjLoader() {
        TempDir temp;
        const fs::path file = temp.path / "shape.obj";
        writeFile(file, CUBE_FACE_OBJ);

        MeshData mesh;
        std::string error;
        CHECK(ObjLoader::load(file, mesh, &error));
        // The quad becomes two triangles, plus one triangle from relative indices.
        CHECK(mesh.vertexCount() == 9);
        CHECK(mesh.vertices[5] == 1.0f);

        writeFile(file, "v 0 0 0\nf 1 2 3\n");
        CHECK(!ObjLoader::load(file, mesh, &error));
    }

    void testUserPaths() {
        CHECK(UserPaths::expand("~/Games") == UserPaths::home() / "Games");
        CHECK(UserPaths::expand("~") == UserPaths::home());
        CHECK(UserPaths::expand("/abs/path") == fs::path("/abs/path"));
        CHECK(UserPaths::abbreviate(UserPaths::home() / "Games") == "~/Games");
        CHECK(UserPaths::abbreviate("/opt/thing") == "/opt/thing");
    }
}

// The hierarchy finds what is in a ray's way, as a brute-force check does.
void testRayTrace() {
    // A floor of two triangles, a wall of two, and a lot of small triangles to make the tree deep.
    std::vector<glm::vec3> points;
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d) {
        for (const glm::vec3& p : {a, b, c, a, c, d}) points.push_back(p);
    };
    quad({-10, 0, -10}, {10, 0, -10}, {10, 0, 10}, {-10, 0, 10});
    quad({-1, 0, 5}, {1, 0, 5}, {1, 3, 5}, {-1, 3, 5});
    for (int i = 0; i < 400; i++) {
        const float x = -9.0f + (i % 20) * 0.9f, z = -9.0f + (i / 20) * 0.9f;
        points.push_back({x, 6.0f, z});
        points.push_back({x + 0.5f, 6.0f, z});
        points.push_back({x, 6.0f, z + 0.5f});
    }
    const std::shared_ptr<RayTraceData> data = buildRayTraceData(points);
    CHECK(data->triangleCount() == (int)points.size() / 3);
    CHECK(data->nodeCount() > 10);

    // Down onto the floor: hit. Up from the floor into the sky: clear. Through the wall: hit.
    CHECK(rayHitsAny(*data, {0, 2, 0}, {0, -1, 0}, 10.0f));
    CHECK(!rayHitsAny(*data, {0, 2, 0}, {0, -1, 0}, 1.0f));
    CHECK(!rayHitsAny(*data, {0.2f, 1, -6}, {0, 0, 1}, 5.0f));
    CHECK(rayHitsAny(*data, {0, 1, 0}, {0, 0, 1}, 10.0f));
    CHECK(!rayHitsAny(*data, {5, 1, 0}, {0, 0, 1}, 10.0f));

    // Against every triangle in turn, over many rays.
    unsigned seed = 7;
    auto random = [&]() { seed = seed * 1664525u + 1013904223u; return float(seed >> 8) / float(1 << 24); };
    int disagree = 0;
    for (int i = 0; i < 600; i++) {
        const glm::vec3 origin(random() * 20 - 10, random() * 8, random() * 20 - 10);
        const glm::vec3 direction = glm::normalize(glm::vec3(random() - 0.5f, random() - 0.5f, random() - 0.5f));
        const float far = 3.0f + random() * 20.0f;
        bool expected = false;
        for (size_t t = 0; t + 2 < points.size() && !expected; t += 3) {
            RayTraceData single = *buildRayTraceData({points[t], points[t + 1], points[t + 2]});
            expected = rayHitsAny(single, origin, direction, far);
        }
        if (expected != rayHitsAny(*data, origin, direction, far)) disagree++;
    }
    CHECK(disagree == 0);
    CHECK(buildRayTraceData({})->empty() && !rayHitsAny(*buildRayTraceData({}), {0, 0, 0}, {0, 1, 0}, 5.0f));
}

void testEditMesh() {
    EditMesh cube = EditMesh::cube(2.0f);
    CHECK(cube.valid() && cube.positions.size() == 8 && cube.faces.size() == 6);
    CHECK(cube.toMeshData().vertexCount() == 36);

    // Every normal points out of the cube: the faces wind counter-clockwise seen from outside.
    for (int f = 0; f < 6; f++) CHECK(glm::dot(cube.faceNormal(f), cube.faceCenter(f) - cube.center()) > 0.5f);
    for (const EditMesh& mesh : {EditMesh::plane(4.0f, 3), EditMesh::cylinder(12), EditMesh::cone(12), EditMesh::sphere(12, 6)}) {
        CHECK(mesh.valid() && !mesh.faces.empty());
        CHECK(mesh.toMeshData().vertexCount() > 0);
    }
    EditMesh plane = EditMesh::plane(4.0f, 3);
    CHECK(plane.faces.size() == 9 && glm::dot(plane.faceNormal(0), glm::vec3(0, 1, 0)) > 0.99f);
    EditMesh cylinder = EditMesh::cylinder(12);
    for (int f = 0; f < (int)cylinder.faces.size(); f++) CHECK(glm::dot(cylinder.faceNormal(f), cylinder.faceCenter(f) - cylinder.center()) > 0.0f);
    EditMesh sphere = EditMesh::sphere(12, 6);
    for (int f = 0; f < (int)sphere.faces.size(); f++) CHECK(glm::dot(sphere.faceNormal(f), sphere.faceCenter(f)) > 0.0f);
    EditMesh cone = EditMesh::cone(12);
    for (int f = 0; f < (int)cone.faces.size(); f++) CHECK(glm::dot(cone.faceNormal(f), cone.faceCenter(f) - cone.center()) > 0.0f);

    // A ray from above meets the top of the cube first.
    float distance = 0.0f;
    const int top = cube.pick(glm::vec3(0.1f, 10.0f, 0.2f), glm::vec3(0, -1, 0), distance);
    CHECK(top >= 0 && std::abs(distance - 9.0f) < 0.001f && cube.faceNormal(top).y > 0.99f);
    CHECK(cube.pick(glm::vec3(5.0f, 10.0f, 0.0f), glm::vec3(0, -1, 0), distance) == -1);

    // Extruding the top adds a wall for each of its edges, and the walls face outwards.
    EditMesh extruded = cube;
    const std::set<int> topFace{top};
    extruded.extrude(topFace, 1.0f);
    CHECK(extruded.valid() && extruded.faces.size() == 10 && extruded.positions.size() == 12);
    CHECK(std::abs(extruded.faceCenter(top).y - 2.0f) < 0.001f);
    for (int wall = 6; wall < 10; wall++) {
        const glm::vec3 sideways = extruded.faceCenter(wall) - extruded.faceCenter(top);
        CHECK(glm::dot(extruded.faceNormal(wall), glm::vec3(sideways.x, 0.0f, sideways.z)) > 0.1f);
    }

    // Faces are cut into pieces that still cover the same area.
    EditMesh cut = cube;
    const std::set<int> created = cut.subdivide(topFace);
    CHECK(cut.valid() && cut.faces.size() == 9 && created.size() == 4);
    CHECK(cut.positions.size() > cube.positions.size());

    EditMesh smaller = cube;
    smaller.deleteFaces(topFace);
    CHECK(smaller.valid() && smaller.faces.size() == 5 && smaller.positions.size() == 8);

    EditMesh flipped = cube;
    flipped.flip({0});
    CHECK(glm::dot(flipped.faceNormal(0), cube.faceNormal(0)) < -0.99f);

    EditMesh moved = cube;
    moved.translate(moved.verticesOf({top}), glm::vec3(0, 3, 0));
    CHECK(std::abs(moved.faceCenter(top).y - 4.0f) < 0.001f);
    CHECK(moved.facesUsing(moved.verticesOf({top})).count(top) == 1);

    // Triangles from a model join back into shared vertices, and a saved model reads back.
    EditMesh joined = EditMesh::fromMeshData(cube.toMeshData());
    CHECK(joined.faces.size() == 12 && joined.positions.size() == 8);

    TempDir temp;
    const fs::path file = temp.path / "cube.obj";
    std::string error;
    CHECK(cube.saveObj(file, &error));
    EditMesh loaded;
    CHECK(EditMesh::loadObj(file, loaded, &error) && loaded.valid());
    CHECK(loaded.toMeshData().vertexCount() == 36 && loaded.positions.size() == 8);

    EditMesh smooth = EditMesh::sphere(8, 4);
    smooth.smooth = true;
    const MeshData smoothed = smooth.toMeshData();
    CHECK(smoothed.vertexCount() > 0);
    CHECK(smooth.saveObj(temp.path / "sphere.obj", &error));

    EditMesh doubled = cube;
    doubled.positions.push_back(doubled.positions[0] + glm::vec3(0.0001f));
    doubled.faces[0].corners[0] = 8;
    CHECK(doubled.mergeByDistance(0.01f) == 1 && doubled.valid() && doubled.positions.size() == 8);

    cube.boxProjectUVs(2.0f);
    CHECK(cube.valid());
}

void testAudioEdit() {
    AudioClip clip;
    clip.channels = 2;
    clip.sampleRate = 1000;
    for (int i = 0; i < 100; i++) {
        clip.samples.push_back((int16_t)(i * 100));
        clip.samples.push_back((int16_t)(-i * 100));
    }
    CHECK(clip.frames() == 100);

    // Ranges outside the clip are brought inside it.
    const AudioEdit::Range outside = AudioEdit::clamp(clip, {90, 500});
    CHECK(outside.begin == 90 && outside.end == 100);

    AudioClip trimmed = clip;
    AudioEdit::trim(trimmed, {10, 30});
    CHECK(trimmed.frames() == 20 && trimmed.samples[0] == 1000 && trimmed.samples[1] == -1000);

    AudioClip cut = clip;
    AudioEdit::cut(cut, {10, 30});
    CHECK(cut.frames() == 80 && cut.samples[20] == 3000);

    AudioClip reversed = clip;
    AudioEdit::reverse(reversed, {0, 100});
    CHECK(reversed.samples[0] == 9900 && reversed.samples[198] == 0 && reversed.samples[199] == 0);

    AudioClip faded = clip;
    AudioEdit::fadeIn(faded, {0, 100});
    CHECK(faded.samples[0] == 0 && faded.samples[198] == 9900);
    CHECK(faded.samples[100] < clip.samples[100] && faded.samples[100] > 0);

    // Normalizing brings the loudest sample to the level, and gain in decibels is a factor.
    AudioClip loud = clip;
    AudioEdit::normalize(loud, {0, 100}, 0.5f);
    CHECK(std::abs(AudioEdit::peak(loud, {0, 100}) - 0.5f) < 0.01f);
    AudioClip doubled = clip;
    AudioEdit::gain(doubled, {0, 100}, 6.0206f);
    CHECK(std::abs(doubled.samples[2] - 2 * clip.samples[2]) <= 1);
    AudioClip clipped = clip;
    AudioEdit::gain(clipped, {0, 100}, 40.0f);
    CHECK(clipped.samples[198] == 32767 && clipped.samples[199] == -32768);

    AudioClip faster = clip;
    AudioEdit::resample(faster, 2.0f);
    CHECK(faster.frames() >= 49 && faster.frames() <= 51);

    AudioClip mono = clip;
    AudioEdit::toMono(mono);
    CHECK(mono.channels == 1 && mono.frames() == 100 && mono.samples[50] == 0);

    // A saved file reads back as what was written.
    TempDir temp;
    const std::string file = (temp.path / "edit.wav").string();
    std::string error;
    CHECK(saveWav(clip, file, &error));
    std::shared_ptr<AudioClip> loaded = loadWav(file, &error);
    CHECK(loaded && loaded->channels == 2 && loaded->sampleRate == 1000 && loaded->samples == clip.samples);

    AudioClip silent;
    CHECK(!saveWav(silent, file, &error));
}

int main() {
    testNameValidation();
    testCreateProducesExpectedLayout();
    testCreateRefusals();
    testValidationReportsProblems();
    testOpenRepairsRegenerableFolders();
    testProjectIsRelocatable();
    testGraphicsSettingsPersist();
    testRecentProjects();
    testAssetPaths();
    testImport();
    testObjLoader();
    testUserPaths();
    testAudioEdit();
    testEditMesh();
    testRayTrace();
    return testSummary("project_tests");
}
