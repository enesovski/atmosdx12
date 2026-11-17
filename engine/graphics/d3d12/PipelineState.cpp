#include "graphics/d3d12/PipelineState.h"
#include "graphics/d3d12/GraphicsDevice.h"
#include "core/Assert.h"
#include "core/StringUtils.h"

#include <limits>

namespace atmos
{
    namespace
    {
        D3D12_SHADER_BYTECODE ToBytecode(IDxcBlob* blob)
        {
            if (!blob)
                return {};

            return { blob->GetBufferPointer(), blob->GetBufferSize() };
        }

        D3D12_BLEND_DESC MakeBlendDesc(BlendMode mode)
        {
            D3D12_RENDER_TARGET_BLEND_DESC target{};
            target.SrcBlend = D3D12_BLEND_ONE;
            target.DestBlend = D3D12_BLEND_ZERO;
            target.BlendOp = D3D12_BLEND_OP_ADD;
            target.SrcBlendAlpha = D3D12_BLEND_ONE;
            target.DestBlendAlpha = D3D12_BLEND_ZERO;
            target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            target.LogicOp = D3D12_LOGIC_OP_NOOP;
            target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            switch (mode)
            {
                case BlendMode::AlphaBlend:
                    target.BlendEnable = TRUE;
                    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
                    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
                    target.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
                    break;

                case BlendMode::Additive:
                    target.BlendEnable = TRUE;
                    target.DestBlend = D3D12_BLEND_ONE;
                    target.DestBlendAlpha = D3D12_BLEND_ONE;
                    break;

                case BlendMode::Opaque:
                    break;
            }

            D3D12_BLEND_DESC desc{};

            for (D3D12_RENDER_TARGET_BLEND_DESC& renderTarget : desc.RenderTarget)
                renderTarget = target;

            return desc;
        }

        D3D12_RASTERIZER_DESC MakeRasterizerDesc(CullMode mode)
        {
            D3D12_RASTERIZER_DESC desc{};
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            desc.DepthClipEnable = TRUE;

            switch (mode)
            {
                case CullMode::None:  desc.CullMode = D3D12_CULL_MODE_NONE; break;
                case CullMode::Back:  desc.CullMode = D3D12_CULL_MODE_BACK; break;
                case CullMode::Front: desc.CullMode = D3D12_CULL_MODE_FRONT; break;
            }

            return desc;
        }

        D3D12_DEPTH_STENCIL_DESC MakeDepthStencilDesc(DepthMode mode)
        {
            D3D12_DEPTH_STENCIL_DESC desc{};
            desc.DepthEnable = mode != DepthMode::Disabled;
            desc.DepthWriteMask = mode == DepthMode::ReadWrite ? D3D12_DEPTH_WRITE_MASK_ALL
                                                               : D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
            return desc;
        }

        D3D_PRIMITIVE_TOPOLOGY ToTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
        {
            switch (type)
            {
                case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
                case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:  return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
                default:                                  return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            }
        }
    }

    PipelineState::PipelineState(ComPtr<ID3D12PipelineState> pipelineState, PipelineType type,
                                 D3D_PRIMITIVE_TOPOLOGY topology)
        : pipelineState_(std::move(pipelineState)), type_(type), topology_(topology)
    {
    }

    PipelineState PipelineState::CreateGraphics(const GraphicsDevice& device, const GraphicsPipelineDesc& desc)
    {
        ATMOS_ASSERT(desc.vertexShader, "Graphics pipeline needs a vertex shader");
        ATMOS_ASSERT(desc.renderTargetFormats.size() <= D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT,
                     "Too many render targets");

        D3D12_GRAPHICS_PIPELINE_STATE_DESC nativeDesc{};
        nativeDesc.pRootSignature = device.GetBindlessRootSignature();
        nativeDesc.VS = ToBytecode(desc.vertexShader);
        nativeDesc.PS = ToBytecode(desc.pixelShader);
        nativeDesc.BlendState = MakeBlendDesc(desc.blendMode);
        nativeDesc.SampleMask = std::numeric_limits<UINT>::max();
        nativeDesc.RasterizerState = MakeRasterizerDesc(desc.cullMode);
        nativeDesc.DepthStencilState = MakeDepthStencilDesc(desc.depthMode);
        nativeDesc.PrimitiveTopologyType = desc.topology;
        nativeDesc.NumRenderTargets = static_cast<UINT>(desc.renderTargetFormats.size());
        nativeDesc.DSVFormat = desc.depthFormat;
        nativeDesc.SampleDesc.Count = 1;

        for (size_t i = 0; i < desc.renderTargetFormats.size(); ++i)
            nativeDesc.RTVFormats[i] = desc.renderTargetFormats[i];

        ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(device.GetNativeDevice()->CreateGraphicsPipelineState(&nativeDesc,
                                                                            IID_PPV_ARGS(&pipelineState)),
                      "Failed to create graphics pipeline " + ToUtf8(desc.debugName));
        SetDebugName(pipelineState.Get(), desc.debugName);

        return PipelineState(std::move(pipelineState), PipelineType::Graphics, ToTopology(desc.topology));
    }

    PipelineState PipelineState::CreateCompute(const GraphicsDevice& device, const ComputePipelineDesc& desc)
    {
        ATMOS_ASSERT(desc.computeShader, "Compute pipeline needs a compute shader");

        D3D12_COMPUTE_PIPELINE_STATE_DESC nativeDesc{};
        nativeDesc.pRootSignature = device.GetBindlessRootSignature();
        nativeDesc.CS = ToBytecode(desc.computeShader);

        ComPtr<ID3D12PipelineState> pipelineState;
        ThrowIfFailed(device.GetNativeDevice()->CreateComputePipelineState(&nativeDesc,
                                                                           IID_PPV_ARGS(&pipelineState)),
                      "Failed to create compute pipeline " + ToUtf8(desc.debugName));
        SetDebugName(pipelineState.Get(), desc.debugName);

        return PipelineState(std::move(pipelineState), PipelineType::Compute,
                             D3D_PRIMITIVE_TOPOLOGY_UNDEFINED);
    }
}
