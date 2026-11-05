#!/usr/bin/env python3
"""Aureon MCP server: lets an AI assistant read and edit Aureon projects and see the result.

Speaks the Model Context Protocol over stdio, with no dependencies beyond Python 3.8+.
Scenes are edited as the JSON files they are (Assets/Scenes/*.scene); render_shot runs the
`aureon_shot` program built next to the Editor, which draws a scene without a window.

Every path is taken relative to a project folder and kept inside it.
"""
import base64
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BUILD = Path(os.environ.get("AUREON_BUILD_DIR", REPO / "build"))
SHOT = BUILD / "editor" / "aureon_shot"
OBJECT_TYPES = ["empty", "cube", "plane", "sphere", "cylinder", "mesh", "light", "camera", "player",
                "particles", "fire", "water", "audioSource", "ui", "terrain"]
TEXT_SUFFIXES = {".cpp", ".h", ".hpp", ".txt", ".md", ".json", ".frag", ".vert", ".glsl", ".mat", ".mtl"}


class ToolError(Exception):
    pass


def project_dir(arguments):
    root = Path(os.path.expanduser(arguments.get("project", ""))).resolve()
    if not (root / "Project.json").exists() and not (root / "ProjectSettings" / "Project.json").exists():
        raise ToolError(f"{root} is not an Aureon project (no Project.json).")
    return root


def inside(root, relative):
    path = (root / relative).resolve()
    if root != path and root not in path.parents:
        raise ToolError(f"{relative} is outside the project.")
    return path


def scene_path(root, arguments):
    relative = arguments.get("scene")
    if not relative:
        settings = root / "ProjectSettings" / "Project.json"
        relative = json.loads(settings.read_text()).get("startupScene", "") if settings.exists() else ""
    if not relative:
        raise ToolError("Say which scene: the project has no startup scene.")
    path = inside(root, relative)
    if not path.exists():
        raise ToolError(f"No such scene: {relative}")
    return path


def load_scene(path):
    return json.loads(path.read_text())


def save_scene(path, scene):
    temp = path.with_suffix(".scene.tmp")
    temp.write_text(json.dumps(scene, indent=2))
    temp.replace(path)


def find_object(scene, arguments):
    objects = scene["objects"]
    if "id" in arguments:
        for o in objects:
            if o["id"] == arguments["id"]:
                return o
        raise ToolError(f"No object with id {arguments['id']}.")
    if "name" in arguments:
        matches = [o for o in objects if o.get("name") == arguments["name"]]
        if len(matches) == 1:
            return matches[0]
        raise ToolError(f"{len(matches)} objects are named {arguments['name']!r}; use an id.")
    raise ToolError("Give an id or a name.")


def merge(target, changes):
    for key, value in changes.items():
        if isinstance(value, dict) and isinstance(target.get(key), dict):
            merge(target[key], value)
        else:
            target[key] = value


def brief(o):
    return {"id": o["id"], "name": o.get("name"), "type": o.get("type"), "parent": o.get("parent", 0),
            "position": o.get("position")}


# ---- tools -----------------------------------------------------------------------------

def t_project_info(a):
    root = project_dir(a)
    settings = root / "ProjectSettings"
    info = {"root": str(root)}
    for name in ("Project.json", "Graphics.json"):
        for place in (root / name, settings / name):
            if place.exists():
                info[name] = json.loads(place.read_text())
    info["scenes"] = sorted(str(p.relative_to(root)) for p in (root / "Assets").rglob("*.scene"))
    return info


def t_list_assets(a):
    root = project_dir(a)
    kinds = {"model": {".obj"}, "texture": {".png", ".jpg", ".jpeg", ".tga", ".bmp"}, "audio": {".wav"},
             "script": {".cpp", ".h"}, "scene": {".scene"}, "font": {".ttf", ".otf"}}
    wanted = kinds.get(a.get("kind", ""), None)
    needle = a.get("contains", "").lower()
    found = []
    for p in sorted((root / "Assets").rglob("*")):
        if p.is_file() and (wanted is None or p.suffix.lower() in wanted) and needle in p.name.lower():
            found.append(str(p.relative_to(root)))
    limit = int(a.get("limit", 200))
    return {"count": len(found), "assets": found[:limit], "truncated": len(found) > limit}


