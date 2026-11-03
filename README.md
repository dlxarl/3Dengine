# 3D Engine

**A real-time 3D engine and desktop editor built with C++, OpenGL, GLFW and Dear ImGui.**

3D Engine is a C++17 graphics project focused on real-time rendering, scene management, object transformations, lighting, shadows and collision-based physics, with retro rendering inspired by the PlayStation 1 and PlayStation 2 as its direction.

The repository holds three things:

- **`engine/`**: the rendering engine, as a static library with no UI code.
- **`editor/`**: a desktop editor with a project launcher, dockable panels and an integrated viewport. See [The Editor](#the-editor).
- **`demo/`**: the original `DemoPhysics` and `DemoScene` sample scenes, as a standalone executable.

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
- Retro rendering: low internal resolution, per-vertex lighting, affine texture mapping, vertex snapping, texture filter control, color quantization and ordered dithering.

### Geometry and scene management
- Reusable shape base class and scene abstraction.
- Data-driven scenes with stable object ids, a parent/child hierarchy and JSON serialization.
- OBJ model loading, with a binary cache.
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
- Game rules driven by scene data: a player, pickups, a goal and patrolling hazards.
- Collision checks between objects.
- Static and dynamic scene objects.
- First-person player movement.
- Walking, running, crouching, and jumping.
- Runtime object selection and transformations.
- Keyboard toggles for lighting, shadows, and post-processing.

> The physics implementation is a lightweight custom demonstration, not a full rigid-body physics engine. Collision handling and dynamics are intentionally limited.

## Demo controls

The demo starts in `DemoPhysics`. Pass `--basic` to run `DemoScene` instead.

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
| nlohmann/json | Project, settings and scene files |
| Dear ImGui (docking) | Editor interface |
| ImGuizmo | Scene view transform gizmos |
| CMake 3.20+ | Build configuration |

Dependencies are declared through CMake FetchContent, in `cmake/Dependencies.cmake`. GLAD source and headers are included in the repository. The Editor's fonts come from the Dear ImGui source tree.

## Repository structure

```text
3Dengine/
├── cmake/                   # Dependencies and resource copying
├── engine/
│   ├── include/             # Public headers
│   ├── src/
│   └── shaders/             # Built-in shaders, copied next to each executable
├── editor/
│   ├── resources/shaders/   # Grid, outline and picking shaders
│   └── src/
│       ├── app/             # Windows, the launcher, theme, widgets
│       ├── core/            # Document, Play mode, console, dialogs, shared context
│       ├── panels/          # Hierarchy, Inspector, Project, Console, Render Settings, viewport
│       ├── platform/        # Native file dialogs, Finder, Trash
│       ├── project/         # Project format, recent projects, asset import
│       └── viewport/        # Editor camera, overlays, picking
├── demo/
│   ├── assets/              # Sample textures and skyboxes
│   ├── scenes/
│   └── main.cpp
└── tests/
```

## Requirements

- CMake **3.20 or newer**
- A C++17-compatible compiler
- Git
- An OpenGL 3.3-compatible graphics driver
- Internet access during the first CMake configure step, to fetch dependencies

On macOS, the Xcode Command Line Tools are enough. The Editor and the demo run on Apple Silicon.

## Build

```bash
git clone https://github.com/dlxarl/3Dengine.git
cd 3Dengine
cmake -S . -B build
cmake --build build -j
```

Set `-DENGINE_BUILD_EDITOR=OFF` or `-DENGINE_BUILD_DEMO=OFF` to leave a target out.

## Run

Open the Editor:

```bash
open build/editor/Editor.app
```

Or start it from a terminal, which also accepts a project to open directly:

```bash
./build/editor/Editor.app/Contents/MacOS/Editor --project ~/path/to/MyGame
```

Run the demo, from any directory:

```bash
./build/demo/3D_Engine
```

## The Editor

### Project launcher

The launcher opens first. It lists recent projects with their details, creates new projects from a template, and opens existing ones. Projects live wherever you choose, independent of the engine, and the Editor refuses locations inside its own bundle or source tree.

The list and the last used location are kept per user, in `~/Library/Application Support/3D Engine`.

### Project layout

```text
MyGame/
├── Assets/                  # Your files: Models, Textures, Materials, Scenes, Audio, Scripts
├── ProjectSettings/
│   ├── Project.json         # The startup scene
│   └── Graphics.json        # Render settings
├── Packages/
├── Library/                 # Generated data: mesh cache, thumbnails, panel layout
├── .gitignore               # Excludes Library
└── Project.json             # Name, id, engine version
```

Scenes refer to assets by project-relative path, so a project can be moved or renamed on disk. `Library` can be deleted at any time; it is rebuilt as needed.

### Panels

| Panel | What it does |
| --- | --- |
| Scene and Game | The engine's renderer, in a resizable viewport. Scene adds a grid, selection outline, gizmos and object markers. Game looks through the first camera in the scene. |
| Hierarchy | Selects, creates, renames, duplicates and deletes objects. Drag a row onto another to nest it, or between rows to reorder. |
| Inspector | Edits the live object: name, transform, color, texture, shadows, physics, and light or camera properties. |
| Project | Browses `Assets`, imports files and creates folders. Drag a texture onto an object or a slot, a model into the Scene view, or a scene to open it. |
| Console | Engine and Editor messages, filtered by level and text. |
| Render Settings | The renderer's settings, saved in the project, and the scene's background and skybox. |

Every control is connected: `tests/render_tests.cpp` checks that each render setting changes the rendered image.

| Render setting | Effect |
| --- | --- |
| Rendering Mode | Modern shades per pixel. PS1 lights per vertex and maps textures affinely. PS2 lights per vertex with perspective-correct textures. Selecting a mode also loads a matching set of the settings below. |
| Internal Resolution | Lines rendered before upscaling, with nearest-neighbor pixels. Width follows the viewport's shape. |
| Texture Filtering | Nearest, bilinear or trilinear, for all material textures. |
| Color Quantization | Bits of color for the final image. |
| Dithering | A 4x4 Bayer pattern in internal-resolution pixels. Needs quantization. |
| Vertex Precision | Snaps vertices to a screen grid, for the wobble of early 3D. |

Lighting, shadows and motion blur are also available there.

### Play mode

Play takes a snapshot of the scene, runs the engine's physics and game rules on it and switches to the Game view. Stop restores the snapshot, so nothing done during play reaches the saved scene. Objects with Gravity fall and collide with what has Collision.

### Making a game

Any scene with a **Player** object is playable, with no code. Create one from the **Create** menu, and give other objects a role in the Inspector's **Gameplay** section:

| Role | In play |
| --- | --- |
| Pickup | Collected by touching it, and spins until then. |
| Goal | Wins the game once every pickup is collected. |
| Hazard | Sends the player back to the start. Patrol makes it swing back and forth. |

The player also goes back to the start after falling 14 units below it, keeping what they collected. The Game view shows a counter and a timer, and the first camera in the scene follows the player.

| Key, in the Game view | Action |
| --- | --- |
| `W` `A` `S` `D` or arrows | Move, relative to the camera |
| `Space` | Jump |
| `Q` `E`, or right mouse button | Turn the camera |
| Mouse | Look around, in first person. Click the view to capture the mouse, `Esc` to free it |
| `V` | Switch between first and third person |
| `R` | Restart |

Tick **First person** on the camera in the Inspector to start a game in that view.

### Keyboard and mouse

| Input | Action |
| --- | --- |
| Hold right mouse button | Look around; add `W` `A` `S` `D` `Q` `E` to fly, and `Shift` to go faster |
| `Option` + left mouse button | Orbit |
| Middle mouse button, or `Option` + `Shift` + left mouse button | Pan |
| Scroll | Zoom |
| `W` `E` `R` | Move, rotate and scale tools |
| `F` | Focus the selection |
| `Cmd` + `S`, `Shift` + `Cmd` + `S` | Save, save as |
| `Cmd` + `P`, `Shift` + `Cmd` + `P` | Play or stop, pause |
| `Cmd` + `D`, `Delete`, `F2` | Duplicate, delete, rename |

Docked panels keep their arrangement per project. **Window > Reset layout** restores the default.

## Architecture overview

Dependencies point one way: `editor` uses `engine`, and `engine` knows nothing about the Editor or Dear ImGui.

- **Renderer** runs a frame: a shadow pass, the scene pass at the internal resolution, then a post-process pass into the output. The demo and the Editor share it.
- **Scene** is the interface the renderer draws. **SceneGraph** is its data-driven implementation, and the demo scenes are hand-written ones.
- **SceneObject** gives everything in a scene an id, a name, a transform and a place in the hierarchy. **Shape** adds geometry, and **LightObject** and **CameraObject** are objects too.
- **GameSession** applies the game rules to a scene from objects' roles, taking input as plain data, so it depends on neither the Editor nor a keyboard.
- **SceneSerializer** reads and writes scene files. **AssetManager** loads assets by project-relative path.
- **Engine** is the runtime loop for standalone executables. The Editor has its own, in **EditorApp**.
- **EditorSession** draws one frame of the whole UI and leaves the event loop to its caller, which is what lets the tests run the real UI with scripted input.
- **EditorContext** holds what panels share, and the operations they have in common, so that a menu item and a shortcut do the same thing.

Built-in resources are found next to the executable by **Resources**, never relative to the working directory.

## Tests

```bash
ctest --test-dir build
```

| Suite | Covers | Needs a display |
| --- | --- | --- |
| `project_tests` | Project creation and validation, relocation, recent projects, asset paths, import, the OBJ loader | No |
| `render_tests` | Each render setting on the real pipeline, picking, hierarchy, serialization, physics, game rules, Play mode | Yes |
| `ui_tests` | The real UI in a hidden window with scripted mouse and keyboard: selection, drag and drop, renaming, shortcuts, gizmos, asset drops, Play, game controls, the unsaved changes dialog | Yes |

Suites that need a display report as skipped when there is none.

## Status and limitations

### Not implemented

- **Undo and redo.** Deleting an object cannot be reversed, except by reopening the saved scene.
- **Scripts.** Games are limited to the built-in roles. The `Scripts` and `Audio` folders exist, but the engine does not run scripts or play sound.
- **Standalone games.** A game runs in the Editor's Game view; there is no export to an executable.
- **Materials.** Objects have a color and one albedo texture. Importing an OBJ also copies its `.mtl`, but the engine does not read it.
- **Model formats other than OBJ.**
- **More than one light.** The renderer uses the first light in the scene, and the Editor says so instead of offering a second.
- **Multiple selection, and renaming assets in the Project panel.**
- **Floating panels outside the main window.** Dear ImGui's multi-viewport mode is off.

### Limits worth knowing

- Physics is a lightweight demonstration. Collisions compare axis-aligned boxes and ignore rotation.
- Nothing watches the disk continuously: the Project panel rescans every two seconds while it has focus.
- The Scene view renders with the project's render settings, so retro settings apply there too. The grid and gizmos stay at full resolution.

### Verification

Built and run on an Apple Silicon Mac with macOS 26. The demo's output was compared with the previous version of the engine, patched only to open a window on macOS, and frame 300 matched on every pixel.

- **Tested only through the test suites:** dropping files on the window was exercised from the point where the Editor receives them, not with a real drag from Finder.
- **Partly tested:** the Import button opens the native file dialog, but choosing files in it was not exercised. The import behind it is covered by `project_tests`. The same applies to **Browse** and **Open project** in the launcher.
- **Not built:** Windows and Linux. The code keeps them in mind, with typed paths in place of native dialogs, but neither has been compiled.

## Contributing

Contributions and improvements are welcome. For larger changes, open an issue to discuss the design first. Please keep pull requests focused and include build or runtime verification steps when possible.

## License

No license file is currently present in this repository. Until a license is added, the source code remains subject to the default copyright rules; do not assume permission for unrestricted reuse or redistribution.
