#include "AssetImporter.h"
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
    return testSummary("project_tests");
}
