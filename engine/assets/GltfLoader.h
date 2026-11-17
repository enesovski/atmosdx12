#pragma once

#include "graphics/MeshData.h"

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <vector>

namespace atmos
{
    // One placement of a mesh in the model.
    struct ModelInstance
    {
        uint32_t meshIndex = 0;
        DirectX::XMFLOAT4X4 world{};
    };

    struct ModelData
    {
        std::vector<MeshData> meshes;
        std::vector<ModelInstance> instances;
    };

    // Loads a .gltf/.glb file into engine space. glTF is right-handed with counter-clockwise
    // front faces; the loader mirrors Z (positions, normals, node matrices) and reverses
    // triangle winding. Phase 1 reads geometry, the node hierarchy and baseColorFactor only.
    // Throws on failure.
    ModelData LoadGltfModel(const std::filesystem::path& path);
}
