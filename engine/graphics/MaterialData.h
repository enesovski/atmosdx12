#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace atmos
{
    enum class AlphaMode : uint32_t
    {
        Opaque,
        Mask,   // Alpha test against alphaCutoff.
        Blend,  // Alpha-blended in the sorted transparent pass.
    };

    enum class MaterialTextureSlot : uint32_t
    {
        BaseColor,          // sRGB
        Normal,             // tangent space, BC5 (xy)
        MetallicRoughness,  // linear: G roughness, B metallic (glTF)
        Occlusion,          // linear: R
        Emissive,           // sRGB
        Count,
    };

    // glTF metallic-roughness material. Texture paths are absolute; empty means unused.
    struct MaterialData
    {
        std::string name;
        DirectX::XMFLOAT4 baseColorFactor{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 emissiveFactor{ 0.0f, 0.0f, 0.0f };
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        float alphaCutoff = 0.5f;
        AlphaMode alphaMode = AlphaMode::Opaque;
        bool doubleSided = false;
        std::filesystem::path textures[static_cast<size_t>(MaterialTextureSlot::Count)];
    };
}
