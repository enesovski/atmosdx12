
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>

class D3D12Renderer
{
public:
    D3D12Renderer(HWND window, IDXGIFactory6* factory, ID3D12Device* device, int width, int height);
    ~D3D12Renderer();

    D3D12Renderer(const D3D12Renderer&) = delete;
    D3D12Renderer& operator=(const D3D12Renderer&) = delete;

    void Render();

private:
    static constexpr UINT FrameCount = 2;

    void CreateCommandObjects();
    void CreateSwapChain(HWND window, IDXGIFactory6* factory, int width, int height);
    void CreateRenderTargets();
    void CreatePipeline();
    void WaitForGPU();

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[FrameCount];
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    UINT rtvDescriptorSize_ = 0;
    UINT64 fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;
};
