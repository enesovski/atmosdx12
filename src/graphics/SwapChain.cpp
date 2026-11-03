#include "SwapChain.h"
#include "../core/DxError.h"

using Microsoft::WRL::ComPtr;

SwapChain::SwapChain(HWND window, IDXGIFactory6* factory, ID3D12Device* device,
                     ID3D12CommandQueue* commandQueue, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = GraphicsConfig::BackBufferFormat;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = GraphicsConfig::FrameCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain;

    CheckHRESULT(factory->CreateSwapChainForHwnd(
        commandQueue, window, &desc, nullptr, nullptr, &swapChain
    ), "Failed to create swap chain");

    CheckHRESULT(swapChain.As(&swapChain_), "Failed to query swap chain 3");
    CheckHRESULT(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER),
                 "Failed to disable DXGI Alt+Enter handling");

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = GraphicsConfig::FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    CheckHRESULT(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_)),
                 "Failed to create RTV heap");

    rtvDescriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (UINT i = 0; i < GraphicsConfig::FrameCount; ++i)
    {
        CheckHRESULT(swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i])),
                     "Failed to acquire back buffer");

        device->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, GetRenderTargetView(i));
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE SwapChain::GetRenderTargetView(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * rtvDescriptorSize_;

    return handle;
}

void SwapChain::Present()
{
    CheckHRESULT(swapChain_->Present(1, 0), "Failed to present frame");
}
