#pragma once

#include "assets/ModelData.h"

#include <filesystem>

namespace atmos
{
    // Imports into engine space: mirrors Z, negates tangent W, reverses winding, generates missing
    // tangents. Embedded (data URI/.glb) images are not supported yet. Throws on failure.
    ModelData ImportGltfModel(const std::filesystem::path& path);
}
