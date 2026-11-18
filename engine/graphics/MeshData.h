#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <string>
#include <vector>

namespace atmos
{
    // Mirrors the attribute stream layout read in Mesh.hlsl (36 bytes per vertex).
    struct VertexAttributes
    {
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT4 tangent;  // xyz tangent, w bitangent sign: B = cross(N, T) * w
        DirectX::XMFLOAT2 uv;
    };

    static_assert(sizeof(VertexAttributes) == 36);

    // A range of the index buffer drawn with one material.
    struct Submesh
    {
        uint32_t indexOffset = 0;
        uint32_t indexCount = 0;
        uint32_t materialIndex = 0;  // Into the owning model's material list.
    };

    // CPU-side geometry in engine space (left-handed, +Y up, clockwise front faces).
    // Loaders and procedural generators produce it; the asset manager uploads it.
    struct MeshData
    {
        std::string name;
        std::vector<DirectX::XMFLOAT3> positions;
        std::vector<VertexAttributes> attributes;  // Same count as positions.
        std::vector<uint32_t> indices;
        std::vector<Submesh> submeshes;
    };
}
