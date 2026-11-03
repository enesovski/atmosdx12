#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "GraphicsConfig.h"

#include <array>
#include <memory>

class FrameScheduler
{
public:
    FrameScheduler(ID3D12Device* device, ID3D12CommandQueue* commandQueue);

    FrameScheduler(const FrameScheduler&) = delete;
    FrameScheduler& operator=(const FrameScheduler&) = delete;

    UINT BeginFrame();
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    void Submit();
    void EndFrame();
    void Flush();

private:
    struct FrameResources
    {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
        UINT64 fenceValue = 0;
    };

    UINT64 Signal();
    void WaitForFence(UINT64 value);

    // The context's queue outlives the scheduler.
    ID3D12CommandQueue* commandQueue_;

    std::array<FrameResources, GraphicsConfig::FrameCount> frames_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::unique_ptr<void, decltype(&CloseHandle)> fenceEvent_{ nullptr, &CloseHandle };

    UINT frameIndex_ = 0;
    UINT64 nextFenceValue_ = 1;
};
