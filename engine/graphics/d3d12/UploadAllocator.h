#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <cstddef>
#include <cstdint>

namespace atmos
{
    struct UploadAllocation
    {
        std::byte* cpuAddress = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
    };

    // Per-frame linear upload memory; one region per frame slot, reset after its fence.
    class UploadAllocator
    {
    public:
        UploadAllocator(ID3D12Device* device, uint64_t bytesPerFrame, uint32_t frameCount);
        ~UploadAllocator();

        UploadAllocator(const UploadAllocator&) = delete;
        UploadAllocator& operator=(const UploadAllocator&) = delete;

        void BeginFrame(uint32_t frameIndex);

        // Valid until this frame slot comes around again.
        UploadAllocation Allocate(uint64_t size,
                                  uint64_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    private:
        ComPtr<ID3D12Resource> buffer_;
        std::byte* mappedData_ = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuBase_ = 0;
        uint64_t bytesPerFrame_ = 0;
        uint32_t frameCount_ = 0;
        uint64_t frameStart_ = 0;
        uint64_t offset_ = 0;
    };
}
