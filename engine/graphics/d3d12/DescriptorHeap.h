#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace atmos
{
    // For shader-visible heaps, `index` is the bindless index (ResourceDescriptorHeap[index]).
    struct Descriptor
    {
        static constexpr uint32_t InvalidIndex = UINT32_MAX;

        uint32_t index = InvalidIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};

        bool IsValid() const { return index != InvalidIndex; }
    };

    // Fixed-capacity heap with free-list allocation of single descriptors.
    class DescriptorHeap
    {
    public:
        DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity,
                       bool shaderVisible, std::wstring_view debugName);

        DescriptorHeap(const DescriptorHeap&) = delete;
        DescriptorHeap& operator=(const DescriptorHeap&) = delete;

        Descriptor Allocate();

        // The GPU must no longer use it.
        void Free(Descriptor& descriptor);

        ID3D12DescriptorHeap* GetHeap() const { return heap_.Get(); }

    private:
        Descriptor MakeDescriptor(uint32_t index) const;

        ComPtr<ID3D12DescriptorHeap> heap_;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart_{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart_{};
        uint32_t descriptorSize_ = 0;
        uint32_t capacity_ = 0;
        uint32_t nextUnused_ = 0;
        std::vector<uint32_t> freeIndices_;
        bool shaderVisible_ = false;
    };
}
