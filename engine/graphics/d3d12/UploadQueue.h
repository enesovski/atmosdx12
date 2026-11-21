#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <windows.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <vector>

namespace atmos
{
    class Buffer;
    class GraphicsDevice;
    class Texture;

    // One subresource (mip) of CPU texture data, tightly packed rows.
    struct TextureSubresourceData
    {
        const void* data = nullptr;
        uint64_t rowPitch = 0;    // bytes per row (per block row for BC formats)
        uint64_t slicePitch = 0;  // bytes for the whole subresource
    };

    // Asynchronous uploads on the copy queue. A resource may be used once IsComplete(Submit())
    // is true. Destinations stay in COMMON.
    class UploadQueue
    {
    public:
        explicit UploadQueue(GraphicsDevice& device);
        ~UploadQueue();

        UploadQueue(const UploadQueue&) = delete;
        UploadQueue& operator=(const UploadQueue&) = delete;

        void UploadBuffer(const Buffer& destination, const void* data, uint64_t size);
        void UploadTexture(const Texture& destination, std::span<const TextureSubresourceData> subresources);

        // Returns 0 when nothing was recorded.
        uint64_t Submit();
        bool IsComplete(uint64_t fenceValue) const;

        // Frees staging memory of finished batches.
        void ReleaseCompleted();
        void WaitIdle();

    private:
        struct Batch
        {
            ComPtr<ID3D12CommandAllocator> allocator;
            std::vector<ComPtr<ID3D12Resource>> stagingBuffers;
            uint64_t fenceValue = 0;
        };

        void EnsureRecording();
        ComPtr<ID3D12Resource> CreateStagingBuffer(uint64_t size, void** mappedData);

        ID3D12Device* device_;
        ID3D12CommandQueue* queue_;
        ComPtr<ID3D12GraphicsCommandList> commandList_;
        ComPtr<ID3D12Fence> fence_;
        std::unique_ptr<void, decltype(&CloseHandle)> fenceEvent_{ nullptr, &CloseHandle };

        Batch recording_;
        bool isRecording_ = false;
        std::deque<Batch> inFlight_;
        std::vector<ComPtr<ID3D12CommandAllocator>> freeAllocators_;
        uint64_t nextFenceValue_ = 1;
    };
}
