
#pragma once

#include <d3d12.h>
#include <filesystem>

#include "../d3d12/GraphicsPipeline.h"

class RenderTexture;

class TonemapPass
{
public:
    TonemapPass(ID3D12Device* device,
                const std::filesystem::path& shaderPath,
                DXGI_FORMAT renderTargetFormat);

    void Record(ID3D12GraphicsCommandList* commandList,
                const RenderTexture& source);

private:
    GraphicsPipeline pipeline_;
};
