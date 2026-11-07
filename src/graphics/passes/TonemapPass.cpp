
#include "TonemapPass.h"
#include "../d3d12/RenderTexture.h"
#include "../d3d12/ShaderCompiler.h"

#include <limits>

namespace
{
    GraphicsPipeline CreateTonemapPipeline(
        ID3D12Device* device,
        const std::filesystem::path& shaderPath,
        DXGI_FORMAT renderTargetFormat)
    {
        const auto vs = ShaderCompiler::Compile(shaderPath, "VSMain", "vs_5_1");
        const auto ps = ShaderCompiler::Compile(shaderPath, "PSMain", "ps_5_1");

        D3D12_DESCRIPTOR_RANGE range{};
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.NumDescriptors = 1;
        range.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER parameter{};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.DescriptorTable.NumDescriptorRanges = 1;
        parameter.DescriptorTable.pDescriptorRanges = &range;
        parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &parameter;
        rootDesc.NumStaticSamplers = 1;
        rootDesc.pStaticSamplers = &sampler;
        rootDesc.Flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
        desc.SampleMask = std::numeric_limits<UINT>::max();
        desc.SampleDesc.Count = 1;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = renderTargetFormat;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        auto& blend = desc.BlendState.RenderTarget[0];
        blend.SrcBlend = D3D12_BLEND_ONE;
        blend.DestBlend = D3D12_BLEND_ZERO;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.LogicOp = D3D12_LOGIC_OP_NOOP;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;

        desc.DepthStencilState.DepthEnable = FALSE;
        desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.DepthStencilState.StencilEnable = FALSE;

        return GraphicsPipeline(device, rootDesc, desc);
    }
}

TonemapPass::TonemapPass(
    ID3D12Device* device,
    const std::filesystem::path& shaderPath,
    DXGI_FORMAT renderTargetFormat)
    : pipeline_(CreateTonemapPipeline(
          device, shaderPath, renderTargetFormat))
{
}

void TonemapPass::Record(
    ID3D12GraphicsCommandList* commandList,
    const RenderTexture& source)
{
    pipeline_.Bind(commandList);

    ID3D12DescriptorHeap* heaps[] = { source.GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetGraphicsRootDescriptorTable(0, source.GetSRV());

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->DrawInstanced(3, 1, 0, 0);
}
