#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <cstdint>
#include <deque>

namespace atmos
{
    // Keeps released GPU objects alive until the frame fence passes their release frame.
    class DeferredReleaseQueue
    {
    public:
        DeferredReleaseQueue() = default;
        DeferredReleaseQueue(const DeferredReleaseQueue&) = delete;
        DeferredReleaseQueue& operator=(const DeferredReleaseQueue&) = delete;

        void Release(ComPtr<ID3D12Resource> resource);
        void Release(DescriptorHeap& heap, Descriptor& descriptor);  // Resets `descriptor`.

        void SetPendingFenceValue(uint64_t value) { pendingFenceValue_ = value; }
        void ReleaseCompleted(uint64_t completedFenceValue);

        // Only when the GPU is idle.
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
