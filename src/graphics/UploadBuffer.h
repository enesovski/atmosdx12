#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include "../core/DxError.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>

class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, std::size_t elementSize, UINT elementCount)
        : elementSize_(elementSize), elementCount_(elementCount)
    {
        constexpr std::size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

        if (elementSize == 0 || elementCount == 0 ||
            elementSize > std::numeric_limits<std::size_t>::max() - (alignment - 1))
            throw std::invalid_argument("Invalid upload buffer size");

        elementStride_ = (elementSize + alignment - 1) & ~(alignment - 1);

        if (elementStride_ > std::numeric_limits<std::size_t>::max() / elementCount)
            throw std::invalid_argument("Upload buffer size overflow");

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = elementStride_ * elementCount;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        CheckHRESULT(device->CreateCommittedResource(
            &heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&resource_)
        ), "Failed to create upload buffer");

        D3D12_RANGE readRange{ 0, 0 };
        void* mapped = nullptr;
        CheckHRESULT(resource_->Map(0, &readRange, &mapped), "Failed to map upload buffer");
        mappedData_ = static_cast<std::byte*>(mapped);
    }

    ~UploadBuffer()
    {
        if (mappedData_)
            resource_->Unmap(0, nullptr);
    }

    UploadBuffer(const UploadBuffer&) = delete;
    UploadBuffer& operator=(const UploadBuffer&) = delete;

    // The caller must wait for the frame slot before writing its constants.
    void Write(UINT index, const void* data, std::size_t size)
    {
        if (index >= elementCount_ || size != elementSize_)
            throw std::out_of_range("Invalid upload buffer write");

        std::memcpy(mappedData_ + index * elementStride_, data, size);
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress(UINT index) const
    {
        if (index >= elementCount_)
            throw std::out_of_range("Invalid upload buffer index");

        return resource_->GetGPUVirtualAddress() + index * elementStride_;
    }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    std::byte* mappedData_ = nullptr;

    std::size_t elementSize_;
    std::size_t elementStride_ = 0;
    UINT elementCount_;
};
