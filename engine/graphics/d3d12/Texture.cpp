#include "graphics/d3d12/Texture.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "core/Assert.h"

#include <algorithm>
#include <utility>

namespace atmos
{
    namespace
    {
        // Depth textures read by shaders need a typeless resource and a color SRV format.
        DXGI_FORMAT GetResourceFormat(const TextureDesc& desc)
        {
            if (desc.format == DXGI_FORMAT_D32_FLOAT && HasUsage(desc.usage, TextureUsage::ShaderResource))
                return DXGI_FORMAT_R32_TYPELESS;

            return desc.format;
        }

        DXGI_FORMAT GetShaderResourceFormat(DXGI_FORMAT format)
        {
            return format == DXGI_FORMAT_D32_FLOAT ? DXGI_FORMAT_R32_FLOAT : format;
        }

        D3D12_RESOURCE_FLAGS GetResourceFlags(TextureUsage usage)
        {
            D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

            if (HasUsage(usage, TextureUsage::RenderTarget))
                flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

            if (HasUsage(usage, TextureUsage::DepthStencil))
            {
                flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

                if (!HasUsage(usage, TextureUsage::ShaderResource))
                    flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            }

            if (HasUsage(usage, TextureUsage::UnorderedAccess))
                flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

            return flags;
        }

        D3D12_RESOURCE_STATES GetInitialState(TextureUsage usage)
        {
            if (HasUsage(usage, TextureUsage::DepthStencil))
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;

            if (HasUsage(usage, TextureUsage::RenderTarget))
                return D3D12_RESOURCE_STATE_RENDER_TARGET;

            if (HasUsage(usage, TextureUsage::UnorderedAccess))
                return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

            return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    Texture::Texture(GraphicsDevice& device, const TextureDesc& desc)
        : device_(&device), desc_(desc), state_(GetInitialState(desc.usage))
    {
        ATMOS_ASSERT(desc.width > 0 && desc.height > 0, "Texture size must be positive");

        D3D12_HEAP_PROPERTIES heapProperties{};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = desc.width;
        resourceDesc.Height = desc.height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = static_cast<UINT16>(desc.mipLevels);
        resourceDesc.Format = GetResourceFormat(desc);
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.Flags = GetResourceFlags(desc.usage);

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = desc.format;
        const D3D12_CLEAR_VALUE* optimizedClear = nullptr;

        if (HasUsage(desc.usage, TextureUsage::DepthStencil))
        {
            clearValue.DepthStencil.Depth = desc.clearDepth;
            optimizedClear = &clearValue;
        }
        else if (HasUsage(desc.usage, TextureUsage::RenderTarget))
        {
            std::copy(desc.clearColor.begin(), desc.clearColor.end(), clearValue.Color);
            optimizedClear = &clearValue;
        }

        ThrowIfFailed(device.GetNativeDevice()->CreateCommittedResource(
                          &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, state_,
                          optimizedClear, IID_PPV_ARGS(&resource_)),
                      "Failed to create texture");
        SetDebugName(resource_.Get(), desc.debugName);

        CreateViews();
    }

    Texture::Texture(GraphicsDevice& device, ComPtr<ID3D12Resource> resource, TextureUsage usage,
                     std::wstring debugName, D3D12_RESOURCE_STATES currentState)
        : device_(&device), resource_(std::move(resource)), state_(currentState)
    {
        const D3D12_RESOURCE_DESC resourceDesc = resource_->GetDesc();
        desc_.width = static_cast<uint32_t>(resourceDesc.Width);
        desc_.height = resourceDesc.Height;
        desc_.format = resourceDesc.Format;
        desc_.mipLevels = resourceDesc.MipLevels;
        desc_.usage = usage;
        desc_.debugName = std::move(debugName);
        SetDebugName(resource_.Get(), desc_.debugName);

        CreateViews();
    }

    Texture::~Texture()
    {
        Release();
    }

    Texture::Texture(Texture&& other) noexcept
    {
        *this = std::move(other);
    }

    Texture& Texture::operator=(Texture&& other) noexcept
    {
        if (this != &other)
        {
            Release();

            device_ = std::exchange(other.device_, nullptr);
            resource_ = std::move(other.resource_);
            desc_ = std::move(other.desc_);
            state_ = other.state_;
            srv_ = std::exchange(other.srv_, {});
            uav_ = std::exchange(other.uav_, {});
            rtv_ = std::exchange(other.rtv_, {});
            dsv_ = std::exchange(other.dsv_, {});
            readOnlyDsv_ = std::exchange(other.readOnlyDsv_, {});
        }

        return *this;
    }

    uint32_t Texture::GetSrvIndex() const
    {
        ATMOS_ASSERT(srv_.IsValid(), "Texture has no ShaderResource usage");
        return srv_.index;
    }

    uint32_t Texture::GetUavIndex() const
    {
        ATMOS_ASSERT(uav_.IsValid(), "Texture has no UnorderedAccess usage");
        return uav_.index;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRtv() const
    {
        ATMOS_ASSERT(rtv_.IsValid(), "Texture has no RenderTarget usage");
        return rtv_.cpu;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDsv(bool readOnly) const
    {
        ATMOS_ASSERT(dsv_.IsValid(), "Texture has no DepthStencil usage");
        return readOnly ? readOnlyDsv_.cpu : dsv_.cpu;
    }

    void Texture::CreateViews()
    {
        ID3D12Device* device = device_->GetNativeDevice();

        if (HasUsage(desc_.usage, TextureUsage::ShaderResource))
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc{};
            viewDesc.Format = GetShaderResourceFormat(desc_.format);
            viewDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            viewDesc.Texture2D.MipLevels = desc_.mipLevels;

            srv_ = device_->GetShaderResourceHeap().Allocate();
            device->CreateShaderResourceView(resource_.Get(), &viewDesc, srv_.cpu);
        }

        if (HasUsage(desc_.usage, TextureUsage::UnorderedAccess))
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc{};
            viewDesc.Format = desc_.format;
            viewDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            uav_ = device_->GetShaderResourceHeap().Allocate();
            device->CreateUnorderedAccessView(resource_.Get(), nullptr, &viewDesc, uav_.cpu);
        }

        if (HasUsage(desc_.usage, TextureUsage::RenderTarget))
        {
            rtv_ = device_->GetRenderTargetHeap().Allocate();
            device->CreateRenderTargetView(resource_.Get(), nullptr, rtv_.cpu);
        }

        if (HasUsage(desc_.usage, TextureUsage::DepthStencil))
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC viewDesc{};
            viewDesc.Format = desc_.format;
            viewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

            dsv_ = device_->GetDepthStencilHeap().Allocate();
            device->CreateDepthStencilView(resource_.Get(), &viewDesc, dsv_.cpu);

            viewDesc.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
            readOnlyDsv_ = device_->GetDepthStencilHeap().Allocate();
            device->CreateDepthStencilView(resource_.Get(), &viewDesc, readOnlyDsv_.cpu);
        }
    }

    void Texture::Release()
    {
        if (device_)
        {
            DeferredReleaseQueue& queue = device_->GetReleaseQueue();
            queue.Release(device_->GetShaderResourceHeap(), srv_);
            queue.Release(device_->GetShaderResourceHeap(), uav_);
            queue.Release(device_->GetRenderTargetHeap(), rtv_);
            queue.Release(device_->GetDepthStencilHeap(), dsv_);
            queue.Release(device_->GetDepthStencilHeap(), readOnlyDsv_);
            queue.Release(std::move(resource_));
        }

        resource_.Reset();
        device_ = nullptr;
    }
}
