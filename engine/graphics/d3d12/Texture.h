#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    enum class TextureUsage : uint32_t
    {
        None = 0,
        ShaderResource = 1 << 0,
        RenderTarget = 1 << 1,
        DepthStencil = 1 << 2,
        UnorderedAccess = 1 << 3,
    };

    constexpr TextureUsage operator|(TextureUsage a, TextureUsage b)
    {
        return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr bool HasUsage(TextureUsage usage, TextureUsage flag)
    {
        return (static_cast<uint32_t>(usage) & static_cast<uint32_t>(flag)) != 0;
    }

    struct TextureDesc
    {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;  // 6 for cube maps
        bool cube = false;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        TextureUsage usage = TextureUsage::ShaderResource;
        std::array<float, 4> clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        float clearDepth = 0.0f;  // Reverse-Z: 0 is the far plane.
        std::wstring debugName;
    };

    // 2D texture, texture array or cube map with the views its usage needs and a tracked
    // resource state. SRVs cover every mip and slice; UAVs exist per mip, DSVs per slice.
    class Texture
    {
    public:
        static constexpr uint32_t AllSubresources = UINT32_MAX;

        Texture() = default;
        Texture(GraphicsDevice& device, const TextureDesc& desc);

        // Wraps an existing resource, e.g. a swap-chain buffer.
        Texture(GraphicsDevice& device, ComPtr<ID3D12Resource> resource, TextureUsage usage,
                std::wstring debugName, D3D12_RESOURCE_STATES currentState);

        ~Texture();

        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        bool IsValid() const { return resource_ != nullptr; }
        ID3D12Resource* GetResource() const { return resource_.Get(); }
        const TextureDesc& GetDesc() const { return desc_; }
        uint32_t GetWidth() const { return desc_.width; }
        uint32_t GetHeight() const { return desc_.height; }
        D3D12_RESOURCE_STATES GetState() const { return state_; }

        // Bindless indices into ResourceDescriptorHeap.
        uint32_t GetSrvIndex() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return srv_.gpu; }  // e.g. for ImGui::Image
        uint32_t GetUavIndex(uint32_t mip = 0) const;

        D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const;
        // `readOnly` allows depth testing while the texture is in DEPTH_READ (single-slice only).
        D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(uint32_t slice = 0, bool readOnly = false) const;

    private:
        friend class CommandContext;  // Sole owner of state transitions.

        void CreateViews();
        void Release();

        GraphicsDevice* device_ = nullptr;
        ComPtr<ID3D12Resource> resource_;
        TextureDesc desc_;
        D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;

        Descriptor srv_;
        Descriptor rtv_;
        Descriptor readOnlyDsv_;
        std::vector<Descriptor> uavs_;  // per mip
        std::vector<Descriptor> dsvs_;  // per slice
    };
}
