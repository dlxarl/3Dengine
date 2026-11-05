# Aureon MCP server

Lets an AI assistant (Claude Code, Claude Desktop, any MCP client) work on an Aureon project and see the result.
No dependencies beyond Python 3.8+.

## Tools

| Tool | What it does |
|---|---|
| `project_info`, `list_assets` | Settings, graphics settings, scenes; files under `Assets` by kind and name |
| `scene_summary`, `find_objects`, `get_object` | Look into a scene without reading all of it (the town has 6600 objects) |
| `set_object`, `add_object`, `delete_object` | Edit a scene file: fields are merged (`{"light": {"intensity": 2}}`) |
| `set_graphics` | Edit `ProjectSettings/Graphics.json` (post-processing, ray traced shadows) |
| `read_file`, `write_file` | Text files of the project; writes are limited to `Assets` |
| `build`, `run_tests` | `cmake --build` and `ctest` on the engine |
| `render_shot` | Draws a scene to a PNG without opening the Editor, through the scene's camera or from `eye` to `target` |

Paths stay inside the project folder. Scene edits are written to the `.scene` file straight away, so keep the
project in git, and reopen the scene in the Editor to see them there.

## Setup

Build once (`cmake --build build`), which also makes `build/editor/aureon_shot`. Then add the server to your client.
For Claude Code, in the project's `.mcp.json`:

```json
{ "mcpServers": { "aureon": { "command": "python3", "args": ["tools/mcp/server.py"] } } }
```

Set `AUREON_BUILD_DIR` if the build is not in `build/`. `python3 tools/mcp/smoke_test.py` checks the whole path:
it talks to the server like a client, edits a throwaway scene and renders it.

## Not included

It does not drive the running Editor window (select, Play, undo). That needs a local control server inside the Editor.
