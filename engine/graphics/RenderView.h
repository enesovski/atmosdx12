#pragma once

#include <DirectXMath.h>

#include <cstdint>

namespace atmos
{
    class Camera;

    // Everything the renderer needs to know about one point of view.
    struct RenderView
    {
        DirectX::XMFLOAT4X4 viewMatrix{};
        DirectX::XMFLOAT4X4 projectionMatrix{};
        DirectX::XMFLOAT3 position{};
        uint32_t width = 0;
        uint32_t height = 0;

        static RenderView FromCamera(const Camera& camera, uint32_t width, uint32_t height);
    };
}
