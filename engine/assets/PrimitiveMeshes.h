#pragma once

#include "graphics/MeshData.h"

namespace atmos
{
    // Unit cube centered on the origin (extent -0.5..0.5), one submesh using material 0.
    MeshData CreateCubeMeshData();

    // Square on the XZ plane facing +Y, centered on the origin, one submesh using material 0.
    MeshData CreatePlaneMeshData(float size);
}