def t_scene_summary(a):
    scene = load_scene(scene_path(project_dir(a), a))
    counts = {}
    for o in scene["objects"]:
        counts[o["type"]] = counts.get(o["type"], 0) + 1
    roots = [brief(o) for o in scene["objects"] if not o.get("parent")][:40]
    return {"objects": len(scene["objects"]), "byType": counts, "environment": scene.get("environment"),
            "hasGame": bool(scene.get("game")), "firstRoots": roots}


def t_find_objects(a):
    scene = load_scene(scene_path(project_dir(a), a))
    needle = a.get("name_contains", "").lower()
    kind = a.get("type")
    found = [o for o in scene["objects"] if needle in o.get("name", "").lower() and (not kind or o["type"] == kind)]
    limit = int(a.get("limit", 50))
    return {"count": len(found), "objects": [brief(o) for o in found[:limit]], "truncated": len(found) > limit}


def t_get_object(a):
    return find_object(load_scene(scene_path(project_dir(a), a)), a)


def t_set_object(a):
    path = scene_path(project_dir(a), a)
    scene = load_scene(path)
    obj = find_object(scene, a)
    changes = a.get("fields")
    if not isinstance(changes, dict) or not changes:
        raise ToolError("fields must be an object, such as {\"position\": [0, 1, 0]} or {\"light\": {\"intensity\": 2}}.")
    for forbidden in ("id", "type"):
        if forbidden in changes:
            raise ToolError(f"{forbidden} cannot be changed.")
    merge(obj, changes)
    save_scene(path, scene)
    return obj


def t_add_object(a):
    path = scene_path(project_dir(a), a)
    scene = load_scene(path)
    kind = a.get("type")
    if kind not in OBJECT_TYPES:
        raise ToolError(f"type must be one of {OBJECT_TYPES}.")
    new_id = scene.get("nextObjectId", max([o["id"] for o in scene["objects"]] + [0]) + 1)
    obj = {"id": new_id, "name": a.get("name", kind.capitalize()), "type": kind, "parent": a.get("parent", 0),
           "visible": True, "position": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]}
    if a.get("parent") and not any(o["id"] == a["parent"] for o in scene["objects"]):
        raise ToolError(f"No parent object with id {a['parent']}.")
    merge(obj, a.get("fields", {}))
    scene["objects"].append(obj)
    scene["nextObjectId"] = new_id + 1
    save_scene(path, scene)
    return obj


def t_delete_object(a):
    path = scene_path(project_dir(a), a)
    scene = load_scene(path)
    obj = find_object(scene, a)
    doomed = {obj["id"]}
    grew = True
    while grew:
        grew = False
        for o in scene["objects"]:
            if o.get("parent") in doomed and o["id"] not in doomed:
                doomed.add(o["id"])
                grew = True
    scene["objects"] = [o for o in scene["objects"] if o["id"] not in doomed]
    save_scene(path, scene)
    return {"deleted": sorted(doomed)}


def t_set_graphics(a):
    root = project_dir(a)
    path = inside(root, "ProjectSettings/Graphics.json")
    graphics = json.loads(path.read_text()) if path.exists() else {}
    changes = a.get("fields")
    if not isinstance(changes, dict) or not changes:
        raise ToolError("fields must be an object, such as {\"rayTracedShadows\": true, \"post\": {\"bloom\": true}}.")
    merge(graphics, changes)
    path.write_text(json.dumps(graphics, indent=4))
    return graphics


def t_read_file(a):
    root = project_dir(a)
    path = inside(root, a["path"])
    if path.suffix.lower() not in TEXT_SUFFIXES:
        raise ToolError("Only text files can be read.")
    text = path.read_text()
    return {"path": a["path"], "bytes": len(text), "text": text[: int(a.get("max_chars", 20000))]}


