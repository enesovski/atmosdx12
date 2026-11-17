#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <cstdint>

namespace atmos
{
    // Root parameters of the engine-wide root signature, mirrored in shaders/Common.hlsli.
    // Textures and buffers are not bound here: shaders index ResourceDescriptorHeap directly.
    enum class RootSlot : uint32_t
    {
        RootConstants = 0,  // b0: small per-pass/per-draw data, e.g. bindless indices
        ViewConstants = 1,  // b1: per-view CBV
        PassConstants = 2,  // b2: per-pass CBV for data that exceeds root constants
    };

    inline constexpr uint32_t RootConstantCount = 16;

    ComPtr<ID3D12RootSignature> CreateBindlessRootSignature(ID3D12Device* device);
}
