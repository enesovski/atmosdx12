#include "graphics/d3d12/Buffer.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "core/Assert.h"

#include <utility>

namespace atmos
{
    Buffer::Buffer(GraphicsDevice& device, const BufferDesc& desc)
        : device_(&device), desc_(desc)
    {
        ATMOS_ASSERT(desc.size > 0, "Buffer size must be positive");
        ATMOS_ASSERT(desc.stride == 0 || desc.size % desc.stride == 0, "Size must be a multiple of stride");
        ATMOS_ASSERT(!(desc.memory == BufferMemory::Upload && HasUsage(desc.usage, BufferUsage::UnorderedAccess)),
                     "Upload buffers cannot be UAVs");

        const bool isUpload = desc.memory == BufferMemory::Upload;

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = isUpload ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Width = desc.size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (HasUsage(desc.usage, BufferUsage::UnorderedAccess))
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        const D3D12_RESOURCE_STATES initialState = isUpload ? D3D12_RESOURCE_STATE_GENERIC_READ
                                                            : D3D12_RESOURCE_STATE_COMMON;

        state_ = initialState;

        ThrowIfFailed(device.GetNativeDevice()->CreateCommittedResource(
                          &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr,
                          IID_PPV_ARGS(&resource_)),
                      "Failed to create buffer");
        SetDebugName(resource_.Get(), desc.debugName);

        if (isUpload)
        {
            const D3D12_RANGE noRead{ 0, 0 };
            void* mapped = nullptr;
            ThrowIfFailed(resource_->Map(0, &noRead, &mapped), "Failed to map upload buffer");
            mappedData_ = static_cast<std::byte*>(mapped);
        }

        CreateViews();
    }

    Buffer::~Buffer()
    {
        Release();
    }

    Buffer::Buffer(Buffer&& other) noexcept
    {
        *this = std::move(other);
    }

    Buffer& Buffer::operator=(Buffer&& other) noexcept
    {
        if (this != &other)
        {
            Release();

            device_ = std::exchange(other.device_, nullptr);
            resource_ = std::move(other.resource_);
            desc_ = std::move(other.desc_);
            mappedData_ = std::exchange(other.mappedData_, nullptr);
            state_ = other.state_;
            srv_ = std::exchange(other.srv_, {});
            uav_ = std::exchange(other.uav_, {});
        }

        return *this;
    }

    uint32_t Buffer::GetSrvIndex() const
    {
        ATMOS_ASSERT(srv_.IsValid(), "Buffer has no ShaderResource usage");
        return srv_.index;
    }

    uint32_t Buffer::GetUavIndex() const
    {
        ATMOS_ASSERT(uav_.IsValid(), "Buffer has no UnorderedAccess usage");
        return uav_.index;
    }

    std::byte* Buffer::GetMappedData() const
    {
        ATMOS_ASSERT(mappedData_, "Only upload buffers are mapped");
        return mappedData_;
    }

    void Buffer::CreateViews()
    {
        ID3D12Device* device = device_->GetNativeDevice();
        const bool isRaw = desc_.stride == 0;
        const UINT elementCount = static_cast<UINT>(isRaw ? desc_.size / 4 : desc_.size / desc_.stride);

        if (HasUsage(desc_.usage, BufferUsage::ShaderResource))
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            viewDesc.Format = isRaw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
            viewDesc.Buffer.NumElements = elementCount;
            viewDesc.Buffer.StructureByteStride = isRaw ? 0 : desc_.stride;
            viewDesc.Buffer.Flags = isRaw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

            srv_ = device_->GetShaderResourceHeap().Allocate();
            device->CreateShaderResourceView(resource_.Get(), &viewDesc, srv_.cpu);
        }

        if (HasUsage(desc_.usage, BufferUsage::UnorderedAccess))
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
            viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            viewDesc.Format = isRaw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
            viewDesc.Buffer.NumElements = elementCount;
            viewDesc.Buffer.StructureByteStride = isRaw ? 0 : desc_.stride;
            viewDesc.Buffer.Flags = isRaw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

            uav_ = device_->GetShaderResourceHeap().Allocate();
            device->CreateUnorderedAccessView(resource_.Get(), nullptr, &viewDesc, uav_.cpu);
        }
    }

    void Buffer::Release()
    {
        if (device_)
        {
            DeferredReleaseQueue& queue = device_->GetReleaseQueue();
            queue.Release(device_->GetShaderResourceHeap(), srv_);
            queue.Release(device_->GetShaderResourceHeap(), uav_);
            queue.Release(std::move(resource_));
        }

        resource_.Reset();
        mappedData_ = nullptr;
        device_ = nullptr;
    }
}
