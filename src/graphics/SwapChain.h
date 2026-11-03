#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "GraphicsConfig.h"

#include <array>

class SwapChain
{
public:
    SwapChain(HWND window, IDXGIFactory6* factory, ID3D12Device* device,
              ID3D12CommandQueue* commandQueue, int width, int height);

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    UINT GetBackBufferIndex() const { return swapChain_->GetCurrentBackBufferIndex(); }
    ID3D12Resource* GetBackBuffer(UINT index) const { return backBuffers_[index].Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView(UINT index) const;

    void Present();

private:
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, GraphicsConfig::FrameCount> backBuffers_;

    UINT rtvDescriptorSize_ = 0;
};
