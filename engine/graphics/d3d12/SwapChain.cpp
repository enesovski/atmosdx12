#include "graphics/d3d12/SwapChain.h"
#include "graphics/d3d12/GraphicsDevice.h"

#include <format>

namespace atmos
{
    SwapChain::SwapChain(GraphicsDevice& device, HWND window, uint32_t width, uint32_t height)
        : device_(device), width_(width), height_(height)
    {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = GraphicsConfig::BackBufferFormat;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = GraphicsConfig::BackBufferCount;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> swapChain;
        ThrowIfFailed(device.GetFactory()->CreateSwapChainForHwnd(
                          device.GetGraphicsQueue(), window, &desc, nullptr, nullptr, &swapChain),
                      "Failed to create swap chain");

        ThrowIfFailed(swapChain.As(&swapChain_), "Failed to query IDXGISwapChain3");
        ThrowIfFailed(device.GetFactory()->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER),
                      "Failed to disable Alt+Enter");

        AcquireBackBuffers();
    }

    Texture& SwapChain::GetCurrentBackBuffer()
    {
        return backBuffers_[swapChain_->GetCurrentBackBufferIndex()];
    }

    void SwapChain::Present()
    {
        ThrowIfFailed(swapChain_->Present(1, 0), "Failed to present");
    }

    void SwapChain::Resize(uint32_t width, uint32_t height)
    {
        // DXGI refuses to resize while any back-buffer reference is alive.
        for (Texture& backBuffer : backBuffers_)
            backBuffer = Texture();

        ThrowIfFailed(swapChain_->ResizeBuffers(GraphicsConfig::BackBufferCount, width, height,
                                                GraphicsConfig::BackBufferFormat, 0),
                      "Failed to resize swap chain");

        width_ = width;
        height_ = height;
        AcquireBackBuffers();
    }

    void SwapChain::AcquireBackBuffers()
    {
        for (uint32_t i = 0; i < GraphicsConfig::BackBufferCount; ++i)
        {
            ComPtr<ID3D12Resource> buffer;
            ThrowIfFailed(swapChain_->GetBuffer(i, IID_PPV_ARGS(&buffer)), "Failed to get back buffer");

            backBuffers_[i] = Texture(device_, std::move(buffer), TextureUsage::RenderTarget,
                                      std::format(L"Back Buffer {}", i), D3D12_RESOURCE_STATE_PRESENT);
        }
    }
}
