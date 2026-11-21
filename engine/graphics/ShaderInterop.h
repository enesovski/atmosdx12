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

    // Mirrors ViewData in shaders/Scene/SceneData.hlsli. One per rendered view: index 0 is the
    // camera, then shadow cascades, then shadow-atlas tiles.
    struct ViewData
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4 atlasScaleOffset;  // shadow views: uv scale (xy) and offset (zw) in their map
        DirectX::XMFLOAT3 position;
        float texelWorldSize;                // shadow views: world size of one shadow-map texel
    };

    static_assert(sizeof(ViewData) == 96);
}
