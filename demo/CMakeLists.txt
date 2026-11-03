add_executable(3D_Engine
        main.cpp
        scenes/DemoScene.cpp
        scenes/DemoPhysics.cpp
)

target_compile_definitions(3D_Engine PRIVATE
        DEMO_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

target_include_directories(3D_Engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/scenes/include
)

target_link_libraries(3D_Engine PRIVATE engine)

engine_add_resources(3D_Engine)
