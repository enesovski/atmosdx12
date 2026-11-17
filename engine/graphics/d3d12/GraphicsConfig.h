#pragma once

#include <dxgiformat.h>

#include <cstdint>

namespace atmos::GraphicsConfig
{
    // CPU may record this many frames ahead of the GPU.
    inline constexpr uint32_t FramesInFlight = 2;
    inline constexpr uint32_t BackBufferCount = 2;

    inline constexpr DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    inline constexpr DXGI_FORMAT HdrColorFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    inline constexpr DXGI_FORMAT DepthFormat = DXGI_FORMAT_D32_FLOAT;

    inline constexpr uint32_t ShaderResourceDescriptorCount = 65536;
    inline constexpr uint32_t RenderTargetDescriptorCount = 256;
    inline constexpr uint32_t DepthStencilDescriptorCount = 64;

    inline constexpr uint64_t UploadBytesPerFrame = 4ull * 1024 * 1024;
}
