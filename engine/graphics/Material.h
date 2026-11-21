#pragma once

#include "graphics/MaterialData.h"

#include <array>
#include <cstdint>

namespace atmos
{
    // Mirrors MaterialData in shaders/Scene/SceneData.hlsli. Texture fields are bindless SRV indices.
    struct MaterialGpuData
    {
        DirectX::XMFLOAT4 baseColorFactor;
        DirectX::XMFLOAT3 emissiveFactor;
        float alphaCutoff;
        float metallicFactor;
        float roughnessFactor;
        uint32_t textures[static_cast<size_t>(MaterialTextureSlot::Count)];
        uint32_t padding;
    };

    static_assert(sizeof(MaterialGpuData) == 64);

    // Parameters plus bindless texture indices (fallbacks until textures are resident).
    struct Material
    {
        MaterialData data;
        std::array<uint32_t, static_cast<size_t>(MaterialTextureSlot::Count)> textureIndices{};

        MaterialGpuData ToGpuData() const
        {
            MaterialGpuData gpu{};
            gpu.baseColorFactor = data.baseColorFactor;
            gpu.emissiveFactor = data.emissiveFactor;
            gpu.alphaCutoff = data.alphaCutoff;
            gpu.metallicFactor = data.metallicFactor;
            gpu.roughnessFactor = data.roughnessFactor;

            for (size_t i = 0; i < textureIndices.size(); ++i)
                gpu.textures[i] = textureIndices[i];

            return gpu;
        }
    };
}
