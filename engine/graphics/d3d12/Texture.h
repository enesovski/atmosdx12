#pragma once

#include "graphics/d3d12/D3D12Utils.h"
#include "graphics/d3d12/DescriptorHeap.h"

#include <array>
#include <cstdint>
#include <string>

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
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        TextureUsage usage = TextureUsage::ShaderResource;
        std::array<float, 4> clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        float clearDepth = 0.0f;  // Reverse-Z: 0 is the far plane.
        std::wstring debugName;
    };

    // 2D GPU texture with the views its usage needs and its tracked resource state.
    // Move-only; destroying it frees its descriptors, so the GPU must be done with it.
    class Texture
    {
    public:
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
        uint32_t GetUavIndex() const;

        D3D12_CPU_DESCRIPTOR_HANDLE GetRtv() const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;

    private:
        friend class CommandContext;  // Sole owner of state transitions.

        void CreateViews();
        void Release();

        GraphicsDevice* device_ = nullptr;
        ComPtr<ID3D12Resource> resource_;
        TextureDesc desc_;
        D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;

        Descriptor srv_;
        Descriptor uav_;
        Descriptor rtv_;
        Descriptor dsv_;
    };
}
