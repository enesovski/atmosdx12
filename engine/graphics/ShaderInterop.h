#pragma once

#include <DirectXMath.h>

#include <cstddef>
#include <cstdint>

namespace atmos
{
    // Mirrors ViewConstants in shaders/Common.hlsli; change both together.
    // Matrices are row-major and multiply row vectors: mul(position, viewProjection).
    struct ViewConstants
    {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 projection;
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4X4 inverseViewProjection;
        DirectX::XMFLOAT3 cameraPosition;
        uint32_t frameNumber;
        DirectX::XMFLOAT2 viewportSize;
        DirectX::XMFLOAT2 inverseViewportSize;
    };

    static_assert(sizeof(ViewConstants) == 288);
    static_assert(offsetof(ViewConstants, inverseViewProjection) == 192);
    static_assert(offsetof(ViewConstants, cameraPosition) == 256);
    static_assert(offsetof(ViewConstants, frameNumber) == 268);
    static_assert(offsetof(ViewConstants, viewportSize) == 272);
    static_assert(offsetof(ViewConstants, inverseViewportSize) == 280);
}
