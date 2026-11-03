#include "SkyPass.h"
#include "../FrameData.h"
#include "../GraphicsConfig.h"
#include "../GraphicsPipeline.h"

#include <array>

SkyPass::SkyPass(ID3D12Device* device, const std::filesystem::path& shaderPath,
                 DXGI_FORMAT renderTargetFormat, int width, int height)
    : constantBuffer_(device, sizeof(SkyConstants), GraphicsConfig::FrameCount),
      width_(width), height_(height)
{
    CreatePipeline(device, shaderPath, renderTargetFormat);
}

SkyPass::~SkyPass() = default;

void SkyPass::CreatePipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                             DXGI_FORMAT renderTargetFormat)
{
    D3D12_ROOT_PARAMETER constantBufferParameter{};
    constantBufferParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    constantBufferParameter.Descriptor.ShaderRegister = 0;
    constantBufferParameter.Descriptor.RegisterSpace = 0;
    constantBufferParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    std::array<D3D12_ROOT_PARAMETER, 1> rootParameters{
        constantBufferParameter
    };

    pipeline_ = std::make_unique<GraphicsPipeline>(
        device, shaderPath, "VSMain", "PSMain", renderTargetFormat, rootParameters
    );
}

void SkyPass::UpdateConstants(UINT frameIndex, const FrameData& frameData)
{
    SkyConstants constants{};

    constants.cameraRight = {
        frameData.cameraRight.x, frameData.cameraRight.y, frameData.cameraRight.z, 0.0f
    };

    constants.cameraUp = {
        frameData.cameraUp.x, frameData.cameraUp.y, frameData.cameraUp.z, 0.0f
    };

    constants.cameraForward = {
        frameData.cameraForward.x, frameData.cameraForward.y, frameData.cameraForward.z, 0.0f
    };

    constants.params = {
        frameData.elapsedSeconds,
        static_cast<float>(width_) / static_cast<float>(height_),
        frameData.tanHalfFovY,
        0.0f
    };

    constantBuffer_.Write(frameIndex, &constants, sizeof(constants));
}

void SkyPass::Record(ID3D12GraphicsCommandList* commandList, UINT frameIndex, const FrameData& frameData)
{
    UpdateConstants(frameIndex, frameData);

    pipeline_->Bind(commandList);
    commandList->SetGraphicsRootConstantBufferView(0, constantBuffer_.GetGPUAddress(frameIndex));
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(3, 1, 0, 0);
}
