#pragma once

#include "graphics/d3d12/D3D12Utils.h"

#include <dxcapi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace atmos
{
    class GraphicsDevice;

    enum class PipelineType
    {
        Graphics,
        Compute,
    };

    enum class BlendMode
    {
        Opaque,
        AlphaBlend,
        Additive,
    };

    enum class CullMode
    {
        None,
        Back,
        Front,
    };

    // Reverse-Z: nearer is greater, so the default depth test is GREATER_EQUAL.
    enum class DepthMode
    {
        Disabled,
        ReadOnly,
        ReadWrite,
    };

    // Defaults describe a fullscreen pass; set only what differs.
    struct GraphicsPipelineDesc
    {
        std::wstring debugName;
        IDxcBlob* vertexShader = nullptr;
        IDxcBlob* pixelShader = nullptr;
        std::vector<DXGI_FORMAT> renderTargetFormats;
        DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
        BlendMode blendMode = BlendMode::Opaque;
        CullMode cullMode = CullMode::None;
        DepthMode depthMode = DepthMode::Disabled;
        D3D12_COMPARISON_FUNC depthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        int32_t depthBias = 0;              // Reverse-Z: negative pushes depth away from the viewer.
        float slopeScaledDepthBias = 0.0f;
        bool depthClip = true;              // Off for directional shadows: casters behind the near plane clamp.
        D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    };

    struct ComputePipelineDesc
    {
        std::wstring debugName;
        IDxcBlob* computeShader = nullptr;
    };

    // A compiled PSO. All pipelines share the device's bindless root signature.
    class PipelineState
    {
    public:
        static PipelineState CreateGraphics(const GraphicsDevice& device, const GraphicsPipelineDesc& desc);
        static PipelineState CreateCompute(const GraphicsDevice& device, const ComputePipelineDesc& desc);

        ID3D12PipelineState* GetNative() const { return pipelineState_.Get(); }
        PipelineType GetType() const { return type_; }
        D3D_PRIMITIVE_TOPOLOGY GetTopology() const { return topology_; }

    private:
        PipelineState(ComPtr<ID3D12PipelineState> pipelineState, PipelineType type,
                      D3D_PRIMITIVE_TOPOLOGY topology);

        ComPtr<ID3D12PipelineState> pipelineState_;
        PipelineType type_;
        D3D_PRIMITIVE_TOPOLOGY topology_;
    };
}
