#include "SkyPass.h"
#include "../RenderView.h"
#include "../d3d12/GraphicsConfig.h"
#include "../d3d12/ShaderCompiler.h"
#include "../../scene/SkySettings.h"

#include <limits>

namespace
{
    GraphicsPipeline CreateSkyPipeline(ID3D12Device* device,
                                       const std::filesystem::path& shaderPath,
                                       DXGI_FORMAT renderTargetFormat)
    {
        const auto vertexShader = ShaderCompiler::Compile(shaderPath, "VSMain", "vs_5_1");
        const auto pixelShader = ShaderCompiler::Compile(shaderPath, "PSMain", "ps_5_1");

        D3D12_ROOT_PARAMETER constantBufferParameter{};
        constantBufferParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        constantBufferParameter.Descriptor.ShaderRegister = 0;
        constantBufferParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rootDesc{};
        rootDesc.NumParameters = 1;
        rootDesc.pParameters = &constantBufferParameter;
        rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        desc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        desc.SampleMask = std::numeric_limits<UINT>::max();
        desc.SampleDesc.Count = 1;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = renderTargetFormat;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
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

SkyPass::SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
                 DXGI_FORMAT renderTargetFormat)
    : pipeline_(CreateSkyPipeline(device, shaderPath, renderTargetFormat)),
      constantBuffer_(device, sizeof(SkyConstants), GraphicsConfig::FrameCount)
{
}

void SkyPass::Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex,
                     const RenderView& view, const SkySettings& skySettings)
{
    const CameraBasis& camera = view.camera;
    const DirectX::XMFLOAT3 sunDirection = CalculateSunDirection(
        skySettings.sunAzimuthDegrees, skySettings.sunElevationDegrees);

    SkyConstants constants{};
    constants.cameraRight = { camera.right.x, camera.right.y, camera.right.z, 0.0f };
    constants.cameraUp = { camera.up.x, camera.up.y, camera.up.z, 0.0f };
    constants.cameraForward = { camera.forward.x, camera.forward.y, camera.forward.z, 0.0f };
    constants.viewParams = {
        static_cast<float>(view.viewportWidth) / static_cast<float>(view.viewportHeight),
        camera.tanHalfFovY, 0.0f, 0.0f
    };
    constants.sunParams = {
        sunDirection.x, sunDirection.y, sunDirection.z, skySettings.sunIntensity
    };
    constantBuffer_.Write(frameIndex, &constants, sizeof(constants));

    pipeline_.Bind(commandList);
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress(frameIndex));
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
