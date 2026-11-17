#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <cstdint>
#include <deque>

namespace atmos
{
    // Keeps destroyed GPU objects alive until the GPU can no longer use them.
    // Everything released is tagged with the frame fence value that will be signaled at the
    // end of the current frame; FrameScheduler frees entries once that value completes.
    class DeferredReleaseQueue
    {
    public:
        DeferredReleaseQueue() = default;
        DeferredReleaseQueue(const DeferredReleaseQueue&) = delete;
        DeferredReleaseQueue& operator=(const DeferredReleaseQueue&) = delete;

        void Release(ComPtr<ID3D12Resource> resource);
        void Release(DescriptorHeap& heap, Descriptor& descriptor);  // Resets `descriptor`.

        // Driven by FrameScheduler.
        void SetPendingFenceValue(uint64_t value) { pendingFenceValue_ = value; }
        void ReleaseCompleted(uint64_t completedFenceValue);

        // Only when the GPU is idle (e.g. right before a swap-chain resize).
        void ReleaseAll();

    private:
        struct PendingResource
        {
            uint64_t fenceValue;
            ComPtr<ID3D12Resource> resource;
        };

        struct PendingDescriptor
        {
            uint64_t fenceValue;
            DescriptorHeap* heap;
            Descriptor descriptor;
        };

        std::deque<PendingResource> resources_;
        std::deque<PendingDescriptor> descriptors_;
        uint64_t pendingFenceValue_ = 1;
    };
}
