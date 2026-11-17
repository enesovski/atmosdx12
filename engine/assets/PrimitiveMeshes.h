#pragma once

#include "graphics/MeshData.h"

namespace atmos
{
    // Unit cube centered on the origin (extent -0.5..0.5).
    MeshData CreateCubeMeshData(const DirectX::XMFLOAT4& color);

    // Square on the XZ plane facing +Y, centered on the origin.
    MeshData CreatePlaneMeshData(float size, const DirectX::XMFLOAT4& color);
}
