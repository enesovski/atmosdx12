
#include "SkyPass.h"

SkyPass::SkyPass(
    ID3D12Device* device,
    const std::filesystem::path& shaderPath,
    DXGI_FORMAT renderTargetFormat
)
{
    pipeline_ = std::make_unique<GraphicsPipeline>(
        device,
        shaderPath,
        "VSMain",
        "PSMain",
        renderTargetFormat
    );
}

void SkyPass::Render(ID3D12GraphicsCommandList* commandList) const
{
    pipeline_->Bind(commandList);

    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    //Draw a triangle for background of the window
    commandList->DrawInstanced(3, 1, 0, 0);
}
