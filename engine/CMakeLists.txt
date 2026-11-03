add_library(engine STATIC
        src/glad.c

        src/Log.cpp
        src/Resources.cpp
        src/GlWindow.cpp

        src/Engine.cpp
        src/Input.cpp
        src/Shader.cpp
        src/Texture.cpp
        src/stb_image_impl.cpp

        src/SceneObject.cpp
        src/Shape.cpp
        src/Cube.cpp
        src/Plane.cpp
        src/Sphere.cpp
        src/Cylinder.cpp
        src/MeshShape.cpp
        src/ObjectFactory.cpp

        src/Skybox.cpp
        src/PostProcessor.cpp
        src/Player.cpp
        src/Physics.cpp

        src/RenderSettings.cpp
        src/RenderTarget.cpp
        src/Renderer.cpp

        src/SceneGraph.cpp
        src/SceneSerializer.cpp
        src/AssetManager.cpp
        src/ObjLoader.cpp
)

target_include_directories(engine
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${glm_SOURCE_DIR}
        PRIVATE
            ${stb_SOURCE_DIR}
)

target_link_libraries(engine
        PUBLIC glfw
        PRIVATE nlohmann_json::nlohmann_json
)

if (WIN32)
    target_link_libraries(engine PUBLIC opengl32 gdi32 user32 winmm shell32)
endif()

set(ENGINE_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders" CACHE INTERNAL "Built-in engine shaders")
