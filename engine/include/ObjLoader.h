#pragma once
#include "MeshShape.h"
#include <filesystem>
#include <string>

// Reads triangles, quads and n-gons from a Wavefront .obj file into MeshData.
// Faces without normals get flat ones. Materials (.mtl) are ignored.
namespace ObjLoader {
    bool load(const std::filesystem::path& path, MeshData& mesh, std::string* error = nullptr);
}
