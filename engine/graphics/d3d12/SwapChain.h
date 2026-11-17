#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/GraphicsConfig.h"
#include "graphics/d3d12/Texture.h"

#include <dxgi1_6.h>

#include <array>
#include <cstdint>

namespace atmos
{
    class GraphicsDevice;

    class SwapChain
    {
    public:
        SwapChain(GraphicsDevice& device, HWND window, uint32_t width, uint32_t height);

        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        Texture& GetCurrentBackBuffer();
        uint32_t GetWidth() const { return width_; }
        uint32_t GetHeight() const { return height_; }

        void Present();

        // The GPU must be idle: back buffers are released and recreated.
        void Resize(uint32_t width, uint32_t height);

    private:
        void AcquireBackBuffers();

        GraphicsDevice& device_;
        ComPtr<IDXGISwapChain3> swapChain_;
        std::array<Texture, GraphicsConfig::BackBufferCount> backBuffers_;
        uint32_t width_ = 0;
        uint32_t height_ = 0;
    };
}
