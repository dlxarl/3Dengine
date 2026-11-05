#!/usr/bin/env python3
"""Talks to the server as an MCP client would, against a throwaway project made from a template."""
import base64, json, shutil, subprocess, sys, tempfile
from pathlib import Path

SERVER = Path(__file__).with_name("server.py")
proc = subprocess.Popen([sys.executable, str(SERVER)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
counter = 0

def call(method, params=None):
    global counter
    counter += 1
    proc.stdin.write(json.dumps({"jsonrpc": "2.0", "id": counter, "method": method, "params": params or {}}) + "\n")
    proc.stdin.flush()
    return json.loads(proc.stdout.readline())["result"]

def tool(tool_name, **arguments):
    result = call("tools/call", {"name": tool_name, "arguments": arguments})
    return result, (json.loads(result["content"][0]["text"]) if result["content"][0]["type"] == "text" and not result.get("isError") else None)

project = Path(tempfile.mkdtemp()) / "Demo"
(project / "Assets/Scenes").mkdir(parents=True)
(project / "ProjectSettings").mkdir()
(project / "Project.json").write_text(json.dumps({"name": "Demo", "formatVersion": 1, "id": "0" * 32, "engineVersion": "1.0.0", "template": "empty", "createdAt": "2026-01-01T00:00:00Z"}))
(project / "ProjectSettings/Project.json").write_text(json.dumps({"startupScene": "Assets/Scenes/Main.scene"}))
(project / "Assets/Scenes/Main.scene").write_text(json.dumps({"version": 1, "environment": {}, "nextObjectId": 3, "objects": [
    {"id": 1, "name": "Main Camera", "type": "camera", "parent": 0, "position": [0, 3, 8], "rotation": [-15, 0, 0], "scale": [1, 1, 1], "visible": True},
    {"id": 2, "name": "Sun", "type": "light", "parent": 0, "position": [0, 10, 0], "rotation": [-50, 30, 0], "scale": [1, 1, 1], "visible": True,
     "light": {"type": "directional", "color": [1, 1, 1], "intensity": 1}}]}))

assert call("initialize", {"protocolVersion": "2024-11-05"})["serverInfo"]["name"] == "aureon"
names = [t["name"] for t in call("tools/list")["tools"]]
assert {"render_shot", "set_object", "add_object", "build"} <= set(names), names

_, summary = tool("scene_summary", project=str(project))
assert summary["objects"] == 2 and summary["byType"]["light"] == 1
_, added = tool("add_object", project=str(project), type="cube", name="Block", fields={"position": [0, 1, 0], "scale": [2, 2, 2]})
assert added["id"] == 3
_, floor = tool("add_object", project=str(project), type="plane", name="Floor", fields={"scale": [20, 1, 20]})
_, lamp = tool("add_object", project=str(project), type="light", name="Lamp", fields={"position": [3, 4, 0], "light": {"type": "point", "range": 20, "intensity": 3}})
_, changed = tool("set_object", project=str(project), name="Lamp", fields={"light": {"intensity": 5}})
assert changed["light"]["intensity"] == 5 and changed["light"]["range"] == 20
_, found = tool("find_objects", project=str(project), name_contains="l")
assert found["count"] >= 2
err, _ = tool("set_object", project=str(project), id=3, fields={"type": "sphere"})
assert err["isError"]
err, _ = tool("get_object", project="/nonexistent")
assert err["isError"]
err, _ = tool("write_file", project=str(project), path="../evil.cpp", text="x")
assert err["isError"]
_, wrote = tool("write_file", project=str(project), path="Assets/Scripts/Hello.cpp", text="// hi\n")
assert wrote["written"].endswith("Hello.cpp")

shot, _ = tool("render_shot", project=str(project), eye=[0, 5, 12], target=[0, 1, 0], width=640, height=360, frames=1)
if shot.get("isError"):
    print("render_shot unavailable:", shot["content"][0]["text"]); sys.exit(1)
png = base64.b64decode(shot["content"][0]["data"])
assert png[:8] == b"\x89PNG\r\n\x1a\n" and len(png) > 2000
Path("/tmp/aureon_mcp_smoke.png").write_bytes(png)

_, deleted = tool("delete_object", project=str(project), id=3)
assert deleted["deleted"] == [3]
_, summary = tool("scene_summary", project=str(project))
assert summary["objects"] == 4
shutil.rmtree(project.parent)
proc.stdin.close()
print("mcp smoke test passed; image bytes:", len(png))
