#pragma once

#include "assets/ModelData.h"

#include <filesystem>

namespace atmos
{
    // Imports a .gltf/.glb file into engine space. glTF is right-handed with counter-clockwise
    // front faces; the loader mirrors Z (positions, normals, tangents, node matrices) and
    // reverses triangle winding. Materials keep absolute texture paths; textures embedded
    // in .glb buffers are not supported yet. Missing tangents are generated. Throws on failure.
    ModelData ImportGltfModel(const std::filesystem::path& path);
}
