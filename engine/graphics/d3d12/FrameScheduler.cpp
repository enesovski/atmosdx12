#include "graphics/d3d12/FrameScheduler.h"
#include "graphics/d3d12/DeferredReleaseQueue.h"
#include "graphics/d3d12/GraphicsDevice.h"

#include <format>
#include <limits>

namespace atmos
{
    namespace
    {
        constexpr uint64_t DeviceRemovedFenceValue = std::numeric_limits<uint64_t>::max();
    }

    FrameScheduler::FrameScheduler(GraphicsDevice& device)
        : queue_(device.GetGraphicsQueue()), releaseQueue_(device.GetReleaseQueue())
    {
        releaseQueue_.SetPendingFenceValue(nextFenceValue_);

        ID3D12Device* nativeDevice = device.GetNativeDevice();

        for (uint32_t i = 0; i < slots_.size(); ++i)
        {
            ThrowIfFailed(nativeDevice->CreateCommandAllocator(
                              D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&slots_[i].commandAllocator)),
                          "Failed to create command allocator");
            SetDebugName(slots_[i].commandAllocator.Get(), std::format(L"Frame Allocator {}", i));
        }

        ThrowIfFailed(nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      slots_[0].commandAllocator.Get(), nullptr,
                                                      IID_PPV_ARGS(&commandList_)),
                      "Failed to create command list");
        SetDebugName(commandList_.Get(), L"Frame Command List");
        ThrowIfFailed(commandList_->Close(), "Failed to close initial command list");

        ThrowIfFailed(nativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
                      "Failed to create frame fence");
        SetDebugName(fence_.Get(), L"Frame Fence");

        fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

        if (!fenceEvent_)
            throw std::runtime_error("Failed to create fence event");
    }

    uint32_t FrameScheduler::BeginFrame()
    {
        FrameSlot& slot = slots_[slotIndex_];
        WaitForFence(slot.fenceValue);
        releaseQueue_.ReleaseCompleted(fence_->GetCompletedValue());

        ThrowIfFailed(slot.commandAllocator->Reset(), "Failed to reset command allocator");
        ThrowIfFailed(commandList_->Reset(slot.commandAllocator.Get(), nullptr),
                      "Failed to reset command list");

        return slotIndex_;
    }

    void FrameScheduler::Submit()
    {
        ThrowIfFailed(commandList_->Close(), "Failed to close command list");

        ID3D12CommandList* lists[] = { commandList_.Get() };
        queue_->ExecuteCommandLists(1, lists);
    }

    void FrameScheduler::EndFrame()
    {
        slots_[slotIndex_].fenceValue = Signal();
        slotIndex_ = (slotIndex_ + 1) % GraphicsConfig::FramesInFlight;
    }

    void FrameScheduler::Flush()
    {
        // A fresh signal also covers work submitted before a failed Present.
        WaitForFence(Signal());
        releaseQueue_.ReleaseCompleted(fence_->GetCompletedValue());
    }

    uint64_t FrameScheduler::Signal()
    {
        const uint64_t value = nextFenceValue_++;
        ThrowIfFailed(queue_->Signal(fence_.Get(), value), "Failed to signal fence");

        // Anything released from now on must wait for the next signal.
        releaseQueue_.SetPendingFenceValue(nextFenceValue_);
        return value;
    }

    void FrameScheduler::WaitForFence(uint64_t value)
    {
        if (fence_->GetCompletedValue() == DeviceRemovedFenceValue)
            throw DxException(DXGI_ERROR_DEVICE_REMOVED, "GPU device removed while waiting for a frame");

        if (fence_->GetCompletedValue() >= value)
            return;

        const HRESULT eventResult = fence_->SetEventOnCompletion(value, fenceEvent_.get());
        DWORD waitResult = WAIT_FAILED;

        if (SUCCEEDED(eventResult))
            waitResult = WaitForSingleObject(fenceEvent_.get(), INFINITE);

        // Never release in-flight resources because an event failed: poll as a fallback.
        while (fence_->GetCompletedValue() < value)
            Sleep(1);

        if (fence_->GetCompletedValue() == DeviceRemovedFenceValue)
            throw DxException(DXGI_ERROR_DEVICE_REMOVED, "GPU device removed while waiting for a frame");

        ThrowIfFailed(eventResult, "Failed to register fence event");

        if (waitResult != WAIT_OBJECT_0)
            throw std::runtime_error("Failed while waiting for the GPU");
    }
}
