#include "graphics/d3d12/UploadAllocator.h"
#include "core/Assert.h"

namespace atmos
{
    namespace
    {
        uint64_t AlignUp(uint64_t value, uint64_t alignment)
        {
            return (value + alignment - 1) & ~(alignment - 1);
        }
    }

    UploadAllocator::UploadAllocator(ID3D12Device* device, uint64_t bytesPerFrame, uint32_t frameCount)
        : bytesPerFrame_(AlignUp(bytesPerFrame, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)),
          frameCount_(frameCount)
    {
        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytesPerFrame_ * frameCount;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                      IID_PPV_ARGS(&buffer_)),
                      "Failed to create upload buffer");
        SetDebugName(buffer_.Get(), L"Per-Frame Upload Buffer");

        const D3D12_RANGE noRead{ 0, 0 };
        void* mapped = nullptr;
        ThrowIfFailed(buffer_->Map(0, &noRead, &mapped), "Failed to map upload buffer");

        mappedData_ = static_cast<std::byte*>(mapped);
        gpuBase_ = buffer_->GetGPUVirtualAddress();
    }

    UploadAllocator::~UploadAllocator()
    {
        if (mappedData_)
            buffer_->Unmap(0, nullptr);
    }

    void UploadAllocator::BeginFrame(uint32_t frameIndex)
    {
        ATMOS_ASSERT(frameIndex < frameCount_, "Frame index out of range");
        frameStart_ = static_cast<uint64_t>(frameIndex) * bytesPerFrame_;
        offset_ = 0;
    }

    UploadAllocation UploadAllocator::Allocate(uint64_t size, uint64_t alignment)
    {
        ATMOS_ASSERT(size > 0, "Upload allocation must not be empty");

        const uint64_t alignedOffset = AlignUp(offset_, alignment);

        if (alignedOffset + size > bytesPerFrame_)
            throw std::runtime_error("Per-frame upload memory exhausted; raise UploadBytesPerFrame");

        offset_ = alignedOffset + size;

        const uint64_t bufferOffset = frameStart_ + alignedOffset;
        return { mappedData_ + bufferOffset, gpuBase_ + bufferOffset };
    }
}
