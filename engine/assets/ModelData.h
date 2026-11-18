#pragma once

#include "graphics/MaterialData.h"
#include "graphics/MeshData.h"

#include <DirectXMath.h>

#include <cstdint>
#include <vector>

namespace atmos
{
    // One placement of a mesh inside the model (node transform, model space).
    struct ModelInstance
    {
        uint32_t meshIndex = 0;
        DirectX::XMFLOAT4X4 transform{};
    };

    // Everything an imported model contains, CPU-side and in engine space.
    // Submesh::materialIndex refers into `materials`.
    struct ModelData
    {
        std::vector<MeshData> meshes;
        std::vector<MaterialData> materials;
        std::vector<ModelInstance> instances;
    };
}
