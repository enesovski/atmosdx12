#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace atmos
{
    class GraphicsDevice;

    enum class BufferUsage : uint32_t
    {
        None = 0,
        ShaderResource = 1 << 0,
        UnorderedAccess = 1 << 1,
    };

    constexpr BufferUsage operator|(BufferUsage a, BufferUsage b)
    {
        return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr bool HasUsage(BufferUsage usage, BufferUsage flag)
    {
        return (static_cast<uint32_t>(usage) & static_cast<uint32_t>(flag)) != 0;
    }

    enum class BufferMemory
    {
        GpuOnly,  // Default heap; filled through UploadContext.
        Upload,   // CPU-writable, persistently mapped; for data rewritten every frame.
    };

    struct BufferDesc
    {
        uint64_t size = 0;
        uint32_t stride = 0;  // 0: raw ByteAddressBuffer view; otherwise StructuredBuffer stride.
        BufferUsage usage = BufferUsage::ShaderResource;
        BufferMemory memory = BufferMemory::GpuOnly;
        std::wstring debugName;
    };

    // Linear GPU buffer with bindless views. Static buffers stay in COMMON and rely on
    // D3D12 implicit promotion (copy dest, SRV, index buffer) and decay, so they need no
    // explicit barriers. Destruction goes through the device's DeferredReleaseQueue.
    class Buffer
    {
    public:
        Buffer() = default;
        Buffer(GraphicsDevice& device, const BufferDesc& desc);
        ~Buffer();

        Buffer(Buffer&& other) noexcept;
        Buffer& operator=(Buffer&& other) noexcept;
        Buffer(const Buffer&) = delete;
        Buffer& operator=(const Buffer&) = delete;

        bool IsValid() const { return resource_ != nullptr; }
        ID3D12Resource* GetResource() const { return resource_.Get(); }
        const BufferDesc& GetDesc() const { return desc_; }
        uint64_t GetSize() const { return desc_.size; }
        D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return resource_->GetGPUVirtualAddress(); }

        uint32_t GetSrvIndex() const;
        uint32_t GetUavIndex() const;

        // Upload memory only.
        std::byte* GetMappedData() const;

    private:
        void CreateViews();
        void Release();

        GraphicsDevice* device_ = nullptr;
        ComPtr<ID3D12Resource> resource_;
        BufferDesc desc_;
        std::byte* mappedData_ = nullptr;
        Descriptor srv_;
        Descriptor uav_;
    };
}
