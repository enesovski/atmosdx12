#include "graphics/d3d12/DescriptorHeap.h"
#include "core/Assert.h"

namespace atmos
{
    DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                   uint32_t capacity, bool shaderVisible, std::wstring_view debugName)
        : capacity_(capacity), shaderVisible_(shaderVisible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = type;
        desc.NumDescriptors = capacity;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                   : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_)),
                      "Failed to create descriptor heap");
        SetDebugName(heap_.Get(), debugName);

        descriptorSize_ = device->GetDescriptorHandleIncrementSize(type);
        cpuStart_ = heap_->GetCPUDescriptorHandleForHeapStart();

        if (shaderVisible)
            gpuStart_ = heap_->GetGPUDescriptorHandleForHeapStart();
    }

    Descriptor DescriptorHeap::Allocate()
    {
        if (!freeIndices_.empty())
        {
            const uint32_t index = freeIndices_.back();
            freeIndices_.pop_back();
            return MakeDescriptor(index);
        }

        if (nextUnused_ >= capacity_)
            throw std::runtime_error("Descriptor heap is full");

        return MakeDescriptor(nextUnused_++);
    }

    void DescriptorHeap::Free(Descriptor& descriptor)
    {
        if (!descriptor.IsValid())
            return;

        ATMOS_ASSERT(descriptor.index < nextUnused_, "Descriptor does not belong to this heap");
        freeIndices_.push_back(descriptor.index);
        descriptor = {};
    }

    Descriptor DescriptorHeap::MakeDescriptor(uint32_t index) const
    {
        const uint64_t offset = static_cast<uint64_t>(index) * descriptorSize_;

        Descriptor descriptor;
        descriptor.index = index;
        descriptor.cpu.ptr = cpuStart_.ptr + offset;

        if (shaderVisible_)
            descriptor.gpu.ptr = gpuStart_.ptr + offset;

        return descriptor;
    }
}
