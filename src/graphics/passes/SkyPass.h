
#pragma once

#include "../GraphicsPipeline.h"

#include <d3d12.h>
#include <filesystem>
#include <memory>

class SkyPass
{
public:
    SkyPass(
        ID3D12Device* device,
        const std::filesystem::path& shaderPath,
        DXGI_FORMAT renderTargetFormat
    );

    void Render(ID3D12GraphicsCommandList* commandList) const;

private:
    std::unique_ptr<GraphicsPipeline> pipeline_;
};
