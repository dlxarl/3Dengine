include(FetchContent)

# GLFW
FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.3.8
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# stb
FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG d14d46f6b44456eb88beac58cba833a4135d4c89
)
FetchContent_MakeAvailable(stb)

# nlohmann/json: project metadata, settings and scene files
FetchContent_Declare(nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

if (ENGINE_BUILD_EDITOR)
    # Dear ImGui, docking branch. Its CMake-less sources are compiled by editor/CMakeLists.txt.
    FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.9b-docking
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(imgui)

    # ImGuizmo: transform manipulators for the Scene view. Pinned to a commit because the
    # project does not tag releases that track ImGui's. SOURCE_SUBDIR points nowhere on
    # purpose: only the source is needed, and editor/CMakeLists.txt compiles it.
    FetchContent_Declare(imguizmo
            GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
            GIT_TAG 105e4fde45e4e74aae8973d98750820ef7aca7e2
            SOURCE_SUBDIR do-not-add
    )
    FetchContent_MakeAvailable(imguizmo)
endif()
