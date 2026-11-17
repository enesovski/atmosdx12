#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/GraphicsConfig.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <memory>

namespace atmos
{
    class DeferredReleaseQueue;
    class GraphicsDevice;

    // Rotates FramesInFlight frame slots. Each slot owns a command allocator and the fence
    // value that guards it; anything else indexed by the slot (e.g. upload memory) shares
    // that guarantee. It also drives the device's DeferredReleaseQueue with the same fence.
    class FrameScheduler
    {
    public:
        explicit FrameScheduler(GraphicsDevice& device);

        FrameScheduler(const FrameScheduler&) = delete;
        FrameScheduler& operator=(const FrameScheduler&) = delete;

        // Waits until the GPU finished this slot's previous frame, then opens the command list.
        uint32_t BeginFrame();
        ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
        void Submit();
        void EndFrame();

        // Blocks until the GPU has finished all submitted work.
        void Flush();

    private:
        struct FrameSlot
        {
            ComPtr<ID3D12CommandAllocator> commandAllocator;
            uint64_t fenceValue = 0;
        };

        uint64_t Signal();
        void WaitForFence(uint64_t value);

        ID3D12CommandQueue* queue_;
        DeferredReleaseQueue& releaseQueue_;
        std::array<FrameSlot, GraphicsConfig::FramesInFlight> slots_;
        ComPtr<ID3D12GraphicsCommandList> commandList_;
        ComPtr<ID3D12Fence> fence_;
        std::unique_ptr<void, decltype(&CloseHandle)> fenceEvent_{ nullptr, &CloseHandle };
        uint32_t slotIndex_ = 0;
        uint64_t nextFenceValue_ = 1;
    };
}
