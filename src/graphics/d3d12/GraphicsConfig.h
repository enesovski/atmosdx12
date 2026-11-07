
#pragma once

#include <dxgiformat.h>

namespace GraphicsConfig
{
    inline constexpr unsigned int FrameCount = 2;

    inline constexpr DXGI_FORMAT BackBufferFormat =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    inline constexpr DXGI_FORMAT HdrColorFormat =
        DXGI_FORMAT_R16G16B16A16_FLOAT;
}
