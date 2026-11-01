
#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <filesystem>
#include <span>
#include <string>

class GraphicsPipeline
{
public:
    GraphicsPipeline(ID3D12Device* device, const std::filesystem::path& shaderPath,
                     const std::string& vertexEntry, const std::string& pixelEntry,
                     DXGI_FORMAT renderTargetFormat,
                     std::span<const D3D12_ROOT_PARAMETER> rootParameters = {});

    void Bind(ID3D12GraphicsCommandList* commandList) const;

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
