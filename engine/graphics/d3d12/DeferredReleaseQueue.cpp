#include "graphics/d3d12/DeferredReleaseQueue.h"

#include <limits>

namespace atmos
{
    void DeferredReleaseQueue::Release(ComPtr<ID3D12Resource> resource)
    {
        if (resource)
            resources_.push_back({ pendingFenceValue_, std::move(resource) });
    }

    void DeferredReleaseQueue::Release(DescriptorHeap& heap, Descriptor& descriptor)
    {
        if (!descriptor.IsValid())
            return;

        descriptors_.push_back({ pendingFenceValue_, &heap, descriptor });
        descriptor = {};
    }

    void DeferredReleaseQueue::ReleaseCompleted(uint64_t completedFenceValue)
    {
        // Fence values only grow, so both queues are sorted.
        while (!resources_.empty() && resources_.front().fenceValue <= completedFenceValue)
            resources_.pop_front();

        while (!descriptors_.empty() && descriptors_.front().fenceValue <= completedFenceValue)
        {
            PendingDescriptor& pending = descriptors_.front();
            pending.heap->Free(pending.descriptor);
            descriptors_.pop_front();
        }
    }

    void DeferredReleaseQueue::ReleaseAll()
    {
        ReleaseCompleted(std::numeric_limits<uint64_t>::max());
    }
}
