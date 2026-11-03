#include "FrameScheduler.h"
#include "../core/DxError.h"

#include <limits>
#include <stdexcept>

FrameScheduler::FrameScheduler(ID3D12Device* device, ID3D12CommandQueue* commandQueue)
    : commandQueue_(commandQueue)
{
    for (FrameResources& frame : frames_)
    {
        CheckHRESULT(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.commandAllocator)
        ), "Failed to create command allocator");
    }

    CheckHRESULT(device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0].commandAllocator.Get(),
        nullptr, IID_PPV_ARGS(&commandList_)
    ), "Failed to create command list");

    CheckHRESULT(commandList_->Close(), "Failed to close initial command list");
    CheckHRESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
                 "Failed to create fence");

    fenceEvent_.reset(CreateEventW(nullptr, FALSE, FALSE, nullptr));

    if (!fenceEvent_)
        throw std::runtime_error("Failed to create fence event");
}

UINT FrameScheduler::BeginFrame()
{
    FrameResources& frame = frames_[frameIndex_];

    // This fence protects both the allocator and the pass's matching upload slot.
    WaitForFence(frame.fenceValue);

    CheckHRESULT(frame.commandAllocator->Reset(), "Failed to reset command allocator");
    CheckHRESULT(commandList_->Reset(frame.commandAllocator.Get(), nullptr),
                 "Failed to reset command list");

    return frameIndex_;
}

void FrameScheduler::Submit()
{
    CheckHRESULT(commandList_->Close(), "Failed to close command list");

    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);
}

void FrameScheduler::EndFrame()
{
    frames_[frameIndex_].fenceValue = Signal();
    frameIndex_ = (frameIndex_ + 1) % GraphicsConfig::FrameCount;
}

void FrameScheduler::Flush()
{
    // Signal fresh work as well, covering a submission followed by a failed Present.
    WaitForFence(Signal());
}

UINT64 FrameScheduler::Signal()
{
    const UINT64 value = nextFenceValue_++;
    CheckHRESULT(commandQueue_->Signal(fence_.Get(), value), "Failed to signal fence");

    return value;
}

void FrameScheduler::WaitForFence(UINT64 value)
{
    UINT64 completedValue = fence_->GetCompletedValue();

    if (completedValue == std::numeric_limits<UINT64>::max())
        throw std::runtime_error("Direct3D 12 device removed while waiting for GPU");

    if (completedValue >= value)
        return;

    HRESULT eventResult = fence_->SetEventOnCompletion(value, fenceEvent_.get());
    DWORD waitResult = WAIT_FAILED;

    if (SUCCEEDED(eventResult))
        waitResult = WaitForSingleObject(fenceEvent_.get(), INFINITE);

    // An event failure or stale signal must not release resources still in flight.
    while (fence_->GetCompletedValue() < value)
        Sleep(1);

    if (fence_->GetCompletedValue() == std::numeric_limits<UINT64>::max())
        throw std::runtime_error("Direct3D 12 device removed while waiting for GPU");

    CheckHRESULT(eventResult, "Failed to register fence event");

    if (waitResult != WAIT_OBJECT_0)
        throw std::runtime_error("Failed while waiting for GPU");
}