def t_write_file(a):
    root = project_dir(a)
    path = inside(root / "Assets", a["path"].removeprefix("Assets/")) if not a["path"].startswith("..") else None
    if path is None or path.suffix.lower() not in TEXT_SUFFIXES:
        raise ToolError("Writes go under Assets, to text files (.cpp, .h, .json, ...).")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(a["text"])
    return {"written": str(path.relative_to(root)), "bytes": len(a["text"])}


def run(command, cwd, timeout):
    done = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)
    return done.returncode, (done.stdout + done.stderr)


def t_build(a):
    code, out = run(["cmake", "--build", str(BUILD), "-j8"], REPO, int(a.get("timeout", 900)))
    errors = [l for l in out.splitlines() if "error" in l.lower()]
    return {"ok": code == 0, "errors": errors[:30], "tail": out.splitlines()[-5:]}


def t_run_tests(a):
    code, out = run(["ctest", "--test-dir", str(BUILD), "--output-on-failure"], REPO, int(a.get("timeout", 900)))
    failed = [l for l in out.splitlines() if "FAILED" in l or "Failed" in l]
    return {"ok": code == 0, "failures": failed[:40], "summary": out.splitlines()[-4:]}


def t_render_shot(a):
    root = project_dir(a)
    if not SHOT.exists():
        raise ToolError(f"{SHOT} is missing. Run the build tool first.")
    out = Path(tempfile.mkdtemp()) / "shot.png"
    command = [str(SHOT), "--project", str(root), "--out", str(out), "--width", str(a.get("width", 1280)),
               "--height", str(a.get("height", 720)), "--frames", str(a.get("frames", 30))]
    for flag, key in (("--scene", "scene"), ("--hour", "hour"), ("--fov", "fov")):
        if key in a:
            command += [flag, str(a[key])]
    for flag, key in (("--eye", "eye"), ("--target", "target")):
        if key in a:
            command += [flag, ",".join(str(v) for v in a[key])]
    env = dict(os.environ, ENGINE_RESOURCE_DIR=str(BUILD / "editor" / "resources"))
    done = subprocess.run(command, capture_output=True, text=True, timeout=120, env=env)
    if done.returncode != 0 or not out.exists():
        raise ToolError("aureon_shot failed: " + (done.stderr or done.stdout)[-800:])
    return ("image", out.read_bytes())


def schema(properties, required=()):
    return {"type": "object", "properties": properties, "required": list(required)}


PROJECT = {"project": {"type": "string", "description": "Path to the project folder."}}
SCENE = {"scene": {"type": "string", "description": "Project-relative scene, such as Assets/Scenes/Main.scene. Default: the startup scene."}}
ID = {"id": {"type": "integer"}, "name": {"type": "string", "description": "Used when there is no id; must be unique."}}
EDIT_NOTE = (" Fields use the scene file's own keys: position, rotation (degrees), scale, visible, and a block named for the"
             " object's kind: shape, light, camera, ui, audio, water, fire, particles, terrain."
             " Example: {\"light\": {\"type\": \"spot\", \"intensity\": 2.5}}.")

