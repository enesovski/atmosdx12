#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DeferredReleaseQueue.h"
#include "graphics/d3d12/DescriptorHeap.h"
#include "graphics/d3d12/GraphicsDeviceOptions.h"

#include <dxgi1_6.h>

namespace atmos
{
    // GPU-wide objects: adapter, device, queues, descriptor heaps, bindless root signature.
    class GraphicsDevice
    {
    public:
        explicit GraphicsDevice(const GraphicsDeviceOptions& options);

        GraphicsDevice(const GraphicsDevice&) = delete;
        GraphicsDevice& operator=(const GraphicsDevice&) = delete;

        IDXGIFactory6* GetFactory() const { return factory_.Get(); }
        ID3D12Device* GetNativeDevice() const { return device_.Get(); }
        ID3D12CommandQueue* GetGraphicsQueue() const { return graphicsQueue_.Get(); }
        ID3D12CommandQueue* GetCopyQueue() const { return copyQueue_.Get(); }
        ID3D12RootSignature* GetBindlessRootSignature() const { return bindlessRootSignature_.Get(); }

        DescriptorHeap& GetShaderResourceHeap() { return shaderResourceHeap_; }
        DescriptorHeap& GetRenderTargetHeap() { return renderTargetHeap_; }
        DescriptorHeap& GetDepthStencilHeap() { return depthStencilHeap_; }

        DeferredReleaseQueue& GetReleaseQueue() { return releaseQueue_; }

        // Forwards debug-layer messages to the log.
        void LogDebugMessages();

        // Logs the removal reason and DRED data.
        void LogDeviceRemovedInfo();

    private:
        void ConfigureInfoQueue();

        // Declaration order is construction order.
        ComPtr<IDXGIFactory6> factory_;
        ComPtr<IDXGIAdapter1> adapter_;
        ComPtr<ID3D12Device> device_;
        ComPtr<ID3D12CommandQueue> graphicsQueue_;
        ComPtr<ID3D12CommandQueue> copyQueue_;
        DescriptorHeap shaderResourceHeap_;
        DescriptorHeap renderTargetHeap_;
        DescriptorHeap depthStencilHeap_;
        DeferredReleaseQueue releaseQueue_;  // After the heaps: destroyed before them.
        ComPtr<ID3D12RootSignature> bindlessRootSignature_;
        ComPtr<ID3D12InfoQueue> infoQueue_;
    };
}
