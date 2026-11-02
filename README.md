# 3D Engine

**A real-time 3D rendering and physics sandbox built with C++, OpenGL, and GLFW.**

3D Engine is a C++17 graphics project focused on the fundamentals of real-time 3D rendering, scene management, object transformations, lighting, shadows, and basic collision-based physics. It includes a small scene framework, reusable geometric primitives, a first-person player controller, and a demo environment for experimenting with the engine.

The application currently launches the **DemoPhysics** scene by default.

## Features

### Rendering
- Real-time rendering with **OpenGL 3.3 Core Profile**.
- GLSL vertex and fragment shaders for object rendering and lighting.
- Perspective camera projection and view transformations.
- Textured and colored 3D geometry.
- Directional-light depth rendering and shadow mapping.
- HDR cubemap-based skybox support.
- Framebuffer-based post-processing pipeline.
- Motion blur post-processing shader.
- FPS counter and optional frame-rate limiting.

### Geometry and scene management
- Reusable shape base class and scene abstraction.
- Built-in primitives:
  - Cube
  - Plane
  - UV sphere
  - Cylinder
- Object position, rotation, scale, color, and texture controls.
- Separate scene update, render, and depth-rendering passes.
- Demo scenes for rendering and physics experiments.

### Physics and interaction
- Basic gravity and velocity integration.
- Collision checks between objects.
- Static and dynamic scene objects.
- First-person player movement.
- Walking, running, crouching, and jumping.
- Runtime object selection and transformations.
- Keyboard toggles for lighting, shadows, and post-processing.

> The physics implementation is a lightweight custom demonstration, not a full rigid-body physics engine. Collision handling and dynamics are intentionally limited.

## Controls

The active scene is `DemoPhysics`.

| Key | Action |
| --- | --- |
| `W` `A` `S` `D` | Move the player |
| `Left Shift` | Run |
| `C` | Crouch |
| `Space` | Jump |
| `Mouse` | Look around |
| `Mouse Wheel` | Adjust camera field of view |
| `N` | Select the next scene object |
| `Arrow keys` | Move the selected object horizontally |
| `+` / `-` | Move the selected object vertically |
| `R` | Rotate the selected object |
| `I` | Increase the selected object's scale |
| `O` | Decrease the selected object's scale |
| `L` | Toggle lighting |
| `K` | Toggle shadows |
| `M` | Toggle post-processing |
| `T` | Reload the active scene |
| `1` | Limit frame rate to 30 FPS |
| `2` | Limit frame rate to 70 FPS |
| `0` | Disable the frame-rate limit |
| `Esc` | Close the application |

The camera uses a captured mouse cursor. Use the window close button or `Esc` to exit.

## Tech stack

| Technology | Purpose |
| --- | --- |
| C++17 | Core application and engine implementation |
| OpenGL 3.3 | Graphics rendering API |
| GLSL | GPU shaders |
| GLFW 3.3.8 | Window creation, OpenGL context, and input |
| GLAD | OpenGL function loading |
| GLM 1.0.1 | Vector and matrix mathematics |
| stb_image | Image and texture loading |
| CMake 3.20+ | Build configuration |

GLFW, GLM, and stb are declared through CMake FetchContent. GLAD source and headers are included in the repository.

## Project structure

```text
3Dengine/
├── assets/
│   ├── hud/                 # HUD resources
│   ├── skybox/              # Skybox environment maps
│   └── textures/            # Surface textures
├── include/
│   ├── Engine.h             # Main engine interface
│   ├── Input.h              # Input handling
│   ├── Scene.h              # Scene abstraction
│   ├── Shape.h              # Base geometry abstraction
│   ├── Cube.h
│   ├── Plane.h
│   ├── Sphere.h
│   ├── Cylinder.h
│   ├── Player.h             # First-person player controller
│   ├── Shader.h
│   ├── Texture.h
│   ├── Skybox.h
│   ├── ShadowMap.h
│   ├── PostProcessor.h
│   └── glad/                # GLAD headers
├── src/
│   ├── main.cpp
│   ├── Engine.cpp
│   ├── Input.cpp
│   ├── Shape.cpp
│   ├── Cube.cpp
│   ├── Plane.cpp
│   ├── Sphere.cpp
│   ├── Cylinder.cpp
│   ├── Player.cpp
│   ├── Shader.cpp
│   ├── Texture.cpp
│   ├── Skybox.cpp
│   ├── PostProcessor.cpp
│   ├── glad.c
│   ├── *.vert              # Vertex shaders
│   ├── *.frag              # Fragment shaders
│   └── scenes/
│       ├── DemoScene.cpp
│       ├── DemoPhysics.cpp
│       └── include/
├── CMakeLists.txt
└── README.md
```

## Requirements

- CMake **3.20 or newer**
- A C++17-compatible compiler
- Git
- An OpenGL 3.3-compatible graphics driver
- GLFW build dependencies for your operating system
- Internet access during the first CMake configure step (to fetch GLFW, GLM, and stb)

The project has a Windows-specific OpenGL system-library link configuration. Other platforms may require additional OpenGL and window-system linker configuration.

## Build

Clone the repository:

```bash
git clone https://github.com/dlxarl/3Dengine.git
cd 3Dengine
```

Configure the project:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build --config Release
```

CMake downloads the declared GLFW, GLM, and stb dependencies during configuration through FetchContent.

## Run

The executable target is named `3D_Engine`.

On Linux and macOS, the executable is commonly located at:

```bash
./build/3D_Engine
```

On Windows with a multi-configuration generator:

```powershell
.\build\Release\3D_Engine.exe
```

Run the application from the repository root. The engine loads shader and asset files using project-relative paths, so launching it from a different working directory may prevent resources from being found.

## Architecture overview

The project is organized around a small set of engine-level abstractions:

- **Engine** initializes GLFW and GLAD, owns the main loop, updates input and the active scene, and coordinates rendering.
- **Scene** provides the interface for loading, updating, drawing, and rendering scene depth.
- **Shape** provides shared geometry state and transformation behavior for concrete primitives.
- **Shader** loads and configures GLSL programs and their uniforms.
- **Texture** loads image data and exposes it as an OpenGL texture.
- **Input** handles keyboard and mouse input, camera orientation, and field of view.
- **ShadowMap** manages the depth framebuffer used for shadow rendering.
- **PostProcessor** renders the scene into framebuffer textures and applies a screen-space effect.
- **Player** extends the cube shape with movement, gravity, crouching, jumping, and collision checks.

The engine loop follows the sequence:

1. Read frame timing and update input state.
2. Process global engine controls.
3. Update the active scene.
4. Render the depth pass used for shadows.
5. Render scene geometry with lighting.
6. Apply post-processing when enabled.
7. Swap the window buffers and poll events.

## Notes and limitations

- The default entry point selects `DemoPhysics`; `DemoScene` is an additional rendering example.
- Object collisions use simple shape-level checks and should not be treated as robust physical simulation.
- The current shadow map uses a high-resolution depth texture, which can consume significant GPU memory.
- The project uses a fixed project-root compile definition for shader paths and expects to be launched from the repository root for relative asset paths.
- Build and runtime behavior may vary across operating systems and graphics drivers.

## Contributing

Contributions and improvements are welcome. For larger changes, open an issue to discuss the design first. Please keep pull requests focused and include build or runtime verification steps when possible.

## License

No license file is currently present in this repository. Until a license is added, the source code remains subject to the default copyright rules; do not assume permission for unrestricted reuse or redistribution.