TOOLS = {
    "project_info": (t_project_info, "Project settings, graphics settings and the list of scenes.", schema(PROJECT, ["project"])),
    "list_assets": (t_list_assets, "Files under Assets, filtered by kind (model, texture, audio, script, scene, font) and name.",
                    schema({**PROJECT, "kind": {"type": "string"}, "contains": {"type": "string"}, "limit": {"type": "integer"}}, ["project"])),
    "scene_summary": (t_scene_summary, "Object counts by type, the environment, and the top-level objects of a scene.",
                      schema({**PROJECT, **SCENE}, ["project"])),
    "find_objects": (t_find_objects, "Objects whose name contains a text and/or that have a type.",
                     schema({**PROJECT, **SCENE, "name_contains": {"type": "string"}, "type": {"type": "string", "enum": OBJECT_TYPES}, "limit": {"type": "integer"}}, ["project"])),
    "get_object": (t_get_object, "The full JSON of one object.", schema({**PROJECT, **SCENE, **ID}, ["project"])),
    "set_object": (t_set_object, "Changes fields of one object, merged into what it has." + EDIT_NOTE,
                   schema({**PROJECT, **SCENE, **ID, "fields": {"type": "object"}}, ["project", "fields"])),
    "add_object": (t_add_object, "Adds an object. type is one of " + ", ".join(OBJECT_TYPES) + "." + EDIT_NOTE,
                   schema({**PROJECT, **SCENE, "type": {"type": "string", "enum": OBJECT_TYPES}, "name": {"type": "string"},
                           "parent": {"type": "integer"}, "fields": {"type": "object"}}, ["project", "type"])),
    "delete_object": (t_delete_object, "Deletes an object and everything under it.", schema({**PROJECT, **SCENE, **ID}, ["project"])),
    "set_graphics": (t_set_graphics, "Changes ProjectSettings/Graphics.json (post-processing, ray traced shadows, retro look), merged.",
                     schema({**PROJECT, "fields": {"type": "object"}}, ["project", "fields"])),
    "read_file": (t_read_file, "Reads a text file of the project (scripts, shaders, data).",
                  schema({**PROJECT, "path": {"type": "string"}, "max_chars": {"type": "integer"}}, ["project", "path"])),
    "write_file": (t_write_file, "Writes a text file under Assets, such as a C++ script.",
                   schema({**PROJECT, "path": {"type": "string"}, "text": {"type": "string"}}, ["project", "path", "text"])),
    "build": (t_build, "Builds the engine and Editor (cmake --build).", schema({"timeout": {"type": "integer"}})),
    "run_tests": (t_run_tests, "Runs the engine's tests.", schema({"timeout": {"type": "integer"}})),
    "render_shot": (t_render_shot, "Renders a scene to an image without opening the Editor. By default through the scene's Main Camera; "
                    "give eye and target ([x, y, z]) to look from anywhere. hour sets the time of day.",
                    schema({**PROJECT, **SCENE, "eye": {"type": "array", "items": {"type": "number"}}, "target": {"type": "array", "items": {"type": "number"}},
                            "fov": {"type": "number"}, "hour": {"type": "number"}, "width": {"type": "integer"}, "height": {"type": "integer"},
                            "frames": {"type": "integer"}}, ["project"])),
}


# ---- protocol --------------------------------------------------------------------------

def reply(message_id, result=None, error=None):
    message = {"jsonrpc": "2.0", "id": message_id}
    if error is not None:
        message["error"] = error
    else:
        message["result"] = result
    sys.stdout.write(json.dumps(message) + "\n")
    sys.stdout.flush()


def call_tool(name, arguments):
    if name not in TOOLS:
        raise ToolError(f"Unknown tool {name}.")
    result = TOOLS[name][0](arguments or {})
    if isinstance(result, tuple) and result[0] == "image":
        return {"content": [{"type": "image", "data": base64.b64encode(result[1]).decode(), "mimeType": "image/png"}]}
    return {"content": [{"type": "text", "text": json.dumps(result, indent=2)}]}


def handle(message):
    method, message_id = message.get("method"), message.get("id")
    if method == "initialize":
        reply(message_id, {"protocolVersion": message["params"].get("protocolVersion", "2024-11-05"),
                           "capabilities": {"tools": {}}, "serverInfo": {"name": "aureon", "version": "0.1.0"}})
    elif method == "tools/list":
        reply(message_id, {"tools": [{"name": n, "description": d, "inputSchema": s} for n, (_, d, s) in TOOLS.items()]})
    elif method == "tools/call":
        try:
            reply(message_id, call_tool(message["params"]["name"], message["params"].get("arguments")))
        except (ToolError, KeyError, subprocess.TimeoutExpired, json.JSONDecodeError) as e:
            reply(message_id, {"content": [{"type": "text", "text": f"Error: {e}"}], "isError": True})
    elif method == "ping":
        reply(message_id, {})
    elif message_id is not None:
        reply(message_id, error={"code": -32601, "message": f"Unknown method {method}"})


def main():
    for line in sys.stdin:
        line = line.strip()
        if line:
            handle(json.loads(line))


if __name__ == "__main__":
    main()
